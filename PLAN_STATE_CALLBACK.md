# Plan: LoopRecorder State → UI Event Flow

## Problem

`LoopRecorder` state can change **internally on the audio thread** via MIDI:
- CC95 received → `startRecordOnPlay()` → state becomes `StartRecordOnPlay`
- First MIDI note while armed → state becomes `Recording`
- CC93 received → `stopRecording()` → state becomes `Playing`

`MainViewModel` only updates `loopState` on explicit button presses, so these
MIDI-triggered transitions are invisible to the UI.

---

## Approach: Sentinel Event via Existing SpscRing → dispatchLoop → JNI

Piggyback on the existing path:

```
audio thread
  → SpscRing<MidiEvt>
  → dispatchLoop()
  → JNI callback (onMidiEvent / onLoopState)
  → MidiRouter
  → SharedFlow
  → MainViewModel
```

Use a **sentinel MidiEvt** (`channel = 0xFF`, `type = LoopRecorder::State`) to
carry loop state changes through the same ring buffer, without adding a second
thread or ring buffer.

---

## Step-by-Step Changes

### Step 1 — `LoopRecorder.h / .cpp`: add state-change callback

Add a public callback field:

```cpp
std::function<void(State)> onStateChange;
```

Call it at every `mState.store(...)` site, including the MIDI-triggered
transitions inside `onMidiEvent()`.

```cpp
// example in stopRecording()
mState.store(State::Playing, std::memory_order_release);
if (onStateChange) onStateChange(State::Playing);
```

### Step 2 — `ChannelEngine.h / .cpp`: wire callback + loop-state JNI method ID

**ChannelEngine.h** — add field:
```cpp
jmethodID mOnLoopStateId {nullptr};
```

**ChannelEngine.cpp** constructor — wire the callback:
```cpp
mLoopRecorder.onStateChange = [this](LoopRecorder::State s) {
    mEventQueue.push({0xFF, static_cast<uint8_t>(s), 0, 0});
};
```

**`setOutputPort()`** — look up the second method on the same callback object:
```cpp
mOnLoopStateId = env->GetMethodID(cls, "onLoopState", "(I)V");
```

**`clearOutputPort()`** — reset:
```cpp
mOnLoopStateId = nullptr;
```

### Step 3 — `OboeEngine::dispatchLoop()`: handle sentinel

```cpp
while (mEventQueue.pop(evt)) {
    any = true;
    if (evt.channel == 0xFF) {
        if (env && mMidiCallback && mOnLoopStateId)
            env->CallVoidMethod(mMidiCallback, mOnLoopStateId, (jint)evt.type);
    } else {
        if (env && mMidiCallback && mOnMidiEventId)
            env->CallVoidMethod(mMidiCallback, mOnMidiEventId,
                                (jint)evt.channel, (jint)evt.type,
                                (jint)evt.data1,   (jint)evt.data2);
    }
}
```

### Step 4 — `MidiEventListener` (Kotlin): add `onLoopState`

Convert from `fun interface` to a regular interface and add a default no-op:

```kotlin
interface MidiEventListener {
    fun onMidiEvent(channel: Int, type: Int, data1: Int, data2: Int)
    fun onLoopState(state: Int) {}
}
```

### Step 5 — `MidiRouter.kt`: emit loop state events

```kotlin
private val _loopState = MutableSharedFlow<Int>(extraBufferCapacity = 8)
val loopStateEvents: SharedFlow<Int> = _loopState.asSharedFlow()

override fun onLoopState(state: Int) { _loopState.tryEmit(state) }
```

### Step 6 — `LoopState` enum: add `Armed`

C++ has 4 states (`Idle=0`, `Recording=1`, `Playing=2`, `StartRecordOnPlay=3`).
Kotlin currently only has 3. Add `Armed` to match:

```kotlin
enum class LoopState { Idle, Recording, Playing, Armed }
```

Update `LoopControls.kt`:
- Show label `"armed…"` when `loopState == LoopState.Armed`
- REC button enabled when `loopState` is `Idle` or `Armed` (not `Playing`)
- STOP button enabled when `loopState` is `Recording` or `Armed`

### Step 7 — `MainViewModel`: collect loop state flow + fix `loopRecord()`

**Collect events in `init`:**

```kotlin
viewModelScope.launch {
    midiRouter.loopStateEvents.collect { state ->
        val ls = when (state) {
            1    -> LoopState.Recording
            2    -> LoopState.Playing
            3    -> LoopState.Armed
            else -> LoopState.Idle
        }
        _uiState.value = _uiState.value.copy(loopState = ls)
    }
}
```

**Fix `loopRecord()`** — C++ sets `StartRecordOnPlay` (armed), not `Recording`:

```kotlin
fun loopRecord() {
    NativeAudioEngine.loopStartRecord()
    _uiState.value = _uiState.value.copy(loopState = LoopState.Armed, loopLengthSec = 0f)
}
```

---

## Why the sentinel is safe for the audio thread

`mEventQueue.push()` is a lock-free, non-blocking SpscRing write — safe to call
from the Oboe audio callback thread. Loop state transitions are infrequent
(user-scale events), so the 256-slot ring will never be full.

## Scope of MIDI-triggered vs UI-triggered transitions

The sentinel path only delivers events when a BLE device is connected (that is
the only time `dispatchLoop` drains the ring and MIDI input can arrive). For
UI-button-triggered transitions (no BLE device), `MainViewModel` already updates
`loopState` synchronously on the button press — these will also receive a
redundant sentinel confirmation from the ring, which is harmless.

---

## File Checklist

| File | Change |
|------|--------|
| `app/src/main/cpp/LoopRecorder.h` | add `onStateChange` field |
| `app/src/main/cpp/LoopRecorder.cpp` | call `onStateChange` at every `mState.store` |
| `app/src/main/cpp/ChannelEngine.h` | add `mOnLoopStateId` field |
| `app/src/main/cpp/ChannelEngine.cpp` | wire callback, look up JNI method, reset on clear |
| `app/src/main/cpp/OboeEngine.cpp` | sentinel check in `dispatchLoop()` |
| `app/src/main/java/.../synth/NativeAudioEngine.kt` | convert to regular interface, add `onLoopState` |
| `app/src/main/java/.../midi/MidiRouter.kt` | add `loopStateEvents` flow, implement `onLoopState` |
| `app/src/main/java/.../MainViewModel.kt` | collect `loopStateEvents`, fix `loopRecord()` |
| `app/src/main/java/.../ui/LoopControls.kt` | handle `Armed` state in labels and button enable logic |
