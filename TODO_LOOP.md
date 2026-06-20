# Loop Engine — Detailed Plan

## Overview

Record MIDI events on channel 9 (drums only) with frame-accurate timestamps, then
replay them in a repeating loop while the user plays piano on top.

**Constraints:**
- Records channel 9 only (drum hits from pad UI or BLE MIDI)
- Replace mode: starting a new recording always clears the previous loop
- Both OboeEngine and WifiEngine get the loop (shared via ChannelEngine)
- `kSampleRate = 48000` (AudioConfig.h)

## User workflow

1. Tap **REC** → play drum pads → tap **STOP** → drum loop starts automatically
2. Tap keyboard sound selector to switch to Piano (or any other voice)
3. Play piano freely over the looping drums
4. Tap **CLEAR** (or **REC** again) to erase the loop

---

## Checklist

- [ ] Step 1 — C++: Create `LoopRecorder.h`
- [ ] Step 2 — C++: Create `LoopRecorder.cpp`
- [ ] Step 3 — C++: Add virtual loop methods to `AudioEngine.h`
- [ ] Step 4 — C++: Add loop methods + `mLoopRecorder` to `ChannelEngine.h`
- [ ] Step 5 — C++: Implement loop methods in `ChannelEngine.cpp`
- [ ] Step 6 — C++: Call `advanceLoop()` in `OboeEngine.cpp`
- [ ] Step 7 — C++: Call `advanceLoop()` in `WifiEngine.cpp`
- [ ] Step 8 — C++: Add loop methods to `AudioGraph.h`
- [ ] Step 9 — C++: Implement loop methods in `AudioGraph.cpp`
- [ ] Step 10 — C++: Add four JNI functions to `jni_bridge.cpp`
- [ ] Step 11 — Kotlin: Add four externals to `NativeAudioEngine.kt`
- [ ] Step 12 — Kotlin: Add `LoopState` enum + `UiState` fields + ViewModel actions
- [ ] Step 13 — UI: Create `LoopControls.kt`
- [ ] Step 14 — UI: Add loop row to `MainScreen.kt`
- [ ] Step 15 — UI: Wire callbacks in `MainActivity.kt`

---

## Step 1 — Create `LoopRecorder.h`

**New file:** `app/src/main/cpp/LoopRecorder.h`

```cpp
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <vector>

struct LoopEvent {
    int32_t frameOffset;
    uint8_t type;       // MidiMsgType value: 0x90 NoteOn, 0x80 NoteOff
    uint8_t note;
    uint8_t velocity;
};

class LoopRecorder {
public:
    enum class State { Idle = 0, Recording = 1, Playing = 2 };

    // Called from UI thread
    void startRecording();  // clears previous loop, enters Recording
    void stopRecording();   // seals loop length; audio thread transitions to Playing
    void clear();           // stops playback, discards events

    State state() const { return mState.load(std::memory_order_acquire); }

    // Called from audio thread once per render buffer
    // fire(type, note, velocity) is called for each event that falls in this window
    void advance(int32_t frames,
                 const std::function<void(uint8_t, uint8_t, uint8_t)>& fire);

    // Called from audio thread for each incoming channel-9 MIDI event
    void onMidiEvent(uint8_t type, uint8_t note, uint8_t velocity);

private:
    std::atomic<State> mState       { State::Idle };
    std::atomic<bool>  mShouldStop  { false };
    std::atomic<bool>  mShouldClear { false };

    // Audio thread only — no synchronisation needed
    std::vector<LoopEvent> mEvents;
    int32_t mRecordFrame { 0 };  // frame counter from recording start
    int32_t mLoopLength  { 0 };  // total loop frames; sealed on stopRecording
    int32_t mPlayFrame   { 0 };  // current playback position in [0, mLoopLength)
    size_t  mPlayIdx     { 0 };  // next mEvents index to fire
};
```

---

## Step 2 — Create `LoopRecorder.cpp`

**New file:** `app/src/main/cpp/LoopRecorder.cpp`

### 2a. UI-thread methods

```cpp
#include "LoopRecorder.h"

void LoopRecorder::startRecording() {
    mShouldStop.store(false,  std::memory_order_relaxed);
    mShouldClear.store(true,  std::memory_order_relaxed);
    mState.store(State::Recording, std::memory_order_release);
}

void LoopRecorder::stopRecording() {
    mShouldStop.store(true, std::memory_order_release);
}

void LoopRecorder::clear() {
    State s = mState.load(std::memory_order_acquire);
    if (s == State::Idle) {
        // No audio thread contention when Idle — safe to clear directly
        mEvents.clear();
        mLoopLength = mRecordFrame = mPlayFrame = 0;
        mPlayIdx = 0;
        mShouldClear.store(false, std::memory_order_relaxed);
    } else {
        // Recording or Playing: signal audio thread to clear and go Idle
        mShouldClear.store(true, std::memory_order_release);
    }
}
```

### 2b. `onMidiEvent` — records ch9 events during Recording

```cpp
void LoopRecorder::onMidiEvent(uint8_t type, uint8_t note, uint8_t velocity) {
    if (mState.load(std::memory_order_acquire) != State::Recording) return;
    if (mShouldStop.load(std::memory_order_acquire))  return;
    if (mShouldClear.load(std::memory_order_acquire)) return;
    mEvents.push_back({ mRecordFrame, type, note, velocity });
}
```

`mRecordFrame` holds the frame offset of the start of the current render buffer, so
all events polled in the same `pollMidi()` call get the same (buffer-boundary) timestamp.
This is accurate enough for drum patterns.

### 2c. `advance` — drives recording frame counter and fires playback events

```cpp
void LoopRecorder::advance(int32_t frames,
                           const std::function<void(uint8_t,uint8_t,uint8_t)>& fire) {
    State s = mState.load(std::memory_order_acquire);

    // ── Recording branch ──────────────────────────────────────────────────────
    if (s == State::Recording) {
        if (mShouldClear.load(std::memory_order_acquire)) {
            mEvents.clear();
            mRecordFrame = 0;
            mShouldClear.store(false, std::memory_order_release);
        }
        if (mShouldStop.load(std::memory_order_acquire)) {
            mLoopLength = mRecordFrame;
            mPlayFrame  = 0;
            mPlayIdx    = 0;
            mShouldStop.store(false, std::memory_order_release);
            State next = (!mEvents.empty() && mLoopLength > 0)
                         ? State::Playing : State::Idle;
            mState.store(next, std::memory_order_release);
        } else {
            mRecordFrame += frames;
        }
        return;
    }

    // ── Playing branch ────────────────────────────────────────────────────────
    if (s == State::Playing) {
        if (mShouldClear.load(std::memory_order_acquire)) {
            mEvents.clear();
            mLoopLength = mRecordFrame = mPlayFrame = 0;
            mPlayIdx = 0;
            mShouldClear.store(false, std::memory_order_release);
            mState.store(State::Idle, std::memory_order_release);
            return;
        }

        int32_t playEnd = mPlayFrame + frames;

        if (playEnd < mLoopLength) {
            // No wrap — fire events in [mPlayFrame, playEnd)
            while (mPlayIdx < mEvents.size() &&
                   mEvents[mPlayIdx].frameOffset < playEnd) {
                const auto& e = mEvents[mPlayIdx++];
                fire(e.type, e.note, e.velocity);
            }
            mPlayFrame = playEnd;
        } else {
            // Loop wraps: fire events in [mPlayFrame, mLoopLength) then [0, overflow)
            while (mPlayIdx < mEvents.size()) {
                const auto& e = mEvents[mPlayIdx++];
                fire(e.type, e.note, e.velocity);
            }
            int32_t overflow = playEnd - mLoopLength;
            mPlayFrame = overflow;
            mPlayIdx   = 0;
            while (mPlayIdx < mEvents.size() &&
                   mEvents[mPlayIdx].frameOffset < overflow) {
                const auto& e = mEvents[mPlayIdx++];
                fire(e.type, e.note, e.velocity);
            }
        }
    }
    // Idle: do nothing
}
```

---

## Step 3 — Add virtual loop methods to `AudioEngine.h`

**File:** `app/src/main/cpp/AudioEngine.h`

After the existing `setDrumBackend` virtual method, add:

```cpp
    virtual void loopStartRecord() {}
    virtual void loopStopRecord()  {}
    virtual void loopClear()       {}
    virtual int  loopState()       { return 0; }
```

---

## Step 4 — Add loop methods + member to `ChannelEngine.h`

**File:** `app/src/main/cpp/ChannelEngine.h`

### 4a. Add include at the top (after `"SpscRing.h"`)

```cpp
#include "LoopRecorder.h"
```

### 4b. Add public overrides (after `controlChange` declaration)

```cpp
    void loopStartRecord() override;
    void loopStopRecord()  override;
    void loopClear()       override;
    int  loopState()       override;
```

### 4c. Add protected helper and member (in the `protected:` block, after `mEventQueue`)

```cpp
    // Fire playback events for this buffer, then advance the loop position.
    // Must be called from the audio thread between pollMidi() and renderChannels().
    void advanceLoop(int32_t frames);

    LoopRecorder mLoopRecorder;
```

---

## Step 5 — Implement loop methods in `ChannelEngine.cpp`

**File:** `app/src/main/cpp/ChannelEngine.cpp`

Add at the bottom of the file:

```cpp
void ChannelEngine::loopStartRecord() { mLoopRecorder.startRecording(); }
void ChannelEngine::loopStopRecord()  { mLoopRecorder.stopRecording();  }
void ChannelEngine::loopClear()       { mLoopRecorder.clear();          }
int  ChannelEngine::loopState()       { return static_cast<int>(mLoopRecorder.state()); }

void ChannelEngine::advanceLoop(int32_t frames) {
    mLoopRecorder.advance(frames, [this](uint8_t type, uint8_t note, uint8_t vel) {
        Instrument* inst = mChannels[9].load(std::memory_order_acquire);
        if (!inst) return;
        if (type == 0x90 && vel > 0)
            inst->noteOn(9, note, vel);
        else
            inst->noteOff(9, note);
    });
}
```

Also update `pollMidi()` to forward ch9 events to the recorder. In the `for` loop
over parsed messages, after routing to `noteOn`/`noteOff`/`controlChange`, add:

```cpp
        for (int k = 0; k < count; ++k) {
            const MidiMsg& m = msgs[k];
            if (m.type == MidiMsgType::NoteOn)
                noteOn(m.channel, m.data1, m.data2);
            else if (m.type == MidiMsgType::NoteOff)
                noteOff(m.channel, m.data1);
            else if (m.type == MidiMsgType::CC)
                controlChange(m.channel, m.data1, m.data2);

            // Record ch9 events for the loop
            if (m.channel == 9 &&
                (m.type == MidiMsgType::NoteOn || m.type == MidiMsgType::NoteOff)) {
                mLoopRecorder.onMidiEvent(
                    static_cast<uint8_t>(m.type), m.data1, m.data2);
            }

            mEventQueue.push({ m.channel, static_cast<uint8_t>(m.type),
                               m.data1, m.data2 });
        }
```

---

## Step 6 — Call `advanceLoop()` in `OboeEngine.cpp`

**File:** `app/src/main/cpp/OboeEngine.cpp`

In `onAudioReady`, insert `advanceLoop` between `pollMidi` and `renderChannels`:

```cpp
oboe::DataCallbackResult OboeEngine::onAudioReady(
        oboe::AudioStream*, void* audioData, int32_t numFrames) {
    pollMidi();
    advanceLoop(numFrames);           // ← add this line

    auto* buf = static_cast<float*>(audioData);
    for (int i = 0; i < numFrames; ++i) buf[i] = 0.0f;
    renderChannels(buf, numFrames);

    return oboe::DataCallbackResult::Continue;
}
```

---

## Step 7 — Call `advanceLoop()` in `WifiEngine.cpp`

**File:** `app/src/main/cpp/WifiEngine.cpp`

In `udpRenderLoop`, insert `advanceLoop` between `pollMidi` and `renderChannels`:

```cpp
        pollMidi();
        advanceLoop(kFramesPerBuf);   // ← add this line

        for (float& s : buf) s = 0.0f;
        renderChannels(buf, kFramesPerBuf);
```

---

## Step 8 — Add loop methods to `AudioGraph.h`

**File:** `app/src/main/cpp/AudioGraph.h`

After `closeMidiDevice()` declaration, add:

```cpp
    void loopStartRecord();
    void loopStopRecord();
    void loopClear();
    int  loopState();
```

---

## Step 9 — Implement loop methods in `AudioGraph.cpp`

**File:** `app/src/main/cpp/AudioGraph.cpp`

Add at the bottom:

```cpp
void AudioGraph::loopStartRecord() { mEngine->loopStartRecord(); }
void AudioGraph::loopStopRecord()  { mEngine->loopStopRecord();  }
void AudioGraph::loopClear()       { mEngine->loopClear();       }
int  AudioGraph::loopState()       { return mEngine->loopState(); }
```

---

## Step 10 — Add four JNI functions to `jni_bridge.cpp`

**File:** `app/src/main/cpp/jni_bridge.cpp`

Add inside `extern "C" {` before the closing `}`:

```cpp
JNIEXPORT void JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_loopStartRecord(
        JNIEnv *, jobject, jlong ptr) {
    GRAPH(ptr)->loopStartRecord();
}

JNIEXPORT void JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_loopStopRecord(
        JNIEnv *, jobject, jlong ptr) {
    GRAPH(ptr)->loopStopRecord();
}

JNIEXPORT void JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_loopClear(
        JNIEnv *, jobject, jlong ptr) {
    GRAPH(ptr)->loopClear();
}

JNIEXPORT jint JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_loopState(
        JNIEnv *, jobject, jlong ptr) {
    return static_cast<jint>(GRAPH(ptr)->loopState());
}
```

---

## Step 11 — Add four externals to `NativeAudioEngine.kt`

**File:** `app/src/main/java/org/gilbertxenodike/btmid/synth/NativeAudioEngine.kt`

### 11a. Public API (after `clearOutputPort()`)

```kotlin
    fun loopStartRecord() = loopStartRecord(ptr)
    fun loopStopRecord()  = loopStopRecord(ptr)
    fun loopClear()       = loopClear(ptr)
    fun loopState(): Int  = loopState(ptr)
```

### 11b. Private externals (after `clearOutputPort` external)

```kotlin
    private external fun loopStartRecord(ptr: Long)
    private external fun loopStopRecord(ptr: Long)
    private external fun loopClear(ptr: Long)
    private external fun loopState(ptr: Long): Int
```

---

## Step 12 — ViewModel: `LoopState` enum, `UiState` fields, actions

**File:** `app/src/main/java/org/gilbertxenodike/btmid/MainViewModel.kt`

### 12a. Add `LoopState` enum (after `KeyboardSound` enum)

```kotlin
enum class LoopState { Idle, Recording, Playing }
```

### 12b. Add two fields to `UiState`

```kotlin
data class UiState(
    // ... existing fields ...
    val loopState: LoopState = LoopState.Idle,
    val loopLengthSec: Float = 0f,   // displayed as "2.4 s" once Playing
)
```

### 12c. Add three ViewModel actions (after `setKeyboardSound`)

```kotlin
fun loopRecord() {
    NativeAudioEngine.loopStartRecord()
    _uiState.value = _uiState.value.copy(loopState = LoopState.Recording, loopLengthSec = 0f)
}

fun loopStop() {
    NativeAudioEngine.loopStopRecord()
    // Exact loop length is not tracked in Kotlin — we poll loopState once to confirm.
    // The C++ engine transitions atomically; update UI state optimistically.
    _uiState.value = _uiState.value.copy(loopState = LoopState.Playing)
}

fun loopClear() {
    NativeAudioEngine.loopClear()
    _uiState.value = _uiState.value.copy(loopState = LoopState.Idle, loopLengthSec = 0f)
}
```

Note: `loopLengthSec` is not strictly required; it can be omitted in v1 if the UI only shows
the loop state label. Add it later if the user wants a duration display.

---

## Step 13 — Create `LoopControls.kt`

**New file:** `app/src/main/java/org/gilbertxenodike/btmid/ui/LoopControls.kt`

```kotlin
package org.gilbertxenodike.btmid.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import org.gilbertxenodike.btmid.LoopState
import org.gilbertxenodike.btmid.ui.theme.BtmidTheme

@Composable
fun LoopControls(
    loopState: LoopState,
    onRecord: () -> Unit,
    onStop: () -> Unit,
    onClear: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(
        modifier = modifier,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        // REC button — red when recording
        Button(
            onClick = onRecord,
            colors = ButtonDefaults.buttonColors(
                containerColor = if (loopState == LoopState.Recording)
                    MaterialTheme.colorScheme.error
                else
                    MaterialTheme.colorScheme.primary
            ),
            enabled = loopState != LoopState.Playing,
        ) {
            Text(if (loopState == LoopState.Recording) "● REC" else "REC")
        }

        // STOP button — only meaningful while Recording
        Button(
            onClick = onStop,
            enabled = loopState == LoopState.Recording,
        ) {
            Text("STOP")
        }

        // CLEAR button — only meaningful while Playing
        OutlinedButton(
            onClick = onClear,
            enabled = loopState == LoopState.Playing,
        ) {
            Text("CLEAR")
        }

        // State label
        val label = when (loopState) {
            LoopState.Idle      -> ""
            LoopState.Recording -> "recording…"
            LoopState.Playing   -> "looping"
        }
        if (label.isNotEmpty()) {
            Text(
                text = label,
                style = MaterialTheme.typography.labelSmall,
                modifier = Modifier.padding(start = 4.dp),
            )
        }
    }
}

@Preview(showBackground = true)
@Composable
private fun LoopControlsIdlePreview() {
    BtmidTheme { LoopControls(LoopState.Idle, {}, {}, {}) }
}

@Preview(showBackground = true)
@Composable
private fun LoopControlsRecordingPreview() {
    BtmidTheme { LoopControls(LoopState.Recording, {}, {}, {}) }
}

@Preview(showBackground = true)
@Composable
private fun LoopControlsPlayingPreview() {
    BtmidTheme { LoopControls(LoopState.Playing, {}, {}, {}) }
}
```

---

## Step 14 — Add loop row to `MainScreen.kt`

**File:** `app/src/main/java/org/gilbertxenodike/btmid/ui/MainScreen.kt`

### 14a. Add three callback parameters to `MainScreen`

```kotlin
@Composable
fun MainScreen(
    uiState: UiState,
    onGrantPermissions: () -> Unit,
    onStartScan: () -> Unit,
    onStopScan: () -> Unit,
    onConnect: (DeviceUiState) -> Unit,
    onDisconnect: () -> Unit,
    onSetDrumBackend: (DrumBackend) -> Unit,
    onSetKeyboardSound: (KeyboardSound) -> Unit,
    showSelectEngineDialog: (Boolean) -> Unit,
    onSelectEngine: (AudioEngine) -> Unit,
    onLoopRecord: () -> Unit,      // ← add
    onLoopStop: () -> Unit,        // ← add
    onLoopClear: () -> Unit,       // ← add
    modifier: Modifier = Modifier,
)
```

Add import:
```kotlin
import org.gilbertxenodike.btmid.LoopState
```

### 14b. Add `LoopControls` row above `DrumEngineSelector`

Find where `DrumEngineSelector` is called in the body and insert before it:

```kotlin
        LoopControls(
            loopState = uiState.loopState,
            onRecord  = onLoopRecord,
            onStop    = onLoopStop,
            onClear   = onLoopClear,
        )
```

### 14c. Update all `@Preview` composables

Add the three new parameters to each `MainScreen(...)` preview call:

```kotlin
    onLoopRecord = {},
    onLoopStop   = {},
    onLoopClear  = {},
```

---

## Step 15 — Wire callbacks in `MainActivity.kt`

**File:** `app/src/main/java/org/gilbertxenodike/btmid/MainActivity.kt`

In the `MainScreen(...)` call inside `setContent { ... }`, add:

```kotlin
    onLoopRecord = { viewModel.loopRecord() },
    onLoopStop   = { viewModel.loopStop()   },
    onLoopClear  = { viewModel.loopClear()  },
```

---

## Also wire on-screen drum pad to the recorder

The DrumTrigger and PianoKeyboard call `NativeAudioEngine.noteOn/Off` directly (bypassing
`pollMidi`). So ch9 noteOn/Off from the **pad UI** never reaches `pollMidi` and won't be
recorded by the current design.

**Fix:** in `AudioGraph::noteOn` / `noteOff`, also forward ch9 events to the loop recorder:

```cpp
// AudioGraph.cpp
void AudioGraph::noteOn(int ch, int note, int vel) {
    mEngine->noteOn(ch, note, vel);
    if (ch == 9) mEngine->loopRecordEvent(0x90, note, vel);   // ch9 UI hit
}

void AudioGraph::noteOff(int ch, int note) {
    mEngine->noteOff(ch, note);
    if (ch == 9) mEngine->loopRecordEvent(0x80, note, 0);
}
```

Add a new virtual method to `AudioEngine.h`:

```cpp
virtual void loopRecordEvent(uint8_t type, uint8_t note, uint8_t vel) {}
```

And implement in `ChannelEngine.cpp`:

```cpp
void ChannelEngine::loopRecordEvent(uint8_t type, uint8_t note, uint8_t vel) {
    mLoopRecorder.onMidiEvent(type, note, vel);
}
```

Add declaration to `ChannelEngine.h`:

```cpp
    void loopRecordEvent(uint8_t type, uint8_t note, uint8_t vel) override;
```

And to `AudioGraph.h`:

```cpp
    void loopRecordEvent(uint8_t type, uint8_t note, uint8_t vel);
```

**Thread-safety note:** `loopRecordEvent` is called from the UI thread (via JNI noteOn/Off),
while `advance()` and the recording `mRecordFrame` counter are on the audio thread. The
`onMidiEvent` call checks `mState` atomically and then writes to `mEvents` — which is **audio
thread only** during recording. This is a data race.

**Safe fix:** give `LoopRecorder` a small `SpscRing` for UI-thread hits, drained in `advance()`:

```cpp
// LoopRecorder.h — add
#include "SpscRing.h"

struct PendingLoopEvent { uint8_t type; uint8_t note; uint8_t vel; };
SpscRing<PendingLoopEvent, 64> mUiEventQueue;   // UI→audio transfer
```

```cpp
// LoopRecorder.cpp — change loopRecordEvent path:
void LoopRecorder::onUiMidiEvent(uint8_t type, uint8_t note, uint8_t vel) {
    // Called from UI/JNI thread — push into ring; audio thread drains it in advance()
    mUiEventQueue.push({type, note, vel});
}
```

In `advance()`, at the top of the Recording branch (after the clear check):

```cpp
        PendingLoopEvent uiEv;
        while (mUiEventQueue.pop(uiEv)) {
            if (mState.load(std::memory_order_acquire) == State::Recording &&
                !mShouldStop.load(std::memory_order_acquire) &&
                !mShouldClear.load(std::memory_order_acquire)) {
                mEvents.push_back({ mRecordFrame, uiEv.type, uiEv.note, uiEv.vel });
            }
        }
```

Change `ChannelEngine::loopRecordEvent` to call `mLoopRecorder.onUiMidiEvent(...)` instead
of `onMidiEvent`.

---

## CMakeLists.txt

**File:** `app/src/main/cpp/CMakeLists.txt`

Add `LoopRecorder.cpp` to the `add_library` source list alongside the other `.cpp` files.

---

## Test plan

1. `./gradlew assembleDebug` — confirm clean build
2. Run app → confirm Loop controls row is visible above drum pads
3. Tap **REC** → confirm button turns red and label shows "recording…"
4. Tap several drum pads (kick, snare, hat) for a few seconds
5. Tap **STOP** → confirm label changes to "looping" and drums repeat
6. Switch keyboard sound to **Piano** → play piano → confirm drums continue looping
7. Tap **REC** again → confirm old loop is replaced (drums stop, new recording starts)
8. Tap **STOP** → new loop plays
9. Tap **CLEAR** → drums stop, controls return to Idle state
10. Connect a BLE MIDI device → play drums from external controller → confirm recorded and looped
11. Switch to WifiEngine → repeat steps 3–9 → confirm loop works over UDP
