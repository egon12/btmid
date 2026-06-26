# Architecture Refactor: UICallback + MidiEngine + OboeOutput

## Current Problems

1. `MidiEngine::setOutputPort` (line 39) recursively calls itself — infinite loop
2. `MidiEngine::setOutputPort` (line 42) calls `OboeEngine::dispatchLoop` — wrong class, method undefined
3. `OboeEngine::dispatchLoop()` declared but never implemented
4. `AudioGraph` line 4 creates `OboeEngine()` with no args, but constructor requires `shared_ptr<MidiEngine>`
5. `jni_bridge.cpp` line 133 creates `OboeEngine()` with no args
6. `UICallback::stop()` and `UICallback::onMidiEvent()` declared but not implemented

## Target Architecture

```
AudioGraph
├── shared_ptr<MidiEngine>
│   ├── UICallback* (non-owning)
│   └── mEventQueue (SpscRing)
├── OboeOutput (wraps MidiEngine for audio)
└── InstrumentRepository
```

**Data flow**: `MidiEngine.pollMidi()` → pushes to `mEventQueue` → calls `mUICallback->onMidiEvent(evt)` → UICallback pushes to its own ring → UICallback dispatch thread drains ring → JNI callbacks to Java.

## Step-by-Step Changes

### 1. Finish `UICallback.h/.cpp`

- Add `setCallback(JNIEnv*, jobject)` method to capture JNI callback info (JavaVM*, jobject, method IDs)
- Implement `onMidiEvent(MidiEvt)` — push into `mEventQueue` (producer side)
- Implement `stop()` — set `mDispatchRunning = false`, join thread, clean up JNI refs
- `start()` already implemented — spawns thread that drains ring and fires JNI callbacks

### 2. Update `MidiEngine.h/.cpp` — Delegate dispatch to UICallback

- Add `UICallback* mUICallback = nullptr` member (non-owning)
- Add `void setUICallback(UICallback*)` method
- In `pollMidi()`, after pushing to `mEventQueue`, also call `if (mUICallback) mUICallback->onMidiEvent(evt)`
- Fix `setOutputPort()` — remove recursive self-call, call `mUICallback->setCallback(env, jCallback)` then `mUICallback->start()`
- Fix `clearOutputPort()` — call `mUICallback->stop()`
- **Remove** these members (all moved to UICallback):
  - `mDispatchThread`, `mDispatchRunning`
  - `mJvm`, `mMidiCallback`, `mOnMidiEventId`, `mOnLoopStateId`

### 3. Rename `OboeEngine` → `OboeOutput`

- Rename files: `outputs/OboeEngine.h` → `outputs/OboeOutput.h`, same for `.cpp`
- Rename class `OboeEngine` → `OboeOutput` everywhere
- Remove `dispatchLoop()` declaration from header (UICallback handles it now)
- Keep `AudioStreamDataCallback` inheritance, `start()`/`stop()`/`onAudioReady()` unchanged

### 4. Update `WifiEngine.h/.cpp` — Wire UICallback

- `start()` — call `mUICallback->start()` after spawning UDP thread
- `stop()` — call `mUICallback->stop()` before joining thread
- No other changes needed (inherits MidiEngine's `pollMidi()` which already calls UICallback)

### 5. Update `AudioGraph.h/.cpp` — Own and wire everything

- Add `std::shared_ptr<MidiEngine> mMidiEngine` member
- Add `std::unique_ptr<UICallback> mUICallback` member
- Constructor:
  1. Create `MidiEngine`
  2. Create `UICallback`
  3. Wire: `mMidiEngine->setUICallback(mUICallback.get())`
  4. Create `OboeOutput(mMidiEngine)`
- `setEngine()`: stop old engine, stop UICallback, create new engine, rewire UICallback, start UICallback if MIDI port open

### 6. Update `jni_bridge.cpp` — Fix references

- Update `OboeEngine` → `OboeOutput` in includes and construction
- Fix `setEngine()` to construct `OboeOutput` with proper `shared_ptr<MidiEngine>`
- `AudioGraph` constructor now handles wiring internally

### 7. Build and verify

```bash
./gradlew assembleDebug
```
