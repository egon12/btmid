# Loop Engine v2 — Bug Fix Plan

Bugs found in the timestamp-based rewrite of `LoopRecorder`. Ordered by severity.

---

## Checklist

- [ ] Fix 1 — Data race: `clear()` mutates vectors on UI thread while audio thread reads them
- [ ] Fix 2 — Real-time safety: `storeRecord()` heap-allocates on the Oboe audio thread
- [x] Fix 3 — Timestamp truncation: `onUiMidiEvent` stores `tv_nsec` only, not full monotonic ns
- [x] Fix 4 — Wrong channel: `onUiMidiEvent` hardcodes `channel=0`; drum hits replay through piano
- [x] Fix 5 — `playedIndex` never updated when all events in window fire — events replay forever
- [ ] Fix 6 — `AudioGraph.h` still declares `loopRecordEvent` whose body was deleted from `.cpp`
- [x] Fix 7 — `current` member has no in-class initializer — UB on first `advance()` call
- [x] Fix 8 — `clear()` does not reset `playedIndex` or `current` — first pass after re-record is silent
- [ ] Fix 9 — Loop wrap does not fire overflow-region events in the same tick — hits drift late
- [ ] Fix 10 — `mEventsRecorded` not cleared before a new recording — stale events contaminate next loop

---

## Fix 1 — Data race on `clear()`

**File:** `app/src/main/cpp/LoopRecorder.cpp`

`clear()` is called from the JNI/UI thread (via `MidiEngine::loopClear()`). It calls
`mEventsRecorded.clear()` and `mEventsPlay.clear()` directly while the Oboe audio thread
may be inside `advance()` (iterating `mEventsPlay`) or `onMidiEvent()` (pushing to
`mEventsRecorded`). `std::vector` is not thread-safe; this is a data race and will crash.

The old design deferred clears to the audio thread using `mShouldClear`. Restore that pattern
so the audio thread performs the actual mutation:

```cpp
// LoopRecorder.h — restore
std::atomic<bool> mShouldClear { false };

// clear() — UI thread only sets the flag
void LoopRecorder::clear() {
    mShouldClear.store(true, std::memory_order_release);
}

// advance() — audio thread checks the flag and does the mutation
if (mShouldClear.load(std::memory_order_acquire)) {
    mEventsRecorded.clear();
    mEventsPlay.clear();
    mEndFrame = 0;
    current = 0;
    playedIndex = -1;
    mShouldClear.store(false, std::memory_order_release);
    mState.store(State::Idle, std::memory_order_release);
    return;
}
```

---

## Fix 2 — Heap allocation on Oboe audio thread

**File:** `app/src/main/cpp/LoopRecorder.cpp`

`play()` calls `storeRecord()` which does `mEventsPlay.clear()` +
`push_back()` in a loop — potentially triggering heap allocation. This path is reachable
from the audio thread when CC ch=0 cc=93 arrives via `onMidiEvent()` → `play()`.

Pre-reserve `mEventsPlay` to avoid reallocation:

```cpp
void LoopRecorder::storeRecord() {
    mEventsPlay.clear();
    mEventsPlay.reserve(mEventsRecorded.size());  // ← add this
    // ... rest unchanged
}
```

For a more thorough fix, do not call `storeRecord()` from the audio thread at
all — set a flag in `play()` and do the conversion on a dedicated worker thread.

---

## Fix 3 — `onUiMidiEvent` stores `tv_nsec` only

**File:** `app/src/main/cpp/LoopRecorder.cpp`

```cpp
// WRONG — tv_nsec is 0..999,999,999 only; wraps every second
mEventsRecorded.push_back({t.tv_nsec, MidiMsg{type, 0, note, vel}});

// CORRECT — full monotonic nanoseconds, same as rec()/play()
int64_t ts = t.tv_sec * 1000000000LL + t.tv_nsec;
mEventsRecorded.push_back({ts, MidiMsg{type, 0, note, vel}});
```

At runtime `mStartRecordNs` is in the trillions of nanoseconds. `tv_nsec` alone is at most
~10⁹, so `e.timestamp - start` massively underflows for any UI event, placing it at a
garbage frame. All UI-triggered drum hits become permanently silent during playback.

---

## Fix 4 — `onUiMidiEvent` hardcodes `channel=0`

**Files:** `app/src/main/cpp/LoopRecorder.cpp`, `MidiEngine.cpp`, `AudioGraph.cpp`

The channel (9 for drums) is dropped in the call chain. Thread it through:

```cpp
// AudioEngine.h — add ch parameter
virtual void loopRecordEvent(MidiMsgType type, int ch, uint8_t note, uint8_t vel) {}

// AudioGraph.cpp — pass ch
void AudioGraph::noteOn(int ch, int note, int vel) {
    mEngine->noteOn(ch, note, vel);
    if (ch == 9) mEngine->loopRecordEvent(MidiMsgType::NoteOn, ch, note, vel);
}
void AudioGraph::noteOff(int ch, int note) {
    mEngine->noteOff(ch, note);
    if (ch == 9) mEngine->loopRecordEvent(MidiMsgType::NoteOff, ch, note, 0);
}

// MidiEngine.cpp — forward ch
void MidiEngine::loopRecordEvent(MidiMsgType type, int ch, uint8_t note, uint8_t vel) {
    loopRecorder.onUiMidiEvent(type, static_cast<uint8_t>(ch), note, vel);
}

// LoopRecorder.cpp — use ch instead of hardcoded 0
void LoopRecorder::onUiMidiEvent(MidiMsgType type, uint8_t ch, uint8_t note, uint8_t vel) {
    timespec t{};
    clock_gettime(CLOCK_MONOTONIC, &t);
    int64_t ts = t.tv_sec * 1000000000LL + t.tv_nsec;
    mEventsRecorded.push_back({ts, MidiMsg{type, ch, note, vel}});
}
```

Without this fix, on-screen drum pad hits are recorded as channel 0 and replayed through
`mChannels[0]` (piano), producing silence or wrong pitched notes instead of drums.

---

## Fix 5 — `playedIndex` not updated when all events in window fire

**File:** `app/src/main/cpp/LoopRecorder.cpp` — `advance()`

The loop only writes `playedIndex` when it hits the first event *outside* the window
(`else if` branch). If all remaining events fall within `endFrame`, the loop exits without
updating `playedIndex`, and those same events re-fire on every subsequent `advance()` call.

```cpp
// Replace the advance() loop with:
int lastFired = playedIndex;
for (int i = playedIndex + 1; i < (int)mEventsPlay.size(); i++) {
    const auto& event = mEventsPlay[i];
    if (event.frame <= endFrame) {
        fire(event.msg);
        lastFired = i;
    } else {
        break;
    }
}
playedIndex = lastFired;
```

Also change the initialization in `LoopRecorder.h` from `playedIndex{0}` to
`playedIndex{-1}` so the very first `advance()` call starts at index 0, not index 1.

---

## Fix 6 — Stale `loopRecordEvent` declaration in `AudioGraph.h`

**File:** `app/src/main/cpp/AudioGraph.h`

The diff deleted `AudioGraph::loopRecordEvent` from `AudioGraph.cpp` but left the
declaration in `AudioGraph.h`. Any call site that uses it will fail at link time.

After applying Fix 4 (which re-adds a body for this method with the updated signature),
update the declaration in `AudioGraph.h` to match:

```cpp
void loopRecordEvent(MidiMsgType type, int ch, uint8_t note, uint8_t vel);
```

If Fix 4 is not applied and the method is truly dead, remove the declaration from
`AudioGraph.h` entirely to keep the header consistent with the `.cpp`.

---

## Fix 7 — `current` uninitialized

**File:** `app/src/main/cpp/LoopRecorder.h`

```cpp
// WRONG — no initializer; UB on first advance() call
int32_t current;

// CORRECT — matches the style of adjacent members
int32_t current{0};
```

Unlike `mEndFrame{0}` and `playedIndex{0}` on adjacent lines, `current` has no
in-class initializer. Default construction leaves it indeterminate. The first
`advance()` call reads it as `endFrame = current + frames` — undefined behavior.

---

## Fix 8 — `clear()` does not reset `playedIndex` or `current`

**File:** `app/src/main/cpp/LoopRecorder.cpp`

The deferred-clear branch inside `advance()` (from Fix 1) must reset all playback state:

```cpp
// In the deferred-clear branch inside advance():
mEventsRecorded.clear();
mEventsPlay.clear();
mEndFrame = 0;
current = 0;       // ← must reset
playedIndex = -1;  // ← must reset (not 0; see Fix 5)
```

Without this, after `clear()` + re-record + play, `playedIndex` from the previous session
remains (e.g. 7). `advance()` starts from index 8 — skipping all events 0–7 in the new
recording. The first playthrough is completely silent.

---

## Fix 9 — Loop wrap does not fire overflow-region events in the same tick

**File:** `app/src/main/cpp/LoopRecorder.cpp` — `advance()`

When `current > mEndFrame`, the code resets and sets `playedIndex = -1` but does NOT
fire events that fall in `[0, current % mEndFrame]`. They are deferred to the next tick,
arriving consistently ~5–10 ms late. Add a second pass after the wrap:

```cpp
current = endFrame;
if (current >= mEndFrame) {   // use >= not > to handle exact-boundary case
    current -= mEndFrame;
    playedIndex = -1;
    // Second pass: fire events in the wrapped region [0, current)
    for (int i = 0; i < (int)mEventsPlay.size(); i++) {
        const auto& event = mEventsPlay[i];
        if (event.frame < current) {
            fire(event.msg);
            playedIndex = i;
        } else {
            break;
        }
    }
}
```

---

## Fix 10 — `mEventsRecorded` not cleared before a new recording

**File:** `app/src/main/cpp/LoopRecorder.cpp`

Neither `rec()` nor the `Armed` branch clears `mEventsRecorded`.
If the user records, stops, then records again without `clear()`, old events remain and
`storeRecord()` processes them all — those with timestamps before `mStartRecordNs`
produce overflowed `int32_t` frame values in `mEventsPlay`, causing ghost notes.

```cpp
void LoopRecorder::rec() {
    mEventsRecorded.clear();   // ← add
    timespec start{};
    clock_gettime(CLOCK_MONOTONIC, &start);
    mStartRecordNs = start.tv_sec * 1000000000LL + start.tv_nsec;
    mState.store(State::Recording, std::memory_order_release);
}

// In onMidiEvent(), Armed branch — clear before first push_back:
if (mState.load(std::memory_order_acquire) == State::Armed) {
    mEventsRecorded.clear();   // ← add
    mStartRecordNs = timestamp;
    mState.store(State::Recording, std::memory_order_release);
    mEventsRecorded.push_back({timestamp + 100, msg});
    return;
}
```

Note: `mEventsRecorded.clear()` must run on the audio thread (the same thread that writes
to it). If `rec()` is called from the UI thread, coordinate with the
deferred-flag pattern from Fix 1.

---

## Additional notes

- `FrameMidiMsg` in `LoopRecorder.h` is a typo — rename to `FrameMidiMsg`.
- The CC 93/95 control-surface mapping inside `LoopRecorder::onMidiEvent` leaks transport
  concerns into the recorder. Consider moving to `MidiEngine::pollMidi()` so the
  mapping is visible and configurable without touching the recorder.
- `timestamp + 100` in the `Armed` branch is unexplained. If the intent is
  "+100 frames" it should be `+100LL * 1000000000LL / kSampleRate` ns (~2 ms at 48 kHz).
  If it is cosmetic, remove it.
