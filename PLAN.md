# Plan: Velocity Sensitivity + Sustain Pedal

## Status

- [ ] Velocity curve (Piano)
- [ ] Sustain pedal (Instrument interface + Piano + NativeEngine)

---

## Velocity Sensitivity

All four instruments already scale velocity correctly (`velocity / 127.0f * 0.7f`).
On-screen keyboard and drum pads hardcode 100 — correct, touchscreens have no pressure data.

**Only change:** apply a mild exponential velocity curve in Piano so soft playing feels
more expressive. Linear `v/127` feels stiff on real MIDI keyboards.

### File: `app/src/main/cpp/instruments/Piano.cpp` — `addVoice()`

```cpp
// Before (linear):
float peak = velocity / 127.0f * 0.7f;

// After (mild exponential curve):
float peak = std::pow(velocity / 127.0f, 1.5f) * 0.7f;
```

No other files need changing for velocity.

---

## Sustain Pedal (CC 64)

MIDI CC 64 is the damper pedal: value ≥ 64 → pedal down, value < 64 → pedal up.
While the pedal is held, notes that receive `noteOff` should keep sounding;
they release only when the pedal lifts.

### Design

Track which **notes** are waiting to release at the Piano level — not inside Voice.
This keeps `Voice` struct untouched and `renderVoice()` unchanged.

CC routing follows the same pattern as `noteOn`/`noteOff`: NativeEngine routes by
channel, the instrument owns the knowledge of which CCs it cares about.

```
Piano members to add:
  bool mSustainHeld {false};
  bool mSustainedNotes[128] {};
```

### File: `app/src/main/cpp/Instrument.h`

Add `controlChange` with a default no-op so existing instruments (NoiseDrum, FmDrum,
SampleDrum) need no changes:

```cpp
virtual void controlChange(int channel, int cc, int value) {}
```

### File: `app/src/main/cpp/instruments/Piano.h`

- Add `bool mSustainHeld {false}` and `bool mSustainedNotes[128] {}` as private members.
- Declare `void controlChange(int channel, int cc, int value) override`.
- Declare `void setSustain(bool on)` as private.

### File: `app/src/main/cpp/instruments/Piano.cpp`

**`addVoice(note, velocity)`** — two places to clear `mSustainedNotes`:

```cpp
// 1. Retrigger: deactivate existing voice for this note
for (auto& v : mVoices) {
    if (v.active && v.note == note) {
        v.active = false;
        mSustainedNotes[note] = false;  // note restarts fresh
        break;
    }
}

// 2. Voice stealing: clear the stolen voice's sustained flag
if (!slot) {
    // ... find oldest voice into slot ...
    mSustainedNotes[slot->note] = false;  // stolen note no longer pending release
}
```

**`releaseVoice(note)`** — check pedal state instead of always going to Release:

```cpp
void Piano::releaseVoice(int note) {
    for (auto& v : mVoices) {
        if (v.active && v.note == note
                && v.phase != Phase::Release && v.phase != Phase::Done) {
            if (mSustainHeld)
                mSustainedNotes[note] = true;   // hold, don't touch phase
            else
                v.phase = Phase::Release;
        }
    }
}
```

**`setSustain(bool on)` — private method:**

```cpp
void Piano::setSustain(bool on) {
    mSustainHeld = on;
    if (!on) {
        for (int n = 0; n < 128; ++n) {
            if (mSustainedNotes[n]) {
                releaseVoice(n);        // mSustainHeld already false → goes to Release
                mSustainedNotes[n] = false;
            }
        }
    }
}
```

**`controlChange(channel, cc, value)` — new override:**

```cpp
void Piano::controlChange(int, int cc, int value) {
    if (cc == 64) setSustain(value >= 64);
}
```

### File: `app/src/main/cpp/NativeEngine.h`

Declare `controlChange` alongside `noteOn` and `noteOff`:

```cpp
void controlChange(int channel, int cc, int value);
```

### File: `app/src/main/cpp/NativeEngine.cpp`

**New method** — same routing pattern as `noteOn`/`noteOff`:

```cpp
void NativeEngine::controlChange(int channel, int cc, int value) {
    if (channel == 0 && mPiano)
        mPiano->controlChange(channel, cc, value);
    else if (channel == 9) {
        int idx = mActiveDrum.load(std::memory_order_relaxed);
        if (mDrumInstruments[idx])
            mDrumInstruments[idx]->controlChange(channel, cc, value);
    }
}
```

**`onAudioReady()`** — add one line alongside `noteOn`/`noteOff`:

```cpp
if (m.type == MidiMsgType::NoteOn)
    noteOn(m.channel, m.data1, m.data2);
else if (m.type == MidiMsgType::NoteOff)
    noteOff(m.channel, m.data1);
else if (m.type == MidiMsgType::CC)
    controlChange(m.channel, m.data1, m.data2);
// existing push to mEventQueue stays unchanged — covers CC too
```

---

## Edge Cases

| Scenario | Outcome |
|---|---|
| `noteOff` while pedal held | `mSustainedNotes[note] = true`, voice phase unchanged |
| Pedal releases | `releaseVoice(n)` for all marked notes → `Phase::Release` |
| Retrigger (noteOn) while note is sustained | `addVoice` clears `mSustainedNotes[note]`, voice restarts |
| Voice stealing (all 8 voices busy) | `addVoice` clears `mSustainedNotes[stolen_note]` before overwriting |
| Voice in Attack/Decay when pedal-held noteOff arrives | Voice runs naturally to Sustain, holds there until pedal releases |
| `setSustain(true)` while already held | No-op (benign) |
| Thread safety | `setSustain()` and `render()` both on audio thread — no race |
| CC 64 in UI log | Already covered: existing catch-all `mEventQueue.push` runs for all message types; `MidiEvent.kt` already has `ControlChange` sealed class |

---

## Out of Scope

- **Sostenuto pedal (CC 66)** — holds only notes already pressed at pedal-down time. More complex, not needed now.
- **Soft pedal (CC 67)** — reduces volume. Not requested.
- **Velocity curves for drums** — drum dynamics already feel natural with linear scaling.
