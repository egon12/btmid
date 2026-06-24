# Plan: LoopRecorder State → UI Event Flow

## Problem

`LoopRecorder` state can change **internally on the audio thread** via MIDI:
- CC95 received → `startRecordOnPlay()` → state becomes `StartRecordOnPlay` (Armed)
- First MIDI note while armed → state becomes `Recording`
- CC93 received → `stopRecording()` → state becomes `Playing`

`MainViewModel` only updates `loopState` on explicit button presses, so these
MIDI-triggered transitions are invisible to the UI.

---

## Approach: Sentinel Event via Existing SpscRing → dispatchLoop → JNI

Piggyback on the existing path:

```
audio thread (or UI thread for onUiMidiEvent)
  → LoopRecorder state change
  → onStateChange callback
  → SpscRing<MidiEvt>.push() with sentinel (channel=0xFF)
  → dispatchLoop() drains ring
  → JNI callback onLoopState(state: Int)
  → MidiRouter.onLoopState()
  → SharedFlow<Int>
  → MainViewModel collects → updates UiState.loopState
```

Use a **sentinel MidiEvt** (`channel = 0xFF`, `type = LoopRecorder::State`) to
carry loop state changes through the same ring buffer, without adding a second
thread or ring buffer.

---

## Step-by-Step Changes

### Step 1 — `LoopRecorder.h`: add callback + `changeState` helper

**File:** `app/src/main/cpp/LoopRecorder.h`

#### 1a. Add public callback field

**Location:** After line 26 (inside `public:` section, after the `State` enum)

**Add this line:**
```cpp
    std::function<void(State)> onStateChange;
```

#### 1b. Add private `changeState` helper method

**Location:** Inside the `private:` section (after line 44, before `mState` declaration)

**Add this method:**
```cpp
    void changeState(State newState) {
        mState.store(newState, std::memory_order_release);
        if (onStateChange) onStateChange(newState);
    }
```

**Why inline is appropriate:** This is a 2-line function called from 6 sites. Defining
it in the class body makes it implicitly inline, avoiding function call overhead and
keeping the implementation visible for optimization.

---

### Step 2 — `LoopRecorder.cpp`: replace `mState.store()` with `changeState()`

**File:** `app/src/main/cpp/LoopRecorder.cpp`

There are **6 sites** where `mState.store()` is called. Replace each with `changeState()`.

#### Site 1: `startRecording()` — line 12
**Before:**
```cpp
    mState.store(State::Recording, std::memory_order_release);
```
**After:**
```cpp
    changeState(State::Recording);
```

#### Site 2: `stopRecording()` — line 19
**Before:**
```cpp
    mState.store(State::Playing, std::memory_order_release);
```
**After:**
```cpp
    changeState(State::Playing);
```

#### Site 3: `startRecordOnPlay()` — line 24
**Before:**
```cpp
    mState.store(State::StartRecordOnPlay, std::memory_order_release);
```
**After:**
```cpp
    changeState(State::StartRecordOnPlay);
```

#### Site 4: `clear()` — line 30
**Before:**
```cpp
    mState.store(State::Idle, std::memory_order_release);
```
**After:**
```cpp
    changeState(State::Idle);
```

#### Site 5: `onMidiEvent()` — line 47 (armed → recording on first note)
**Before:**
```cpp
        mState.store(State::Recording, std::memory_order_release);
```
**After:**
```cpp
        changeState(State::Recording);
```

#### Site 6: `onUiMidiEvent()` — line 69 (armed → recording on UI note)
**Before:**
```cpp
        mState.store(State::Recording, std::memory_order_release);
```
**After:**
```cpp
        changeState(State::Recording);
```

---

### Step 3 — `ChannelEngine.h`: add JNI method ID field

**File:** `app/src/main/cpp/ChannelEngine.h`  
**Location:** Line 54, after `jmethodID  mOnMidiEventId {nullptr};`

**Add this line:**
```cpp
    jmethodID  mOnLoopStateId  {nullptr};
```

**Note:** This field is `protected` (same section as `mOnMidiEventId`), so
`OboeEngine::dispatchLoop()` can access it.

---

### Step 4 — `ChannelEngine.cpp`: wire callback + JNI method lookup

**File:** `app/src/main/cpp/ChannelEngine.cpp`

#### 4a. Wire the callback in the constructor

**Location:** After line 16 (after `ChannelEngine() = default;` in the header,
but since the constructor is defaulted in the header, we need to define it in
the .cpp file).

**Actually, the constructor is defaulted in the header (line 16 of ChannelEngine.h):**
```cpp
    ChannelEngine() = default;
```

**Change it to:**
```cpp
    ChannelEngine();
```

**Then in `ChannelEngine.cpp`, add at the top (after line 7):**
```cpp
ChannelEngine::ChannelEngine() {
    mLoopRecorder.onStateChange = [this](LoopRecorder::State s) {
        mEventQueue.push({0xFF, static_cast<uint8_t>(s), 0, 0});
    };
}
```

#### 4b. Look up the JNI method in `setOutputPort()`

**Location:** Line 103 in `setOutputPort()`, after:
```cpp
    mOnMidiEventId = env->GetMethodID(cls, "onMidiEvent", "(IIII)V");
```

**Add this line:**
```cpp
    mOnLoopStateId = env->GetMethodID(cls, "onLoopState", "(I)V");
```

#### 4c. Reset the method ID in `clearOutputPort()`

**Location:** Line 158 in `clearOutputPort()`, after:
```cpp
    mOnMidiEventId = nullptr;
```

**Add this line:**
```cpp
    mOnLoopStateId = nullptr;
```

---

### Step 5 — `OboeEngine.cpp`: handle sentinel in `dispatchLoop()`

**File:** `app/src/main/cpp/OboeEngine.cpp`  
**Location:** Lines 68-74 in `dispatchLoop()`

**Before:**
```cpp
        while (mEventQueue.pop(evt)) {
            any = true;
            if (env && mMidiCallback && mOnMidiEventId) {
                env->CallVoidMethod(mMidiCallback, mOnMidiEventId,
                                   (jint)evt.channel, (jint)evt.type,
                                   (jint)evt.data1,   (jint)evt.data2);
            }
        }
```

**After:**
```cpp
        while (mEventQueue.pop(evt)) {
            any = true;
            if (evt.channel == 0xFF) {
                // Sentinel: loop state change
                if (env && mMidiCallback && mOnLoopStateId) {
                    env->CallVoidMethod(mMidiCallback, mOnLoopStateId,
                                       (jint)evt.type);
                }
            } else {
                // Normal MIDI event
                if (env && mMidiCallback && mOnMidiEventId) {
                    env->CallVoidMethod(mMidiCallback, mOnMidiEventId,
                                       (jint)evt.channel, (jint)evt.type,
                                       (jint)evt.data1,   (jint)evt.data2);
                }
            }
        }
```

---

### Step 6 — `NativeAudioEngine.kt`: convert interface + add `onLoopState`

**File:** `app/src/main/java/org/gilbertxenodike/btmid/synth/NativeAudioEngine.kt`  
**Location:** Lines 13-15

**Before:**
```kotlin
    fun interface MidiEventListener {
        fun onMidiEvent(channel: Int, type: Int, data1: Int, data2: Int)
    }
```

**After:**
```kotlin
    interface MidiEventListener {
        fun onMidiEvent(channel: Int, type: Int, data1: Int, data2: Int)
        fun onLoopState(state: Int) {}
    }
```

**Note:** Converting from `fun interface` to `interface` is safe because the only
implementation is `MidiRouter`, which already uses the `override` keyword.

---

### Step 7 — `MidiRouter.kt`: emit loop state events

**File:** `app/src/main/java/org/gilbertxenodike/btmid/midi/MidiRouter.kt`

#### 7a. Add the loop state flow

**Location:** After line 10 (after `val events: SharedFlow<MidiEvent> = _events.asSharedFlow()`)

**Add these lines:**
```kotlin
    private val _loopState = MutableSharedFlow<Int>(extraBufferCapacity = 8)
    val loopStateEvents: SharedFlow<Int> = _loopState.asSharedFlow()
```

#### 7b. Implement `onLoopState`

**Location:** After line 21 (after the closing brace of `onMidiEvent`)

**Add this method:**
```kotlin
    override fun onLoopState(state: Int) {
        _loopState.tryEmit(state)
    }
```

---

### Step 8 — `MainViewModel.kt`: add `Armed` state + collect loop events

**File:** `app/src/main/java/org/gilbertxenodike/btmid/MainViewModel.kt`

#### 8a. Add `Armed` to the enum

**Location:** Line 31

**Before:**
```kotlin
enum class LoopState { Idle, Recording, Playing }
```

**After:**
```kotlin
enum class LoopState { Idle, Recording, Playing, Armed }
```

#### 8b. Collect loop state events in `init`

**Location:** After line 107 (after the `midiRouter.events.collect` block)

**Add this block:**
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

#### 8c. Fix `loopRecord()` to set `Armed` instead of `Recording`

**Location:** Lines 214-217

**Before:**
```kotlin
    fun loopRecord() {
        NativeAudioEngine.loopStartRecord()
        _uiState.value = _uiState.value.copy(loopState = LoopState.Recording, loopLengthSec = 0f)
    }
```

**After:**
```kotlin
    fun loopRecord() {
        NativeAudioEngine.loopStartRecord()
        _uiState.value = _uiState.value.copy(loopState = LoopState.Armed, loopLengthSec = 0f)
    }
```

---

### Step 9 — `LoopControls.kt`: handle `Armed` state in UI

**File:** `app/src/main/java/org/gilbertxenodike/btmid/ui/LoopControls.kt`

#### 9a. Update REC button enabled logic

**Location:** Line 40

**Before:**
```kotlin
            enabled = loopState != LoopState.Playing,
```

**After:**
```kotlin
            enabled = loopState == LoopState.Idle || loopState == LoopState.Armed,
```

#### 9b. Update REC button label logic

**Location:** Line 42

**Before:**
```kotlin
            Text(if (loopState == LoopState.Recording) "\u25CF REC" else "REC")
```

**After:**
```kotlin
            Text(
                when (loopState) {
                    LoopState.Recording -> "\u25CF REC"
                    LoopState.Armed     -> "\u25CF ARMED"
                    else                -> "REC"
                }
            )
```

#### 9c. Update STOP button enabled logic

**Location:** Line 47

**Before:**
```kotlin
            enabled = loopState == LoopState.Recording,
```

**After:**
```kotlin
            enabled = loopState == LoopState.Recording || loopState == LoopState.Armed,
```

#### 9d. Update CLEAR button enabled logic

**Location:** Line 54

**Before:**
```kotlin
            enabled = loopState == LoopState.Playing,
```

**After:**
```kotlin
            enabled = loopState == LoopState.Playing || loopState == LoopState.Idle,
```

**Note:** CLEAR should also be enabled when `Idle` so the user can reset after
stopping playback. If you want to keep the original behavior (only enabled during
`Playing`), leave this unchanged.

#### 9e. Add `Armed` case to the label switch

**Location:** Lines 59-63

**Before:**
```kotlin
        val label = when (loopState) {
            LoopState.Idle      -> ""
            LoopState.Recording -> "recording\u2026"
            LoopState.Playing   -> "looping"
        }
```

**After:**
```kotlin
        val label = when (loopState) {
            LoopState.Idle      -> ""
            LoopState.Recording -> "recording\u2026"
            LoopState.Playing   -> "looping"
            LoopState.Armed     -> "armed\u2026"
        }
```

#### 9f. Add a preview for `Armed` state

**Location:** After line 90 (after the last `@Preview` function)

**Add this:**
```kotlin
@Preview(showBackground = true)
@Composable
private fun LoopControlsArmedPreview() {
    BtmidTheme { LoopControls(LoopState.Armed, {}, {}, {}) }
}
```

---

## Thread Safety Analysis

### Audio thread path (BLE MIDI)
```
Oboe audio callback (real-time thread)
  → pollMidi()
  → mLoopRecorder.onMidiEvent()
  → mState.store() + onStateChange callback
  → mEventQueue.push()  // lock-free, non-blocking SpscRing write — SAFE
```

### UI thread path (on-screen piano/drums)
```
UI thread
  → NativeAudioEngine.noteOn()
  → JNI → AudioGraph::noteOn()
  → mEngine->loopRecordEvent()
  → mLoopRecorder.onUiMidiEvent()
  → mState.store() + onStateChange callback
  → mEventQueue.push()  // lock-free, non-blocking SpscRing write — SAFE
```

### Dispatch thread path
```
dispatchLoop() (dedicated thread, started when BLE device connects)
  → mEventQueue.pop()  // lock-free SpscRing read — SAFE
  → JNI CallVoidMethod → MidiRouter.onLoopState()
  → SharedFlow.tryEmit()
  → MainViewModel collects on Main dispatcher
```

**Why it's safe:**
- `SpscRing::push()` is lock-free and non-blocking — safe from any thread
- Only one thread writes to the ring (audio thread OR UI thread, never concurrent)
- Only one thread reads from the ring (dispatch thread)
- Loop state transitions are infrequent (user-scale events), so the 256-slot ring
  will never overflow

---

## Known Limitations

### WifiEngine doesn't drain the event queue

`WifiEngine` inherits from `ChannelEngine` and uses `mEventQueue`, but it doesn't
have a `dispatchLoop()` thread. Events pushed to the ring during WifiEngine mode
will accumulate but never reach the UI.

**Impact:** Loop state changes triggered by BLE MIDI won't update the UI when
using WifiEngine.

**Mitigation options:**
1. **Accept the limitation:** Loop state changes are rare, and the user can
   manually check the state via `NativeAudioEngine.loopState()` if needed.
2. **Add a dispatch loop to WifiEngine:** Similar to `OboeEngine`, spawn a
   thread in `WifiEngine::start()` that drains `mEventQueue` and calls JNI.
3. **Move dispatch loop to ChannelEngine:** Make the dispatch thread a shared
   feature of `ChannelEngine` so both engines benefit.

**Recommendation:** Accept the limitation for now. If it becomes a problem,
implement option 3 (shared dispatch loop in `ChannelEngine`).

---

## File Checklist

| File | Lines Changed | Change Summary |
|------|---------------|----------------|
| `app/src/main/cpp/LoopRecorder.h` | 1 line added | Add `onStateChange` callback field |
| `app/src/main/cpp/LoopRecorder.cpp` | 6 sites modified | Call `onStateChange` after every `mState.store()` |
| `app/src/main/cpp/ChannelEngine.h` | 1 line added | Add `mOnLoopStateId` field |
| `app/src/main/cpp/ChannelEngine.cpp` | Constructor + 2 methods | Wire callback, look up JNI method, reset on clear |
| `app/src/main/cpp/OboeEngine.cpp` | `dispatchLoop()` | Handle sentinel (`channel == 0xFF`) |
| `app/src/main/java/.../synth/NativeAudioEngine.kt` | Interface definition | Convert to regular interface, add `onLoopState` |
| `app/src/main/java/.../midi/MidiRouter.kt` | 2 additions | Add `loopStateEvents` flow, implement `onLoopState` |
| `app/src/main/java/.../MainViewModel.kt` | Enum + `init` + `loopRecord()` | Add `Armed` state, collect events, fix button logic |
| `app/src/main/java/.../ui/LoopControls.kt` | 5 locations | Update button enable logic, labels, add preview |

---

## Testing Checklist

- [ ] Build succeeds: `./gradlew assembleDebug`
- [ ] Connect BLE MIDI device → send CC95 → UI shows "armed..."
- [ ] Play a note → UI shows "recording..."
- [ ] Send CC93 → UI shows "looping"
- [ ] Press REC button → UI shows "armed..."
- [ ] Press STOP button → UI shows "looping"
- [ ] Press CLEAR button → UI shows "" (Idle)
- [ ] Switch to WifiEngine → loop state changes don't crash (known limitation)
- [ ] Disconnect BLE device → reconnect → loop state is consistent
