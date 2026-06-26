# Plan: Move LoopRecorder from MidiEngine to AudioGraph

## Goal

Relocate `LoopRecorder` ownership from `MidiEngine` (audio engine layer) to `AudioGraph` (top-level coordinator). This removes loop-recording concerns from the audio engine interface and keeps `MidiEngine` focused purely on MIDI routing and audio rendering.

## Rationale

- `LoopRecorder` is a MIDI event recorder/player — it has no audio processing responsibilities.
- `AudioEngine` interface is polluted with 5 loop virtual methods unrelated to audio I/O.
- `AudioGraph` already acts as the loop coordinator (calls `loopRecordEvent` on every `noteOn/Off`, delegates all loop control).
- `MidiEngine` should know only about MIDI routing (`mChannels[16]`), AMidi, and rendering.

## Before

```
AudioGraph
├── InstrumentRepository
└── unique_ptr<AudioEngine>
    └── MidiEngine
        ├── mChannels[16]
        ├── mLoopRecorder          ← owned here
        ├── pollMidi() → mLoopRecorder.onMidiEvent()
        ├── advanceLoop() → mLoopRecorder.advance()
        └── loopStartRecord/StopRecord/Clear/State/RecordEvent
```

## After

```
AudioGraph
├── InstrumentRepository
├── LoopRecorder                   ← owned here
└── unique_ptr<AudioEngine>
    └── MidiEngine
        ├── mChannels[16]
        ├── mMidiObserver          ← callback set by AudioGraph
        ├── mAdvanceCallback       ← callback set by AudioGraph
        ├── pollMidi() → mMidiObserver(msg, timestamp)
        └── advanceLoop() → mAdvanceCallback(frames)
```

## Step-by-Step Changes

### Step 1: Update `AudioEngine.h`

**Remove** the 5 loop virtual methods:
- `loopStartRecord()`
- `loopStopRecord()`
- `loopClear()`
- `loopState()`
- `loopRecordEvent(MidiMsgType, uint8_t, uint8_t, uint8_t)`

**Add** 3 observer/callback virtual methods (all default no-op):
- `setMidiObserver(std::function<void(const MidiMsg&, int64_t)>)` — called for each BLE MIDI event
- `setAdvanceCallback(std::function<void(int32_t)>)` — called each render cycle to advance loop
- `pushUiEvent(uint8_t ch, uint8_t type, uint8_t d1, uint8_t d2)` — pushes event to dispatch queue for UI

### Step 2: Update `MidiEngine.h`

- Remove `#include "LoopRecorder.h"`
- Remove `LoopRecorder mLoopRecorder` member
- Remove loop method overrides: `loopStartRecord`, `loopStopRecord`, `loopClear`, `loopState`, `loopRecordEvent`
- Add `std::function<void(const MidiMsg&, int64_t)> mMidiObserver` member
- Add `std::function<void(int32_t)> mAdvanceCallback` member
- Add overrides: `setMidiObserver()`, `setAdvanceCallback()`, `pushUiEvent()`
- Keep `advanceLoop(int32_t frames)` as a protected method (delegates to callback)

### Step 3: Update `MidiEngine.cpp`

- Remove constructor body (only set up `mLoopRecorder.onStateChange`)
- Remove `loopStartRecord()`, `loopStopRecord()`, `loopClear()`, `loopState()`, `loopRecordEvent()` implementations
- In `pollMidi()`: replace `mLoopRecorder.onMidiEvent(m, timestamp)` with `mMidiObserver(m, timestamp)` (guard with null check)
- Rewrite `advanceLoop()`: replace `mLoopRecorder.advance(frames, ...)` with `mAdvanceCallback(frames)` (guard with null check)
- Implement `setMidiObserver()`: store into `mMidiObserver`
- Implement `setAdvanceCallback()`: store into `mAdvanceCallback`
- Implement `pushUiEvent()`: push `MidiEvt{ch, type, d1, d2}` into `mEventQueue`

### Step 4: Update `AudioGraph.h`

- Add `#include "LoopRecorder.h"`
- Add `LoopRecorder mLoopRecorder` member (declared **between** `mRepository` and `mEngine`)
- Add private method `void wireEngine()`

Member declaration order (reverse destruction order):
```cpp
InstrumentRepository         mRepository;    // dies last
LoopRecorder                 mLoopRecorder;  // dies before engine
std::unique_ptr<AudioEngine> mEngine;        // dies first
```

### Step 5: Update `AudioGraph.cpp`

- Implement `wireEngine()`:
  - `mEngine->setMidiObserver(...)` → calls `mLoopRecorder.onMidiEvent(msg, timestamp)`
  - `mEngine->setAdvanceCallback(...)` → calls `mLoopRecorder.advance(frames, fire)` where `fire` routes through `mEngine->noteOn/Off/CC`
  - `mLoopRecorder.onStateChange = ...` → calls `mEngine->pushUiEvent(0xFF, state, 0, 0)`
- Call `wireEngine()` in constructor (after creating default engine)
- Call `wireEngine()` in `setEngine()` (after swapping engine)
- Implement `loopStartRecord/StopRecord/Clear/State` directly on `mLoopRecorder`
- Implement `loopRecordEvent()` → `mLoopRecorder.onUiMidiEvent()`
- In `noteOn/Off`: call `mLoopRecorder.onUiMidiEvent()` directly instead of `mEngine->loopRecordEvent()`

### Step 6: Verify no changes needed

- `OboeEngine.h/cpp` — calls `advanceLoop(numFrames)` which still exists on `MidiEngine`. **No changes.**
- `WifiEngine.h/cpp` — calls `advanceLoop(kFramesPerBuf)` which still exists on `MidiEngine`. **No changes.**
- `jni_bridge.cpp` — calls `AudioGraph::loopStartRecord()` etc. Signatures unchanged. **No changes.**

## Files Changed Summary

| File | Action |
|------|--------|
| `AudioEngine.h` | Remove 5 loop methods, add 3 observer/callback methods |
| `MidiEngine.h` | Remove LoopRecorder include/member, remove loop overrides, add observer/callback members |
| `MidiEngine.cpp` | Remove loop implementations, update pollMidi/advanceLoop to use callbacks |
| `AudioGraph.h` | Add LoopRecorder member, add wireEngine() |
| `AudioGraph.cpp` | Implement wireEngine(), implement loop methods directly, update noteOn/Off and setEngine() |
| `OboeEngine.h/cpp` | No changes |
| `WifiEngine.h/cpp` | No changes |
| `jni_bridge.cpp` | No changes |

## Verification

1. `./gradlew assembleDebug` — must compile cleanly
2. `./gradlew test` — unit tests pass
3. Manual: on-screen piano/drum pads still produce sound
4. Manual: loop record/play/clear still works via UI
5. Manual: BLE MIDI events still route to instruments and record into loop
