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
./gradlew :app:testDebugUnitTest --tests "org.gilbertxenodike.btmid.ExampleUnitTest"

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
- **Audio (local)**: Oboe 1.9.3 (low-latency C++ audio stream)
- **Audio (network)**: Opus codec + UDP — encodes rendered PCM and streams over WiFi
- **MIDI I/O**: AMidi (NDK) via `MidiManager.openBluetoothDevice`
- **Min/Target SDK**: 36
- **AGP**: 9.1.1, Kotlin: 2.2.10, NDK: 28.2.13676358
- **Compose BOM**: 2026.02.01
- **DataStore Preferences**: 1.1.4 — persists drum backend, keyboard type, and waveform selection
- Dependency versions are centralized in `gradle/libs.versions.toml`

## Project Structure

Single-module Android app (`app/`). Package root: `org.gilbertxenodike.btmid`.

- `app/src/main/java/org/gilbertxenodike/btmid/` — Kotlin app source
- `app/src/main/cpp/` — C++ native engine
- `app/src/main/java/org/gilbertxenodike/btmid/ui/theme/` — Compose theme (`Color.kt`, `Theme.kt`, `Type.kt`)
- `app/src/test/` — JUnit unit tests
- `app/src/androidTest/` — instrumented tests (Espresso + Compose UI test)

`MainActivity` uses `ComponentActivity` + `setContent` with `BtmidTheme` wrapping a `Scaffold`. New screens/composables should follow the same pattern: `@Composable` functions in their own files, previewed with `@Preview`.

---

## Feature: Bluetooth MIDI Receiver with Piano + Synths + Drums

The app receives BLE MIDI and synthesizes audio in C++ via Oboe. All audio processing and MIDI parsing runs natively; Kotlin handles UI, BLE/MIDI connection lifecycle, and OGG asset decoding.

- MIDI channel 1 (index `0`) → Keyboard instruments (Piano, PolySynth, or MonoSynth with selectable waveforms)
- MIDI channel 10 (index `9`) → Drums (three selectable C++ backends: Noise, FM, Samples)
- Built-in MIDI loop recorder with overdub support

### File Structure

```
app/src/main/java/org/gilbertxenodike/btmid/
  bluetooth/
    BleScanner.kt            — BluetoothLeScanner filtered by MIDI service UUID
    BleMidiConnection.kt     — MidiManager.openBluetoothDevice; passes MidiDevice to NativeAudioEngine via AMidi

  midi/
    MidiEvent.kt             — sealed class: NoteOn, NoteOff, ControlChange (used for UI event log)
    MidiRouter.kt            — NativeAudioEngine.MidiEventListener; receives parsed events from C++ dispatch thread, emits to SharedFlow for UI; also emits loop state events

  synth/
    NativeAudioEngine.kt     — Kotlin singleton; loads libbtmid.so, exposes JNI bridge (noteOn/Off, loadSample, setDrumBackend, setInstrument, setOutput, loop controls, benchmarkPianos)
    SampleBank.kt            — decodes OGG assets → FloatArray per drum name via MediaCodec; resamples to 48 kHz; calls NativeAudioEngine.loadSample()

  ui/
    MainScreen.kt            — top-level Composable (permission banner, scan, drum selector, piano, drum pads, device list, loop controls, keyboard sound selector, waveform selector, sustain button)
    DeviceListItem.kt        — single discovered-device row
    MidiActivityIndicator.kt — animated dot that flashes on each MIDI event
    DrumEngineSelector.kt    — FilterChip row to switch Noise / FM Synth / Samples at runtime
    KeyboardSoundSelector.kt — FilterChip row to switch Piano / Poly / Mono keyboard types
    WaveformSelector.kt      — FilterChip row to switch Sine / Saw / Square waveforms (visible only for Poly/Mono)
    EngineSelector.kt        — ModalBottomSheet for choosing Oboe (local) or WiFi (UDP) audio output
    LoopControls.kt          — REC/STOP/CLEAR buttons + time-signature button (opens dialog) for MIDI loop recorder
    PianoKeyboard.kt         — one-octave Canvas keyboard (C4–B4, MIDI 60–71); multi-touch via pointerInput → NativeAudioEngine ch 0
    DrumTrigger.kt           — 4×2 grid of drum pads; press/release → NativeAudioEngine ch 9
    modifier/
      Blink.kt               — Modifier extension for infinite alpha blink animation (used by LoopControls)

  DrumBackendStore.kt        — DataStore<Preferences> wrapper; persists selected DrumBackend
  KeyboardTypeStore.kt       — DataStore<Preferences> wrapper; persists selected KeyboardType
  WaveformStore.kt           — DataStore<Preferences> wrapper; persists selected SynthWaveform
  MainViewModel.kt           — UiState + scan/connect/disconnect/setDrumBackend/setKeyboardType/setWaveform/selectEngine/loop actions

app/src/main/cpp/
  AudioConfig.h               — kSampleRate = 48000 constant used project-wide
  AudioGraph.h/.cpp           — top-level owner; holds InstrumentRepository + UICallback + shared_ptr<MidiEngine> + OboeOutput + WifiOutput; single JNI entry point
  MidiEngine.h/.cpp           — concrete class: channel routing (mChannels[16]), MIDI poll, loop advancement, instrument rendering; owns LoopRecorder
  MidiEvt.h                   — MidiEvt struct (channel, type, data1, data2); channel 0xFF overloaded for loop state
  UICallback.h/.cpp           — standalone dispatch thread: drains SpscRing → JNI callback → Kotlin MidiRouter; handles both MIDI events and loop state
  LoopRecorder.h/.cpp         — MIDI loop recorder with overdub; state machine (Idle/Recording/Playing/Armed/Overdubbing); triggered by CC#95/CC#93 or JNI
  PianoBenchmark.h/.cpp       — benchmarks Piano vs PianoSinTable (1-second render × 10)
  InstrumentRepository.h/.cpp — owns all concrete instruments (lazy-created by string id); only file that includes concrete instrument headers
  Instrument.h                — pure-virtual interface: noteOn/Off/render; controlChange has default no-op
  MidiParser.h/.cpp           — parseMidi(): raw MIDI 1.0 bytes (with running-status) → MidiMsg structs
  SpscRing.h                  — lock-free single-producer single-consumer ring buffer
  jni_bridge.cpp              — all Java_… extern "C" functions; bridges NativeAudioEngine.kt → AudioGraph
  opus/                       — Opus codec source (built via add_subdirectory; linked into libbtmid.so)
  outputs/
    OboeOutput.h/.cpp         — thin Oboe wrapper; onAudioReady calls MidiEngine::render()
    WifiOutput.h/.cpp         — UDP + Opus transport; render loop calls MidiEngine::render()
  instruments/
    Piano.h/.cpp              — additive sine synthesis, ADSR, 8-voice polyphony, sustain pedal (CC64), queue-based thread safety
    PianoSinTable.h/.cpp      — sin-table variant of Piano (1024-entry lookup table with linear interpolation)
    PolyOscillator.h/.cpp     — polyphonic oscillator (Sine/Saw/Square), 8-voice, ADSR, sustain pedal, queue-based thread safety
    MonoOscillator.h/.cpp     — monophonic oscillator (Sine/Saw/Square) with portamento glide, CC#74 for portamento time, queue-based thread safety
    WaveTable.h/.cpp          — static utility: 1024-entry sin table, naive saw/square generators (not currently used by instruments)
    NoiseDrum.h/.cpp          — noise-burst synthesis per GM drum note, 16-voice, queue-based thread safety
    FmDrum.h/.cpp             — FM operator synthesis per GM drum note, 16-voice, queue-based thread safety
    SampleDrum.h/.cpp         — plays back FloatArray samples loaded from SampleBank, 16-voice, atomic sample loading, queue-based thread safety
```

Sample assets: `app/src/main/assets/samples/drums/` — `kick.ogg`, `snare.ogg`, `closed_hat.ogg`, `open_hat.ogg`, `crash.ogg`, `ride.ogg`, `tom.ogg` (all toms share one file). Declared `noCompress` in `build.gradle.kts` so MediaExtractor can seek.

### Data Flow

```
BLE MIDI device
  → MidiManager.openBluetoothDevice(bluetoothDevice)
  → BleMidiConnection passes MidiDevice to NativeAudioEngine.openMidiDevice()
  → jni_bridge → AudioGraph::openMidiDevice() → MidiEngine::openMidiDevice()

OboeOutput path (default — local speaker output):
  → OboeOutput::onAudioReady() (Oboe audio thread)
      → MidiEngine::render(buf, frames)
          → pollMidi(): AMidiOutputPort_receive() → parseMidi() → route via mChannels[channel] → Instrument::noteOn/Off/CC
          → advanceLoop(): LoopRecorder.advance() fires playback MIDI into mChannels
          → renderAudio(): deduplicate instruments by pointer → Instrument::render()
      → pushes MidiEvt to UICallback's SpscRing (lock-free, non-blocking)
  → UICallback dispatch thread
      drains SpscRing → JNI callback → MidiRouter.onMidiEvent() / onLoopState()
      → SharedFlow → MainViewModel → UI event log + activity pulse + loop state

WifiOutput path (network output):
  → WifiOutput::udpRenderLoop() (dedicated thread, ~2.5 ms cadence)
      → MidiEngine::render(buf, frames) [same as above]
      → skips UDP send if buffer is all-zero (silence suppression)
      → opus_encode_float() → sendto() UDP socket

On-screen input (no BLE device needed)
  PianoKeyboard (ch 0) ──┐
  DrumTrigger   (ch 9) ──┴→ NativeAudioEngine.noteOn/Off() → jni_bridge → AudioGraph → MidiEngine
```

### Permissions (AndroidManifest.xml)

```xml
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.BLUETOOTH_SCAN"
    android:usesPermissionFlags="neverForLocation" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
<uses-feature android:name="android.hardware.bluetooth_le" android:required="true" />
<uses-feature android:name="android.software.midi" android:required="true" />
```

`neverForLocation` avoids requiring `ACCESS_FINE_LOCATION` on API 31+.

### C++ Architecture

```
AudioGraph
├── InstrumentRepository          — owns concrete instruments; lazy-creates by string id
├── UICallback (unique_ptr)       — dispatch thread: SpscRing → JNI → Kotlin
├── MidiEngine (shared_ptr)       — unified MIDI processing + rendering
│   ├── mChannels[16]             — std::atomic<Instrument*> routing table
│   ├── loopRecorder              — MIDI loop recorder with overdub
│   ├── pollMidi()                — AMidi poll + parse + route
│   ├── advanceLoop()             — loop playback
│   └── renderAudio()             — deduplicate + render instruments
├── OboeOutput (unique_ptr)       — thin Oboe wrapper → MidiEngine::render()
└── WifiOutput (unique_ptr)       — UDP + Opus → MidiEngine::render()
```

#### AudioGraph

- Single JNI entry point (`jni_bridge.cpp` holds one `AudioGraph*`)
- Wires up default instruments in constructor: piano on ch 0, noise_drum on ch 9
- Owns `InstrumentRepository` (declared first), then `UICallback`, then `MidiEngine` (shared_ptr), then outputs — C++ reverse-destruction order ensures outputs stop before engine before instruments are freed
- `setOutput(engineId, host, port)` — stops both outputs, clears MIDI port, stops UICallback, creates brand-new `MidiEngine`, reconnects UICallback, creates appropriate output (1=Oboe, 2=Wifi), re-wires default instruments
- `openMidiDevice` / `closeMidiDevice` delegate to `MidiEngine::openMidiDevice` / `closeMidiDevice`
- `setInstrument(channel, id)` → repository lazy-creates instrument → `MidiEngine::setInstrument`
- `loadDrumSample` → forwards to `InstrumentRepository` (for SampleDrum)
- `loopRecord` / `loopPlay` / `loopClear` / `loopState` → delegate to `MidiEngine::loopRecorder`

#### MidiEngine (concrete class — `MidiEngine.h/.cpp`)

Replaces the old `AudioEngine` pure-virtual interface. All MIDI processing, loop playback, and instrument rendering logic lives here.

| Method | Notes |
|--------|-------|
| `render(float* buf, int32_t frames)` | unified entry point: pollMidi → advanceLoop → renderAudio |
| `setInstrument(channel, Instrument*)` | atomic store into `mChannels[channel]` |
| `noteOn` / `noteOff` / `controlChange` | route to instrument + push to `loopRecorder.onUiMidiEvent()` |
| `openMidiDevice` / `closeMidiDevice` | AMidi device binding + UICallback start/stop |
| `pollMidi()` (protected) | drains AMidi, parses, routes, pushes events to loopRecorder + UICallback |
| `advanceLoop(int32_t frames)` (protected) | calls `loopRecorder.advance()`, fires playback MIDI into `mChannels` |
| `renderAudio(float* buf, int32_t frames)` (protected) | deduplicates instruments by pointer, calls `render()` on each unique one |

- `std::atomic<Instrument*> mChannels[16]` — channel routing table, safe for concurrent access
- `LoopRecorder loopRecorder` — embedded directly; manages MIDI recording/playback

#### InstrumentRepository

- The **only** file that includes concrete instrument headers (`Piano.h`, `PianoSinTable.h`, `PolyOscillator.h`, `MonoOscillator.h`, `NoiseDrum.h`, `FmDrum.h`, `SampleDrum.h`)
- Lazy-creates instruments on first `setInstrument` call for a given id
- Calls `MidiEngine::setInstrument(channel, ptr)`
- Eagerly creates `SampleDrum` in constructor so `loadDrumSample()` always has a target
- Known ids: `"piano"`, `"sine_polysynth"`, `"saw_polysynth"`, `"square_polysynth"`, `"sine_monosynth"`, `"saw_monosynth"`, `"square_monosynth"`, `"noise_drum"`, `"fm_drum"`, `"sample_drum"`

#### UICallback

- Standalone dispatch thread replacing the old dispatch-loop-in-OboeEngine pattern
- `onMidiEvent(MidiEvt)` pushes into `SpscRing<MidiEvt, 256>` (called from audio thread)
- Dispatch thread drains ring; if `channel == 0xFF`, calls `onLoopState(type)`; otherwise calls `onMidiEvent(channel, type, data1, data2)` via JNI
- Sleeps 1ms when idle

#### LoopRecorder

- State machine: `Idle(0) → Recording(1) → Playing(2)`, with `Armed(3)` and `Overdubbing(4)` states
- `rec()`: Idle→Armed, or Playing→Overdubbing
- `play()`: Recording→Playing (maps timestamps to frames), or Overdubbing→Playing (merges overdub events sorted by frame)
- `advance(int32_t frames, fire_fn)`: called from audio thread; plays back events whose frame <= current position
- `onMidiEvent(MidiMsg, timestamp)`: called from MIDI poll; handles CC#95 (start/overdub) and CC#93 (stop)
- `onUiMidiEvent(MidiMsg)`: called from UI noteOn/Off; records with `clock_gettime` timestamps
- `onStateChange` callback fires `MidiEvt{0xFF, state, 0, 0}` into event queue for UI notification
- Lock-free playback: `mPlayEventsPtr` is `shared_ptr<vector<FrameMidiMsg>>` atomically swapped for audio thread read

#### OboeOutput

- Thin wrapper around Oboe stream; `onAudioReady()` zeroes buffer and calls `mEngine->render(buf, numFrames)`
- Oboe stream config: Output, `PERFORMANCE_MODE_LOW_LATENCY`, Exclusive, Float, Mono, 48000 Hz
- No longer owns MIDI poll or dispatch logic — all delegated to `MidiEngine`

#### WifiOutput

- UDP socket + Opus encoder; destination `host:port` set at construction time
- Render loop: dedicated thread, 120 frames (2.5 ms at 48 kHz) per iteration using `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME)` for precise timing
- Calls `mEngine->render()`, skips UDP send if buffer is all-zero (silence suppression)
- Opus encode at 64 kbps, complexity 0, restricted low-delay
- `start()` opens encoder + socket + spawns thread; `stop()` joins thread + closes socket + destroys encoder

### Instruments

All instruments implement `Instrument` interface: `noteOn(note, velocity)`, `noteOff(note)`, `controlChange(channel, cc, value)`, `render(float* buf, int32_t frames)`. All use `SpscRing` queues for thread-safe noteOn/noteOff/CC from UI thread, drained at the top of `render()`.

#### Piano (`Piano`)

- Frequency: `440.0 * pow(2.0, (note - 69) / 12.0)`
- 5 harmonics with amplitudes: 1.00, 0.50, 0.25, 0.12, 0.06
- ADSR: attack 5ms (240 samples), decay 80ms (3840 samples), sustain 60% of peak, release 300ms exponential
- Voice cap: 8 simultaneous voices (steal oldest on overflow); retrigger support for same note
- Velocity curve: `pow(vel/127, 1.5) * 0.7`
- Sustain pedal: CC64 >= 64 engages sustain; `mSustainedNotes[128]` tracks held notes; releases all when pedal lifted

#### PianoSinTable

- 1024-entry sin table with linear interpolation (22-bit fractional phase)
- Same ADSR and voice structure as Piano
- Queue-based thread safety (same as Piano)

#### PolyOscillator

- Polyphonic oscillator with waveform selection (Sine/Saw/Square)
- 8-voice polyphony with oldest-voice stealing; retrigger support
- ADSR: attack 5ms (240 samples), decay 80ms (3840 samples), sustain 60%, release 300ms
- Velocity curve: `pow(vel/127, 1.5) * 0.7`
- Waveform normalization: Sine=1.0, Saw=1.225, Square=0.707
- Sustain pedal support (CC64)
- Phase accumulator (0..1) for oscillator
- Known ids: `"sine_polysynth"`, `"saw_polysynth"`, `"square_polysynth"`

#### MonoOscillator

- Monophonic oscillator with portamento (glide)
- Single voice; portamento tau default 0.15s (CC#74 maps 0-127 to 5ms-400ms)
- ADSR: attack 10ms (480 samples), decay 50ms (2400 samples), sustain 70%, release 200ms
- Phase: Idle → Attack → Decay → Sustain → Release
- Idle→Attack on noteOn (full restart); Release→Attack (re-trigger from zero gain); otherwise glides to new frequency
- One-pole frequency smoothing: `mCurrentFreq = mTargetFreq + (mCurrentFreq - mTargetFreq) * mPortamentoCoeff`
- noteOff only releases if note matches last noteOn
- Known ids: `"sine_monosynth"`, `"saw_monosynth"`, `"square_monosynth"`

### Drum Engine

Three C++ backends selectable at runtime. Switching calls `AudioGraph::setInstrument(9, id)` which atomically swaps `mChannels[9]` in the active `MidiEngine`.

#### NoiseDrum

| GM Note | Sound | Method |
|---------|-------|--------|
| 35/36 | Bass Drum | 60 Hz sine, 150ms decay |
| 38/40 | Snare | white noise + 200 Hz tone, 100ms |
| 42 | Closed Hat | difference-filtered noise (`y[n]=x[n]-x[n-1]`), 50ms |
| 46 | Open Hat | white noise, 300ms |
| 49/57 | Crash | white noise, 800ms |
| 51 | Ride | white noise + 600 Hz, 400ms |

- 16-voice polyphony; xorshift32 RNG; queue-based thread safety

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

- 16-voice polyphony; up to 4 FM operator phases per voice; queue-based thread safety

FM formula: `out = amp × env(t) × sin(2π·fC·t + depth × mod)` where `mod = sin(2π·fM·t + index × sin(2π·fM·t))`.

#### SampleDrum

Plays back pre-decoded OGG assets loaded by `SampleBank`. `SampleBank.load()` runs on `Dispatchers.IO` at startup; decoded `FloatArray`s are resampled to 48 kHz and passed to `NativeAudioEngine.loadSample()` → JNI → `AudioGraph::loadDrumSample()` → `InstrumentRepository::loadDrumSample()` → `SampleDrum::loadSample()`. The "Samples" chip in the UI is disabled with a loading spinner until loading completes.

- 16-voice polyphony; atomic sample loading (release-acquire ordering); queue-based thread safety

#### Backend Selector UI

`DrumEngineSelector` is a `FilterChip` row embedded in `MainScreen`:

```
[ Noise ]  [ FM Synth ]  [ Samples ]
```

The selected backend is persisted across restarts via `DrumBackendStore` (DataStore Preferences key `"drum_backend"`). `setDrumBackend(ordinal)` maps in `jni_bridge.cpp`: 0 → `"noise_drum"`, 1 → `"fm_drum"`, 2 → `"sample_drum"`, then calls `AudioGraph::setInstrument(9, id)`.

### Keyboard Instruments

Three keyboard types selectable via `KeyboardSoundSelector`:

```
[ Piano ]  [ Poly ]  [ Mono ]
```

When Poly or Mono is selected, a `WaveformSelector` appears:

```
[ Sine ]  [ Saw ]  [ Square ]
```

The selected keyboard type and waveform are persisted via `KeyboardTypeStore` and `WaveformStore`. `MainViewModel.instrumentId(type, waveform)` maps combinations to C++ instrument IDs:
- Piano → `"piano"`
- Poly + Sine → `"sine_polysynth"`, Poly + Saw → `"saw_polysynth"`, Poly + Square → `"square_polysynth"`
- Mono + Sine → `"sine_monosynth"`, Mono + Saw → `"saw_monosynth"`, Mono + Square → `"square_monosynth"`

Switching calls `AudioGraph::setInstrument(0, id)` which atomically swaps `mChannels[0]`.

### Sustain Button

`SustainButton` in `MainScreen` toggles sustain pedal (CC#64) via `NativeAudioEngine.controlChange(0, 64, value)`. When pressed: value=127. When released: value=0. Affects Piano, PolyOscillator instruments.

### On-Screen Instruments

#### PianoKeyboard (`ui/PianoKeyboard.kt`)

One-octave Canvas keyboard, C4–B4 (MIDI notes 60–71). Handles multi-touch via `Modifier.pointerInput` + `awaitPointerEventScope`. Tracks a `mutableStateMapOf<PointerId, Int>()` (pointer → active note). Hit-test checks black keys first (higher Z-order), then white keys. Calls `NativeAudioEngine.noteOn(0, note, 100)` on press and `noteOff(0, note)` on release/slide-off.

#### DrumTrigger (`ui/DrumTrigger.kt`)

4×2 grid of rounded drum pads. Each pad uses `awaitEachGesture` + `awaitFirstDown` / `waitForUpOrCancellation`. On press: `noteOn(9, note, 100)`. On release or cancel: `noteOff(9, note)`.

```
[ Closed Hat ] [ Open Hat ] [ Ride ] [ Crash ]
[    Kick    ] [   Snare  ] [Lo Tom] [ Hi Tom ]
```

### Loop Recorder

`LoopControls` in `MainScreen` provides three buttons for the MIDI loop recorder plus a time-signature button:

- **REC**: starts recording (Idle→Armed) or overdub (Playing→Overdubbing). Blinks when armed.
- **STOP**: stops recording (Recording→Playing or Overdubbing→Playing).
- **CLEAR**: clears the loop (Playing→Idle).
- **Time Signature button** (e.g. "4/4 ▾"): opens a dialog to pick beats-per-bar (2–7), note value (4th/8th), and bars (1/2/4/8/16). Stored in `UiState.timeSignature: TimeSignature`. Beat indicator dots show `beatsPerBar` positions during playback. C++ currently sends progress 0–3 (loop quarters); beat mapping will improve when C++ sends 0–99.

Loop state is also controllable via MIDI: CC#95 ch0 = start/overdub, CC#93 ch0 = stop.

State flows: C++ `LoopRecorder::onStateChange` → `MidiEvt{0xFF, state, 0, 0}` → `UICallback` → JNI `onLoopState(state)` → `MidiRouter.loopStateEvents` → `MainViewModel` → UI.

### ViewModel State

```kotlin
enum class DrumBackend { Noise, Fm, Samples }
enum class KeyboardType { Piano, Poly, Mono }
enum class SynthWaveform { Sine, Saw, Square }
enum class LoopState { Idle, Recording, Playing, Armed, Overdubbing }
sealed class AudioEngine { Oboe; Wifi(host: String, port: Int) }

data class TimeSignature(
    val beatsPerBar: Int = 4,
    val noteValue: Int = 4,   // denominator: 4 = quarter note, 8 = eighth note
    val bars: Int = 1,        // loop length in bars (1, 2, 4, 8, 16)
) {
    val label: String get() = if (bars == 1) "$beatsPerBar/$noteValue" else "${bars}×$beatsPerBar/$noteValue"
}

data class UiState(
    val permissionsGranted: Boolean = false,   // checked against system at ViewModel init
    val connectionStatus: ConnectionStatus = ConnectionStatus.Idle,
    val discoveredDevices: List<DeviceUiState> = emptyList(),
    val connectedDeviceAddress: String? = null,
    val recentEvents: List<MidiEventUiModel> = emptyList(),  // capped at 10
    val midiActivityPulse: Boolean = false,
    val drumBackend: DrumBackend = DrumBackend.Noise,
    val samplesLoaded: Boolean = false,
    val engine: AudioEngine = AudioEngine.Oboe,
    val selectEngineDialogVisible: Boolean = false,
    val keyboardType: KeyboardType = KeyboardType.Piano,
    val synthWaveform: SynthWaveform = SynthWaveform.Sine,
    val loopState: LoopState = LoopState.Idle,
    val loopLengthSec: Int = 0,              // loop progress 0–3 from C++ (quarters of loop); will be 0–99 when C++ updated
    val timeSignature: TimeSignature = TimeSignature(),
    val channels: List<ChannelStrip> = ...,
    val selectedChannel: Int = 0,
    val mixerVisible: Boolean = false,
)
```

### Threading

- BLE scan callbacks → main thread (`Handler(mainLooper)`)
- **OboeOutput**: Oboe audio thread → `onAudioReady()` → `MidiEngine::render()` → pollMidi + advanceLoop + renderAudio; pushes `MidiEvt` to `UICallback`'s `SpscRing` (non-blocking)
- **UICallback**: dispatch thread → drains `SpscRing` → JNI `onMidiEvent()` / `onLoopState()` → `MidiRouter` → `SharedFlow.tryEmit()` → ViewModel
- **WifiOutput**: `udpRenderLoop()` thread (~2.5 ms timer) → `MidiEngine::render()` → Opus encode → UDP send
- `NativeAudioEngine.noteOn/Off()` (called from UI thread) → JNI → `AudioGraph` → `MidiEngine` → instrument (lock-free via `mChannels` + `SpscRing` queues)
- `MidiEngine::mChannels[16]` are `std::atomic<Instrument*>` — UI thread writes via `setInstrument`, audio/render thread reads safely
- All instruments use `SpscRing` queues for noteOn/noteOff/CC from UI thread, drained at top of `render()`

### Cleanup (`MainViewModel.onCleared`)

```
bleScanner.stopScan()
bleMidiConnection.disconnect()   // closeMidiDevice() + closes MidiDevice
NativeAudioEngine.stop()         // stops engine (Oboe stream or UDP thread)
// AudioGraph destructor: stops outputs, stops UICallback, frees MidiEngine, then InstrumentRepository is freed
```
