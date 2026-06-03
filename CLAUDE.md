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

- **Language**: Kotlin
- **UI**: Jetpack Compose with Material3
- **Min/Target SDK**: 36
- **AGP**: 9.1.1, Kotlin: 2.2.10
- **Compose BOM**: 2026.02.01
- **DataStore Preferences**: 1.1.4 — persists drum backend selection
- Dependency versions are centralized in `gradle/libs.versions.toml`

## Project Structure

Single-module Android app (`app/`). Package root: `org.egon12.btmid`.

- `app/src/main/java/org/egon12/btmid/` — app source
- `app/src/main/java/org/egon12/btmid/ui/theme/` — Compose theme (`Color.kt`, `Theme.kt`, `Type.kt`)
- `app/src/test/` — JUnit unit tests
- `app/src/androidTest/` — instrumented tests (Espresso + Compose UI test)

`MainActivity` uses `ComponentActivity` + `setContent` with `BtmidTheme` wrapping a `Scaffold`. New screens/composables should follow the same pattern: `@Composable` functions in their own files, previewed with `@Preview`.

---

## Feature: Bluetooth MIDI Receiver with Piano + Drums

The app receives BLE MIDI and synthesizes audio entirely via `AudioTrack` — no native `.so` files.

- MIDI channel 1 (index `0`) → Piano (additive sine synthesis)
- MIDI channel 10 (index `9`) → Drums (three selectable backends: Noise, FM Synth, Samples)

### File Structure

```
app/src/main/java/org/egon12/btmid/
  bluetooth/
    BleScanner.kt            — BluetoothLeScanner filtered by MIDI service UUID
    BleMidiConnection.kt     — MidiManager.openBluetoothDevice, owns MidiDevice lifecycle

  midi/
    MidiEvent.kt             — sealed class: NoteOn, NoteOff, ControlChange
    MidiParser.kt            — pure fun: ByteArray → List<MidiEvent> (handles running-status)
    MidiRouter.kt            — routes events to PianoSynth/DrumSynth, emits to SharedFlow
    AppMidiReceiver.kt       — MidiReceiver subclass, calls MidiParser → MidiRouter

  synth/
    AudioEngine.kt           — owns AudioTrack, coroutine render loop on Dispatchers.Default
    PianoSynth.kt            — additive synthesis (5 harmonics, ADSR envelope)
    DrumSynth.kt             — DrumSynth interface + DrumSynthProxy (runtime-swappable backend)
    NoiseDrumSynth.kt        — noise-burst synthesis per GM drum type (original backend)
    FmDrumSynth.kt           — FM operator synthesis per GM drum type
    SampleBank.kt            — decodes OGG assets → FloatArray per drum name via MediaCodec
    SampleDrumSynth.kt       — plays back pre-loaded samples from SampleBank
    Voice.kt                 — data class: note, phase accumulators, envelope state

  ui/
    MainScreen.kt            — top-level Composable (permission banner, scan, device list)
    DeviceListItem.kt        — single discovered-device row
    MidiActivityIndicator.kt — animated dot that flashes on each MIDI event
    DrumEngineSelector.kt    — FilterChip row to switch Noise / FM Synth / Samples at runtime

  DrumBackendStore.kt        — DataStore<Preferences> wrapper; persists selected DrumBackend
  MainViewModel.kt           — UiState + scan/connect/disconnect/setDrumBackend actions
```

Sample assets: `app/src/main/assets/samples/drums/` — `kick.ogg`, `snare.ogg`, `closed_hat.ogg`, `open_hat.ogg`, `crash.ogg`, `ride.ogg`, `tom.ogg` (all toms share one file). Declared `noCompress` in `build.gradle.kts` so MediaExtractor can seek.

### Data Flow

```
BluetoothLeScanner (filter: MIDI UUID 03B80E5A-…)
  → MidiManager.openBluetoothDevice(bluetoothDevice)
  → MidiDevice.openOutputPort(0).connect(AppMidiReceiver)
  → AppMidiReceiver.onSend(bytes)
  → MidiParser.parse() → List<MidiEvent>
  → MidiRouter.route(event)
      channel 0 → PianoSynth.noteOn/Off(note, velocity)
      channel 9 → DrumSynth.noteOn/Off(note, velocity)
      all events → SharedFlow → MainViewModel → UI log
  → AudioEngine render coroutine
      sums active Voice buffers → AudioTrack.write(floatArray)
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

### MIDI Parsing Rules

- `0x9n` with velocity 0 → treat as NoteOff (MIDI 1.0 spec — very common on BLE devices)
- Running status: if byte has no high bit, reuse previous status byte
- Channel is zero-indexed: `statusByte and 0x0F`

### Piano Synthesis (PianoSynth)

- Frequency: `440.0 * 2.0.pow((note - 69) / 12.0)`
- 5 harmonics with amplitudes: 1.00, 0.50, 0.25, 0.12, 0.06
- ADSR: attack 5ms, decay 80ms, sustain 60% of peak, release 300ms exponential
- Peak amplitude: `velocity / 127.0 * 0.7`
- Voice cap: 8 simultaneous voices (steal oldest on overflow)

### Drum Engine

Three backends implement the `DrumSynth` interface. `DrumSynthProxy` holds a `@Volatile var backend: DrumSynth` so the active backend can be swapped at runtime between render passes without locking. Both `MidiRouter` and `AudioEngine` hold the same proxy instance.

```kotlin
interface DrumSynth {
    fun noteOn(note: Int, velocity: Int)
    fun noteOff(note: Int)
    fun render(buffer: FloatArray)
}
```

#### NoiseDrumSynth (default)

| GM Note | Sound | Method |
|---------|-------|--------|
| 35/36 | Bass Drum | 60 Hz sine, 150ms decay |
| 38/40 | Snare | white noise + 200 Hz tone, 100ms |
| 42 | Closed Hat | difference-filtered noise (`y[n]=x[n]-x[n-1]`), 50ms |
| 46 | Open Hat | white noise, 300ms |
| 49/57 | Crash | white noise, 800ms |
| 51 | Ride | white noise + 600 Hz, 400ms |

#### FmDrumSynth

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

#### SampleDrumSynth

Plays back pre-decoded OGG assets from `SampleBank`. All toms map to `tom.ogg`. `SampleBank.load()` is called on `Dispatchers.IO` at startup using `MediaCodec` + `MediaExtractor`; decoded once to mono 44100 Hz `FloatArray` per file. The "Samples" chip in the UI is disabled with a loading spinner until loading completes.

#### Backend Selector UI

`DrumEngineSelector` is a `FilterChip` row embedded in `MainScreen`, always visible below the connection status:

```
[ Noise ]  [ FM Synth ]  [ Samples ]
```

The selected backend is persisted across restarts via `DrumBackendStore` (DataStore Preferences key `"drum_backend"`).

### AudioEngine

- Format: `ENCODING_PCM_FLOAT`, mono, 44100 Hz
- Buffer: `max(minBufferSize, sampleRate/200 * 4)` (5ms floor)
- AudioAttributes: `USAGE_GAME` + `CONTENT_TYPE_SONIFICATION`
- `PERFORMANCE_MODE_LOW_LATENCY` — routes through Android fast mixer, target ≤20ms output latency
- Render loop on `Dispatchers.Default`; sums PianoSynth + DrumSynth into a single buffer each iteration

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
- `MidiReceiver.onSend` → MIDI binder thread → `router.route()` → `SharedFlow.tryEmit` to ViewModel
- `PianoSynth.noteOn/Off` / `DrumSynth.noteOn` → post to `ConcurrentLinkedQueue` (non-blocking, no lock)
- AudioEngine render → `Dispatchers.Default`; drains queues at top of each render pass — MIDI thread never waits on audio
- Drum backend swap → `DrumSynthProxy.backend` is `@Volatile`; main thread writes, render thread reads — single-word volatile write is safe without locking

### Cleanup (MainViewModel.onCleared)

```
bleScanner.stopScan()
bleMidiConnection.disconnect()   // closes inputPort + MidiDevice
audioEngine.stop()               // stops + releases AudioTrack, cancels coroutine
```

### Implementation Sequence

Each step leaves the app in a runnable state:

1. ~~Manifest + permission UI — banner with "Grant Permissions" button~~ ✅ Done
2. ~~BLE scan — `BleScanner` + device list; verify devices appear in Logcat~~ ✅ Done
3. ~~MIDI connection — stub `AppMidiReceiver` logging raw bytes to Logcat~~ ✅ Done
4. ~~MIDI parser — `MidiParser` + `MidiEvent` with unit tests~~ ✅ Done
5. ~~AudioEngine scaffold — silent render loop, verify no `AudioTrack` errors~~ ✅ Done
6. ~~Piano synth — wire `PianoSynth` through `MidiRouter`, test with keyboard~~ ✅ Done
7. ~~Drum synth — wire `NoiseDrumSynth`, test with drum pads~~ ✅ Done
8. ~~UI polish — activity indicator animation, event log, connection status styling~~ ✅ Done
9. ~~Drum engine upgrade — `DrumSynth` interface + `DrumSynthProxy`; `NoiseDrumSynth` (renamed), `FmDrumSynth`, `SampleBank` + `SampleDrumSynth`, `DrumEngineSelector` UI~~ ✅ Done
10. ~~Persist drum backend — `DrumBackendStore` (DataStore Preferences), restored on app restart~~ ✅ Done
