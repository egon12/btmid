# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run Commands

```bash
# Build debug APK
./gradlew assembleDebug

# Build release APK
./gradlew assembleRelease

# Run unit tests
./gradlew test

# Run a single unit test class
./gradlew :app:testDebugUnitTest --tests "org.egon12.btmid.ExampleUnitTest"

# Run instrumented (device/emulator) tests
./gradlew connectedAndroidTest

# Lint
./gradlew lint

# Clean build
./gradlew clean
```

## Tech Stack

- **Language**: Kotlin + C++ (NDK)
- **UI**: Jetpack Compose with Material3
- **Audio (local)**: Oboe (low-latency C++ audio stream)
- **Audio (network)**: Opus codec + UDP — encodes rendered PCM and streams over WiFi
- **MIDI I/O**: AMidi (NDK) via `MidiManager.openBluetoothDevice`
- **Min/Target SDK**: 36
- **AGP**: 9.1.1, Kotlin: 2.2.10
- **Compose BOM**: 2026.02.01
- **DataStore Preferences**: 1.1.4 — persists drum backend selection
- Dependency versions are centralized in `gradle/libs.versions.toml`

## Project Structure

Single-module Android app (`app/`). Package root: `org.egon12.btmid`.

- `app/src/main/java/org/egon12/btmid/` — Kotlin app source
- `app/src/main/cpp/` — C++ native engine
- `app/src/main/java/org/egon12/btmid/ui/theme/` — Compose theme (`Color.kt`, `Theme.kt`, `Type.kt`)
- `app/src/test/` — JUnit unit tests
- `app/src/androidTest/` — instrumented tests (Espresso + Compose UI test)

`MainActivity` uses `ComponentActivity` + `setContent` with `BtmidTheme` wrapping a `Scaffold`. New screens/composables should follow the same pattern: `@Composable` functions in their own files, previewed with `@Preview`.

---

## Feature: Bluetooth MIDI Receiver with Piano + Drums

The app receives BLE MIDI and synthesizes audio in C++ via Oboe. All audio processing and MIDI parsing runs natively; Kotlin handles UI, BLE/MIDI connection lifecycle, and OGG asset decoding.

- MIDI channel 1 (index `0`) → Piano (C++ additive sine synthesis)
- MIDI channel 10 (index `9`) → Drums (three selectable C++ backends: Noise, FM, Samples)

### File Structure

```
app/src/main/java/org/egon12/btmid/
  bluetooth/
    BleScanner.kt            — BluetoothLeScanner filtered by MIDI service UUID
    BleMidiConnection.kt     — MidiManager.openBluetoothDevice; passes MidiDevice to NativeAudioEngine via AMidi

  midi/
    MidiEvent.kt             — sealed class: NoteOn, NoteOff, ControlChange (used for UI event log)
    MidiRouter.kt            — NativeAudioEngine.MidiEventListener; receives parsed events from C++ dispatch thread, emits to SharedFlow for UI

  synth/
    NativeAudioEngine.kt     — Kotlin singleton; loads libbtmid.so, exposes JNI bridge (noteOn/Off, loadSample, setDrumBackend, setOutputPort)
    SampleBank.kt            — decodes OGG assets → FloatArray per drum name via MediaCodec; calls NativeAudioEngine.loadSample()

  ui/
    MainScreen.kt            — top-level Composable (permission banner, scan, drum selector, piano, drum pads, device list)
    DeviceListItem.kt        — single discovered-device row
    MidiActivityIndicator.kt — animated dot that flashes on each MIDI event
    DrumEngineSelector.kt    — FilterChip row to switch Noise / FM Synth / Samples at runtime
    PianoKeyboard.kt         — one-octave Canvas keyboard (C4–B4, MIDI 60–71); multi-touch via pointerInput → NativeAudioEngine ch 0
    DrumTrigger.kt           — 4×2 grid of drum pads; press/release → NativeAudioEngine ch 9

  DrumBackendStore.kt        — DataStore<Preferences> wrapper; persists selected DrumBackend
  MainViewModel.kt           — UiState + scan/connect/disconnect/setDrumBackend actions

app/src/main/cpp/
  AudioGraph.h/.cpp           — top-level owner; holds unique_ptr<AudioEngine> + InstrumentRepository; single JNI entry point
  AudioEngine.h               — pure-virtual interface for all audio engines; defines MidiEvt struct
  OboeEngine.h/.cpp           — AudioEngine impl: Oboe stream + AMidi poll loop + SpscRing; routes MIDI via mChannels[16]
  WifiEngine.h/.cpp           — AudioEngine impl: renders instruments, encodes with Opus, streams over UDP
  InstrumentRepository.h/.cpp — owns all concrete instruments (lazy-created by string id); only file that includes concrete instrument headers
  Instrument.h                — pure-virtual interface: noteOn/Off/render(float*, int32_t)
  MidiParser.h/.cpp           — parseMidi(): raw MIDI 1.0 bytes (with running-status) → MidiMsg structs
  SpscRing.h                  — lock-free single-producer single-consumer ring buffer (audio→dispatch)
  jni_bridge.cpp              — all Java_… extern "C" functions; bridges NativeAudioEngine.kt → AudioGraph
  opus/                       — Opus codec source (built via add_subdirectory; linked into libbtmid.so)
  instruments/
    Piano.h/.cpp              — additive sine synthesis, ADSR, 8-voice polyphony
    NoiseDrum.h/.cpp          — noise-burst synthesis per GM drum note
    FmDrum.h/.cpp             — FM operator synthesis per GM drum note
    SampleDrum.h/.cpp         — plays back FloatArray samples loaded from SampleBank
    PianoSinTable.h/.cpp      — sin-table variant of Piano (lookup-table oscillator)
```

Sample assets: `app/src/main/assets/samples/drums/` — `kick.ogg`, `snare.ogg`, `closed_hat.ogg`, `open_hat.ogg`, `crash.ogg`, `ride.ogg`, `tom.ogg` (all toms share one file). Declared `noCompress` in `build.gradle.kts` so MediaExtractor can seek.

### Data Flow

```
BLE MIDI device
  → MidiManager.openBluetoothDevice(bluetoothDevice)
  → BleMidiConnection passes MidiDevice to NativeAudioEngine.setOutputPort()
  → jni_bridge → AudioGraph::openMidiDevice() → AudioEngine::setOutputPort()

OboeEngine path (default — local speaker output):
  → OboeEngine::onAudioReady() (Oboe audio thread)
      polls AMidiOutputPort_receive()
      → parseMidi() → MidiMsg
      → routes via mChannels[channel] → Instrument::noteOn/Off/CC
      → pushes MidiEvt to SpscRing (lock-free, non-blocking)
      → renders each unique instrument in mChannels → Oboe float output
  → OboeEngine::dispatchLoop() (dedicated thread)
      drains SpscRing → JNI callback → MidiRouter.onMidiEvent()
      → SharedFlow → MainViewModel → UI event log + activity pulse

WifiEngine path (network output):
  → WifiEngine::udpRenderLoop() (dedicated thread, 10 ms cadence)
      polls AMidiOutputPort_receive()
      → parseMidi() → MidiMsg
      → routes via mChannels[channel] → Instrument::noteOn/Off/CC
      → renders each unique instrument in mChannels → float buf
      → opus_encode_float() → sendto() UDP socket

On-screen input (no BLE device needed)
  PianoKeyboard (ch 0) ──┐
  DrumTrigger   (ch 9) ──┴→ NativeAudioEngine.noteOn/Off() → jni_bridge → AudioGraph → AudioEngine
```

### Permissions (AndroidManifest.xml)

```xml
<uses-permission android:name="android.permission.BLUETOOTH_SCAN"
    android:usesPermissionFlags="neverForLocation" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
<uses-feature android:name="android.hardware.bluetooth_le" android:required="true" />
<uses-feature android:name="android.software.midi" android:required="true" />
```

`neverForLocation` avoids requiring `ACCESS_FINE_LOCATION` on API 31+.

### C++ Architecture

Three-layer design. `AudioGraph` is the single object exposed to JNI.

```
AudioGraph
├── AudioEngine (unique_ptr) — audio I/O; knows only Instrument interface
│   ├── OboeEngine           — Oboe stream → local speaker
│   └── WifiEngine           — UDP + Opus → network receiver
└── InstrumentRepository     — owns concrete instruments; lazy-creates by string id
```

#### AudioEngine (interface — `AudioEngine.h`)

Pure-virtual base for all audio engines. Also defines the `MidiEvt` struct.

| Method | Pure virtual | Notes |
|--------|-------------|-------|
| `start()` / `stop()` | ✓ | lifecycle |
| `noteOn` / `noteOff` / `controlChange` | ✓ / ✓ / no-op | MIDI routing |
| `setInstrument(channel, Instrument*)` | no-op | wired by `InstrumentRepository` |
| `loadSample(id, data, len)` | no-op | only used by `InstrumentRepository` path |
| `setDrumBackend(id)` | no-op | only used by `InstrumentRepository` path |
| `setOutputPort` / `clearOutputPort` | ✓ | BLE MIDI device binding |

#### AudioGraph

- Single JNI entry point (`jni_bridge.cpp` holds one `AudioGraph*`)
- Wires up default instruments in constructor: piano on ch 0, noise_drum on ch 9
- Owns `InstrumentRepository` (declared first) then `unique_ptr<AudioEngine>` (declared second) — C++ reverse-destruction order ensures engine stops before instruments are freed
- `setEngine(unique_ptr<AudioEngine>)` — stops current engine, swaps to new one; caller configures and starts the new engine
- `openMidiDevice` / `closeMidiDevice` delegate to `AudioEngine::setOutputPort` / `clearOutputPort`
- `setInstrument(channel, id)` → repository lazy-creates instrument → `AudioEngine::setInstrument`
- `loadDrumSample` → forwards to both `InstrumentRepository` (for OboeEngine's SampleDrum) and `AudioEngine::loadSample` (no-op on OboeEngine)

#### InstrumentRepository

- The **only** file that includes concrete instrument headers (`Piano.h`, `NoiseDrum.h`, `FmDrum.h`, `SampleDrum.h`)
- Lazy-creates instruments on first `setInstrument` call for a given id
- Calls `AudioEngine::setInstrument(channel, ptr)` — works for both `OboeEngine` (stores in `mChannels`) and `WifiEngine` (same)
- Holds `SampleDrum*` for `loadDrumSample`; all other instruments accessed via `Instrument*`
- Known ids: `"piano"`, `"noise_drum"`, `"fm_drum"`, `"sample_drum"`

#### OboeEngine

- **Audio**: Oboe stream, `PERFORMANCE_MODE_LOW_LATENCY`, mono float 44100 Hz
- **Routing**: `std::atomic<Instrument*> mChannels[16]`; `setInstrument(channel, ptr)` is an atomic store safe to call from any thread
- **MIDI poll**: `AMidiOutputPort_receive()` called at the top of each `onAudioReady()` callback; routes via `mChannels[channel]`
- **Render**: iterates `mChannels`, deduplicates by pointer, calls `render()` on each unique instrument
- **Lock-free bridge**: audio thread pushes `MidiEvt` into `SpscRing<MidiEvt, 256>`; `dispatchLoop()` thread pops and fires JNI callback for UI
- Knows nothing about concrete instrument types

#### WifiEngine

- **Transport**: UDP socket + Opus encoder; destination `host:port` set at construction time
- **Routing**: same `std::atomic<Instrument*> mChannels[16]` pattern as `OboeEngine`; wired by `InstrumentRepository`
- **Render loop**: dedicated thread (`udpRenderLoop`), 10 ms cadence (120 frames @ 48 kHz)
  - polls AMidi, renders unique instruments into a float buffer
  - skips UDP send if buffer is all-zero (silence suppression)
  - encodes with `opus_encode_float` (64 kbps, complexity 0, restricted low-delay) → `sendto`
- **Start/stop**: `start()` opens encoder + socket + spawns thread; `stop()` joins thread + closes socket + destroys encoder
- No instrument ownership — instruments are owned by `InstrumentRepository` and wired via `setInstrument`

### Piano (`Piano`)

- Frequency: `440.0 * pow(2.0, (note - 69) / 12.0)`
- 5 harmonics with amplitudes: 1.00, 0.50, 0.25, 0.12, 0.06
- ADSR: attack 5ms, decay 80ms, sustain 60% of peak, release 300ms exponential
- Voice cap: 8 simultaneous voices (steal oldest on overflow)

### Drum Engine

Three C++ backends selectable at runtime. Switching calls `AudioGraph::setInstrument(9, id)` which atomically swaps `mChannels[9]` in the active `AudioEngine`.

#### NoiseDrum

| GM Note | Sound | Method |
|---------|-------|--------|
| 35/36 | Bass Drum | 60 Hz sine, 150ms decay |
| 38/40 | Snare | white noise + 200 Hz tone, 100ms |
| 42 | Closed Hat | difference-filtered noise (`y[n]=x[n]-x[n-1]`), 50ms |
| 46 | Open Hat | white noise, 300ms |
| 49/57 | Crash | white noise, 800ms |
| 51 | Ride | white noise + 600 Hz, 400ms |

#### FmDrum

| GM Note | Sound | Method |
|---------|-------|--------|
| 35/36 | Bass Drum | self-FM sine sweep 50→30 Hz, mod index 4→0, 180ms |
| 38/40 | Snare | 2-op FM (200 Hz carrier, 180 Hz mod, index 2) + noise, 120ms |
| 42 | Closed Hat | √2-ratio 2-op FM at 8 kHz, mod index 8, 45ms |
| 46 | Open Hat | same as closed hat, 350ms |
| 49/57 | Crash | 4-op FM chain + noise, random phases per hit, 900ms |
| 51 | Ride | 3-op FM at 600 Hz with inharmonic upper ops (index 1.5), 500ms |
| 41/43/45/47/48/50 | Toms | self-FM sine sweep startFreq→½, mod index 3→0, 150ms |

FM formula: `out = amp × env(t) × sin(2π·fC·t + depth × mod)` where `mod = sin(2π·fM·t + index × sin(2π·fM·t))`.

#### SampleDrum

Plays back pre-decoded OGG assets loaded by `SampleBank`. `SampleBank.load()` runs on `Dispatchers.IO` at startup; decoded `FloatArray`s are passed to `NativeAudioEngine.loadSample()` → JNI → `AudioGraph::loadDrumSample()` → `InstrumentRepository::loadDrumSample()` → `SampleDrum::loadSample()`. The "Samples" chip in the UI is disabled with a loading spinner until loading completes.

#### Backend Selector UI

`DrumEngineSelector` is a `FilterChip` row embedded in `MainScreen`:

```
[ Noise ]  [ FM Synth ]  [ Samples ]
```

The selected backend is persisted across restarts via `DrumBackendStore` (DataStore Preferences key `"drum_backend"`). `setDrumBackend(ordinal)` maps in `jni_bridge.cpp`: 0 → `"noise_drum"`, 1 → `"fm_drum"`, 2 → `"sample_drum"`, then calls `AudioGraph::setInstrument(9, id)`.

### On-Screen Instruments

#### PianoKeyboard (`ui/PianoKeyboard.kt`)

One-octave Canvas keyboard, C4–B4 (MIDI notes 60–71). Handles multi-touch via `Modifier.pointerInput` + `awaitPointerEventScope`. Tracks a `mutableStateMapOf<PointerId, Int>()` (pointer → active note). Hit-test checks black keys first (higher Z-order), then white keys. Calls `NativeAudioEngine.noteOn(0, note, 100)` on press and `noteOff(0, note)` on release/slide-off.

#### DrumTrigger (`ui/DrumTrigger.kt`)

4×2 grid of rounded drum pads. Each pad uses `awaitEachGesture` + `awaitFirstDown` / `waitForUpOrCancellation`. On press: `noteOn(9, note, 100)`. On release or cancel: `noteOff(9, note)`.

```
[ Closed Hat ] [ Open Hat ] [ Ride ] [ Crash ]
[    Kick    ] [   Snare  ] [Lo Tom] [ Hi Tom ]
```

### ViewModel State

```kotlin
enum class DrumBackend { Noise, Fm, Samples }

data class UiState(
    val permissionsGranted: Boolean = false,
    val connectionStatus: ConnectionStatus = ConnectionStatus.Idle,
    val discoveredDevices: List<DeviceUiState> = emptyList(),
    val connectedDeviceAddress: String? = null,
    val recentEvents: List<MidiEventUiModel> = emptyList(),  // capped at 10
    val midiActivityPulse: Boolean = false,
    val drumBackend: DrumBackend = DrumBackend.Noise,
    val samplesLoaded: Boolean = false,
)
```

### Threading

- BLE scan callbacks → main thread (`Handler(mainLooper)`)
- **OboeEngine**: Oboe audio thread → `OboeEngine::onAudioReady()` → AMidi poll → parse → routes via `mChannels[channel]` → render; pushes `MidiEvt` to `SpscRing` (non-blocking)
- **OboeEngine**: `dispatchLoop()` thread → drains `SpscRing` → JNI `onMidiEvent()` → `MidiRouter` → `SharedFlow.tryEmit()` → ViewModel
- **WifiEngine**: `udpRenderLoop()` thread (10 ms timer) → AMidi poll → parse → render → Opus encode → UDP send
- `NativeAudioEngine.noteOn/Off()` (called from UI thread) → JNI → `AudioGraph` → `AudioEngine` → instrument (lock-free via `mChannels`)
- `AudioEngine::mChannels[16]` are `std::atomic<Instrument*>` — UI thread writes via `setInstrument`, audio/render thread reads safely

### Cleanup (`MainViewModel.onCleared`)

```
bleScanner.stopScan()
bleMidiConnection.disconnect()   // closeMidiDevice() + closes MidiDevice
NativeAudioEngine.stop()         // stops engine (Oboe stream or UDP thread)
// AudioGraph destructor: stops AudioEngine, then InstrumentRepository is freed
```
