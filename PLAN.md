# Latency Reduction — Target <15 ms

## Goal

Reduce the felt latency between hitting a drum pad and hearing the sound. Sub-10 ms total is not achievable over BLE MIDI (7.5 ms minimum BLE connection interval is a hard protocol constraint), but the current setup is much worse than it needs to be.

| Source | Current | After fix |
|--------|---------|-----------|
| BLE connection interval | ~30–45 ms (default) | ~7–10 ms |
| Audio output (AudioTrack write-loop) | ~20–30 ms | ~5–8 ms |
| **Total** | **~50–75 ms** | **~12–18 ms** |

Piano latency is a non-goal for now, but the new C++ engine covers it naturally.

---

## Architecture — Full Realtime Path in C++

Java handles device discovery and one-time connection setup (unavoidable — no NDK API exists for `MidiManager` or `BluetoothGatt`). After that, **all realtime work moves to C++**: MIDI polling via AMidi, parsing, routing, synthesis, and audio output via Oboe. No Java callbacks on the hot path.

```
[Java — setup only, runs once per connection]
BluetoothDevice
  → connectGatt(TRANSPORT_LE)
      onConnected → gatt.requestConnectionPriority(CONNECTION_PRIORITY_HIGH)
  → MidiManager.openBluetoothDevice() → MidiDevice
      → MidiDevice.openOutputPort(0) → MidiOutputPort
          → JNI: NativeEngine.setOutputPort(jOutputPort)   ← hand off to C++

[C++ — all realtime work]
Oboe onAudioReady(buffer, frames):
  while AMidiOutputPort_receive() has data:
    MidiParser::parse(bytes) → NoteOn / NoteOff / CC
    channel 0 → pianoRenderer.noteOn/Off(note, velocity)
    channel 9 → drumRenderer.noteOn/Off(note, velocity)
    → post event to UI queue (non-blocking, for event log only)
  for renderer in renderers:
    renderer.render(buffer, frames)        ← AudioRenderer interface
  → Oboe → audio hardware
```

AMidi (`AMidiOutputPort_receive`) is non-blocking and polled at the top of each `onAudioReady()` call, so MIDI events are consumed with zero extra thread hops — minimum possible latency from arrival to sound.

---

## C++ Interface — AudioRenderer

All synth backends implement one interface. The Oboe callback is completely agnostic about what renders:

```cpp
class AudioRenderer {
public:
    virtual ~AudioRenderer() = default;
    virtual void noteOn(int channel, int note, int velocity) = 0;
    virtual void noteOff(int channel, int note) = 0;
    virtual void render(float* buffer, int32_t frames) = 0;
};
```

This also makes future backends (SF2 via FluidSynth, statically-linked synth plugins) drop-in replacements.

---

## New / Changed Files

```
app/src/main/cpp/
  CMakeLists.txt
  NativeEngine.cpp/.h      — owns Oboe stream + AMidi port + renderer list
  MidiParser.cpp/.h        — pure C++ byte parser (mirrors current MidiParser.kt)
  AudioRenderer.h          — abstract interface (above)
  renderers/
    PianoRenderer.cpp/.h   — port of PianoSynth.kt
    NoiseDrumRenderer.cpp/.h
    FmDrumRenderer.cpp/.h
    SampleDrumRenderer.cpp/.h
  jni_bridge.cpp           — JNI entry points (see below)

synth/AudioEngine.kt       — replaced: thin wrapper holding native pointer, calls JNI
bluetooth/BleMidiConnection.kt — adds GATT handle + CONNECTION_PRIORITY_HIGH
midi/AppMidiReceiver.kt    — removed (AMidi replaces it on the audio path)
midi/MidiParser.kt         — removed (replaced by C++)
midi/MidiRouter.kt         — removed (replaced by C++)
build.gradle.kts           — add cmake block; add oboe + amidi prefab deps
```

### JNI surface (`jni_bridge.cpp`)

```cpp
// lifecycle
jlong  NativeEngine_create(JNIEnv*, jobject)
void   NativeEngine_setOutputPort(JNIEnv*, jobject, jlong ptr, jobject jOutputPort)
void   NativeEngine_start(JNIEnv*, jobject, jlong ptr)
void   NativeEngine_stop(JNIEnv*, jobject, jlong ptr)
void   NativeEngine_destroy(JNIEnv*, jobject, jlong ptr)
// runtime control
void   NativeEngine_setDrumBackend(JNIEnv*, jobject, jlong ptr, jint backendId)
void   NativeEngine_registerEventCallback(JNIEnv*, jobject, jlong ptr, jobject listener)
```

`registerEventCallback` stores a global JNI ref to a Kotlin `MidiEventListener`. The engine posts events to a separate lock-free queue; a dedicated non-realtime thread drains that queue and calls the listener — this is the only path back to Kotlin and only exists for the UI event log.

---

## Phase 1 — BLE Connection Priority (pure Kotlin)

`MidiManager.openBluetoothDevice` creates a GATT connection internally but never requests a high-priority connection interval. Opening our own GATT handle to the same device first and calling `requestConnectionPriority(CONNECTION_PRIORITY_HIGH)` lowers the shared connection interval from ~30–45 ms to ~7.5 ms.

### Changes — `BleMidiConnection.kt`

1. Add `connectGatt(context, false, gattCallback, TRANSPORT_LE)` before `openBluetoothDevice`
2. In `onConnectionStateChange(STATE_CONNECTED)` → `gatt.requestConnectionPriority(CONNECTION_PRIORITY_HIGH)`
3. Hold both `BluetoothGatt` and `MidiDevice`; close both on `disconnect()`

**Expected gain:** ~25–35 ms off BLE latency.

### Delivery Steps

- [x] **P1-1** — Add GATT handle + priority request in `BleMidiConnection`
- [x] **P1-2** — Confirm connect / disconnect lifecycle has no regression

---

## Phase 2 — Native Engine (C++/NDK)

Oboe with `EXCLUSIVE` sharing mode + `LOW_LATENCY` performance mode uses a hardware callback that bypasses Android's software mixer. AMidi eliminates Java callbacks on the MIDI receive path. Together these target **5–8 ms output latency** on modern hardware.

Each step below leaves the Kotlin synths active and the app fully working. The strategy: port all renderers with host WAV tests first, then do one Kotlin switch (P2-7) that immediately gives full C++ sounds — no intermediate "everything is 440 Hz sine" phase.

Host test harness lives in `cpp_host_tests/` (standalone CMake, macOS-native, no Oboe dep). Each renderer step adds test cases + WAV output to `renderer_test.cpp`.

---

### ~~P2-1 — Build infrastructure (no behavior change)~~ ✅ Done

Add NDK + CMake to the project. No C++ code that does anything yet.

- Add `ndkVersion`, `cmake {}` block to `build.gradle.kts`
- Add Oboe prefab AAR dependency
- Create `app/src/main/cpp/CMakeLists.txt` — links Oboe, produces `libbtmid.so`
- Create `app/src/main/cpp/NativeEngine.cpp` — empty class, exports one JNI stub `NativeEngine_create` that returns 0
- `./gradlew assembleDebug` passes; `.so` appears in APK

**Test:** build succeeds. App behavior unchanged.

---

### ~~P2-2 — C++ infra + SineTestRenderer + host test harness~~ ✅ Done

- `AudioRenderer.h` — abstract interface
- `SineTestRenderer.cpp/.h` — 440 Hz sine for 500 ms on any `noteOn`
- `NativeEngine.cpp/.h` — opens Oboe stream (`EXCLUSIVE` + `LOW_LATENCY`), renders via `AudioRenderer` list in `onAudioReady()`
- `jni_bridge.cpp` — JNI entry points: `create`, `start`, `stop`, `destroy`, `noteOn`, `noteOff`
- `NativeAudioEngine.kt` — thin Kotlin singleton holding native pointer
- `cpp_host_tests/` — CMake host build; `renderer_test.cpp` asserts silence/sound/decay and writes WAV files

Kotlin synths (`AudioEngine.kt`, `MidiRouter.kt`) **unchanged** — Kotlin wiring deferred to P2-7.

**Test:** 4 host assertions pass; `sine_test.wav` sounds correct.

---

### ~~P2-3 — NoiseDrumRenderer~~ ✅ Done

Port `NoiseDrumSynth.kt` to C++. Validate sound with a host WAV test.

- `renderers/NoiseDrumRenderer.cpp/.h` — direct port of the noise-burst logic per GM note
- Add `NoiseDrumRenderer` test to `cpp_host_tests/renderer_test.cpp`: trigger each drum type (kick, snare, hat, crash, ride), write `noise_drums.wav`

**Test:** host assertions pass; `noise_drums.wav` sounds correct for all drum types.

---

### ~~P2-4 — FmDrumRenderer~~ ✅ Done

Port `FmDrumSynth.kt` to C++.

- `renderers/FmDrumRenderer.cpp/.h` — port of FM operator logic per GM note
- Add `FmDrumRenderer` test to `cpp_host_tests/renderer_test.cpp`: same drum sequence, write `fm_drums.wav`

**Test:** host assertions pass; `fm_drums.wav` sounds correct. Compare with `noise_drums.wav` to hear the difference.

---

### ~~P2-5 — PianoRenderer~~ ✅ Done

Port `PianoSynth.kt` to C++.

- `renderers/PianoRenderer.cpp/.h` — additive sine (5 harmonics) + ADSR envelope, 8-voice cap
- Add `PianoRenderer` test to `cpp_host_tests/renderer_test.cpp`: play a C major chord, write `piano_chord.wav`

**Test:** host assertions pass; `piano_chord.wav` sounds like the current Kotlin piano.

---

### ~~P2-6 — SampleDrumRenderer~~ ✅ Done

OGG decoding stays in Kotlin (`MediaCodec`) — no NDK equivalent. Decoded `FloatArray`s are passed to C++ once at startup.

- `renderers/SampleDrumRenderer.cpp/.h` — holds `float*` per drum name; `render()` mixes the relevant buffer
- Add `NativeEngine_loadSample(name: String, floatArray: FloatArray)` JNI method
- `SampleBank.kt` decodes OGGs as before, then calls `loadSample` for each file

No host WAV test (samples come from Android assets); validated on device in P2-7.

---

### P2-7 — Kotlin wiring (all C++ sounds go live)

Switch `AudioEngine.kt` and `MidiRouter.kt` to `NativeAudioEngine`. All real sounds come from C++ immediately — no 440 Hz sine phase.

- Replace `AudioEngine.kt`: drop `AudioTrack`; call `NativeAudioEngine.start()/stop()`
- Replace `MidiRouter.kt`: call `NativeAudioEngine.noteOn/noteOff` instead of Kotlin synths
- Update `MainViewModel`: remove synth args from `AudioEngine` + `MidiRouter` constructors; wire `setDrumBackend` to `NativeAudioEngine.setDrumBackend(int)`
- Add `NativeEngine_setDrumBackend(int)` JNI method; `NativeEngine` swaps drum backend atomically
- `NativeEngine` routes: channel 0 → `PianoRenderer`, channel 9 → active drum renderer
- Remove `SineTestRenderer`

**Test:** piano + all three drum backends work on device. `DrumEngineSelector` UI switches backends at runtime.

---

### P2-8 — AMidi input (no Java on the hot path)

Replace the Kotlin `MidiRouter` → JNI `noteOn/noteOff` call chain with native AMidi polling directly inside `onAudioReady()`.

- In `BleMidiConnection.kt`: after `openBluetoothDevice`, pass the `MidiOutputPort` Java object to C++ via `NativeEngine_setOutputPort(jOutputPort)`
- `NativeEngine.cpp`: call `AMidiOutputPort_fromJava(env, jOutputPort)`; poll `AMidiOutputPort_receive()` at the top of every `onAudioReady()`; parse bytes with `MidiParser.cpp`; dispatch to renderers
- Add a lock-free event queue in `NativeEngine`; a dedicated non-realtime thread drains it and calls a registered Kotlin `MidiEventListener` for the UI event log and MIDI activity indicator

**Test:** MIDI activity indicator and event log still update. Audio path no longer involves any Kotlin on each note event.

---

### P2-9 — Cleanup + latency measurement

Remove files that are now dead code; confirm latency improvement.

- Delete `AppMidiReceiver.kt`, `MidiParser.kt`, `MidiRouter.kt`, `PianoSynth.kt`, `NoiseDrumSynth.kt`, `FmDrumSynth.kt`, `SampleDrumSynth.kt`, `SampleBank.kt`, `DrumSynth.kt` (interface), `AudioEngine.kt`
- Measure round-trip latency using a loopback cable + `AudioLatencyTester` or logcat timestamps on `noteOn` vs first audio sample
- Confirm audio output latency <10 ms on test device

---

## Recommended Order

Phase 1 first (standalone win, no NDK) → Phase 2 (main win, each step leaves app runnable)
