# Architecture

This document describes the C++ native audio architecture of the BTMID app.

## Overview

The native layer is organized in three layers:

```
AudioGraph                          ← JNI entry point; top-level coordinator
├── InstrumentRepository            ← owns all Instrument instances
├── LoopRecorder                    ← MIDI event loop recorder/player
└── unique_ptr<AudioEngine>         ← audio I/O; knows only Instrument interface
    ├── OboeEngine                  ← Oboe stream → local speaker
    └── WifiEngine                  ← UDP + Opus → network receiver
```

`AudioGraph` is the single object exposed to JNI. It owns `InstrumentRepository`, `LoopRecorder`, and the active `AudioEngine`. Member declaration order ensures reverse destruction: engine stops first, then LoopRecorder, then instruments are freed.

## AudioGraph

The top-level coordinator and sole JNI entry point.

**Responsibilities:**
- Creates and owns `InstrumentRepository`, `LoopRecorder`, and `AudioEngine`
- Wires instruments to engine channels via `InstrumentRepository`
- Owns `LoopRecorder` and connects it to the active engine via callbacks
- Routes MIDI from JNI (on-screen input) to both engine and recorder
- Delegates loop control to `LoopRecorder` directly

**Key methods:**

| Method | Description |
|--------|-------------|
| `setOutput(unique_ptr<AudioEngine>)` | Stops current engine, swaps to new one, rewires LoopRecorder callbacks |
| `setInstrument(channel, id)` | Repository lazy-creates instrument → wires to engine |
| `loadDrumSample(id, data, len)` | Forwards to both repository and engine |
| `noteOn/Off/CC(ch, ...)` | Routes to engine + records UI events into LoopRecorder |
| `loopStartRecord/StopRecord/Clear/State` | Direct calls to LoopRecorder |
| `wireEngine()` (private) | Sets up MIDI observer, advance callback, and state-change callback on current engine |

**Engine wiring (`wireEngine`):**

When the engine is created or swapped, AudioGraph sets three callbacks:

1. **MIDI observer** — `engine->setMidiObserver(...)` → `LoopRecorder::onMidiEvent(msg, timestamp)` for each BLE MIDI event received
2. **Advance callback** — `engine->setAdvanceCallback(...)` → `LoopRecorder::advance(frames, fire)` where `fire` routes played-back MIDI through `engine->noteOn/Off/CC`
3. **State change** — `LoopRecorder::onStateChange = ...` → `engine->pushUiEvent(0xFF, state, 0, 0)` to notify UI of loop state transitions

## AudioEngine (interface)

Pure-virtual base for all audio engines. Knows only about the `Instrument` interface and MIDI routing.

| Method | Pure virtual | Notes |
|--------|-------------|-------|
| `start()` / `stop()` | ✓ | lifecycle |
| `noteOn` / `noteOff` / `controlChange` | ✓ / ✓ / no-op | MIDI routing |
| `setInstrument(channel, Instrument*)` | no-op | wired by InstrumentRepository |
| `loadSample(id, data, len)` | no-op | only used by InstrumentRepository path |
| `setDrumBackend(id)` | no-op | only used by InstrumentRepository path |
| `openMidiDevice` / `closeMidiDevice` | ✓ | BLE MIDI device binding |
| `setMidiObserver(callback)` | no-op | AudioGraph sets this to forward BLE MIDI to LoopRecorder |
| `setAdvanceCallback(callback)` | no-op | AudioGraph sets this to advance LoopRecorder each render cycle |
| `pushUiEvent(ch, type, d1, d2)` | no-op | pushes event to dispatch queue for UI notification |

AudioEngine has no knowledge of LoopRecorder. It communicates with AudioGraph purely through callbacks.

## MidiEngine

Partial `AudioEngine` implementation shared by `OboeEngine` and `WifiEngine`.

**Responsibilities:**
- Channel-indexed instrument routing (`mChannels[16]` — `std::atomic<Instrument*>`)
- AMidi port open/close and JNI callback registration
- `pollMidi()` — drains AMidi, routes to instruments, calls MIDI observer, pushes to event queue
- `render()` — renders each unique instrument into a float buffer (deduplicates by pointer)
- `advanceLoop()` — calls the advance callback (set by AudioGraph) to tick the loop recorder

**Observer/callback members:**
- `std::function<void(const MidiMsg&, int64_t)> mMidiObserver` — called in `pollMidi()` for each received BLE MIDI event
- `std::function<void(int32_t)> mAdvanceCallback` — called in `advanceLoop()` each render cycle

MidiEngine does not own or know about LoopRecorder. It only knows about callback functions.

## InstrumentRepository

The sole owner of all `Instrument` instances.

- Lazy-creates instruments by string ID on first `setInstrument` call
- Calls `AudioEngine::setInstrument(channel, ptr)` to wire instruments to engine channels
- Holds `SampleDrum*` for `loadDrumSample`; all other instruments accessed via `Instrument*`
- Known IDs: `"piano"`, `"noise_drum"`, `"fm_drum"`, `"sample_drum"`, `"sine_polysynth"`, `"saw_polysynth"`, `"square_polysynth"`, `"sine_monosynth"`, `"saw_monosynth"`, `"square_monosynth"`

## LoopRecorder

MIDI event loop recorder and player. Records timestamped MIDI events and plays them back in sync with the audio clock.

**State machine:** `Idle → Armed → Recording → Playing` (with `Overdubbing`)

**Integration:**
- Owned by `AudioGraph` (not by any engine)
- Receives BLE MIDI events via `onMidiEvent()` — called from the MIDI observer callback set on the engine
- Receives UI MIDI events via `onUiMidiEvent()` — called directly from `AudioGraph::noteOn/Off`
- Advances via `advance()` — called from the engine's advance callback each render cycle
- Notifies UI of state changes via `onStateChange` callback → `engine->pushUiEvent()`

## OboeEngine

`MidiEngine` subclass + Oboe `AudioStreamDataCallback`.

**Audio thread (`onAudioReady`):**
1. `pollMidi()` — drain AMidi, route to instruments, call MIDI observer, push to event queue
2. `advanceLoop(numFrames)` — tick LoopRecorder via callback
3. Zero buffer, `render()` — render instruments into Oboe output

**Dispatch thread (`dispatchLoop`):**
- Drains `SpscRing<MidiEvt>` → JNI callback → `MidiRouter` → `SharedFlow` → ViewModel → UI
- Handles special `channel=0xFF` events as loop state notifications

## WifiEngine

`MidiEngine` subclass for network audio streaming.

**Render thread (`udpRenderLoop`, 10 ms cadence):**
1. `pollMidi()` — same as OboeEngine
2. `advanceLoop(kFramesPerBuf)` — tick LoopRecorder via callback
3. Zero buffer, `render()` — render instruments
4. Skip if silent; otherwise `opus_encode_float()` → `sendto()` UDP

## Data Flow

```
BLE MIDI device
  → MidiManager.openBluetoothDevice(bluetoothDevice)
  → BleMidiConnection passes MidiDevice to NativeAudioEngine.openMidiDevice()
  → jni_bridge → AudioGraph::openMidiDevice() → AudioEngine::openMidiDevice()

OboeEngine path (local speaker):
  → onAudioReady() (Oboe audio thread)
      pollMidi()
        → AMidiOutputPort_receive() → parseMidi() → MidiMsg
        → route via mChannels[channel] → Instrument::noteOn/Off/CC
        → mMidiObserver(msg, timestamp) → AudioGraph → LoopRecorder::onMidiEvent()
        → push MidiEvt to SpscRing
      advanceLoop(numFrames)
        → mAdvanceCallback(frames) → AudioGraph → LoopRecorder::advance()
        → LoopRecorder fires played-back events → engine->noteOn/Off/CC → Instrument
      render() → Oboe float output
  → dispatchLoop() (dedicated thread)
      drain SpscRing → JNI callback → MidiRouter → SharedFlow → UI

WifiEngine path (network):
  → udpRenderLoop() (dedicated thread, 10 ms)
      pollMidi() → same as OboeEngine
      advanceLoop(kFramesPerBuf) → same as OboeEngine
      render() → float buf → opus_encode_float → sendto UDP

On-screen input (no BLE device):
  PianoKeyboard (ch 0) ──┐
  DrumTrigger   (ch 9) ──┴→ NativeAudioEngine.noteOn/Off()
      → jni_bridge → AudioGraph::noteOn/Off()
        → engine->noteOn/Off() → Instrument
        → LoopRecorder::onUiMidiEvent()
```

## Threading

- **BLE scan callbacks** → main thread
- **Oboe audio thread** → `onAudioReady()` → pollMidi → advanceLoop → render
- **Oboe dispatch thread** → drains SpscRing → JNI → MidiRouter → SharedFlow → ViewModel
- **WifiEngine render thread** → 10 ms timer → pollMidi → advanceLoop → render → Opus → UDP
- **UI thread** → `NativeAudioEngine.noteOn/Off()` → JNI → AudioGraph → engine + LoopRecorder

## Lock-free guarantees

- `AudioEngine::mChannels[16]` are `std::atomic<Instrument*>` — UI thread writes via `setInstrument`, audio thread reads safely
- `SpscRing<MidiEvt, 256>` — audio thread produces, dispatch thread consumes
- `LoopRecorder::mState` is `std::atomic<State>` — audio thread reads, UI thread writes
- `LoopRecorder::mPlayEventsPtr` is `shared_ptr` — lock-free read on audio thread via atomic load
