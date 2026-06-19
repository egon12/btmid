# Test: Monophonic Mono Oscillator

## Goal
Create a single-voice oscillator that glides (portamento) between notes instead of jumping instantly.

---

## 1. New C++ Class

- File: `app/src/main/cpp/instruments/MonoOscillator.h/.cpp`
- Inherits from `Instrument` interface
- Single sine-wave oscillator (monophonic)

---

## 2. Portamento Logic

- Track two frequencies:
  - `currentFreq` — the frequency currently being rendered
  - `targetFreq` — the frequency we are gliding toward
- On `noteOn(note)`:
  - Set `targetFreq = midiToFreq(note)`
  - `currentFreq` continues from wherever it is (this creates the glide)
- On `noteOff(note)`:
  - Trigger release phase of amplitude envelope
- In `render(buffer, numFrames)`:
  - Each sample/frame, advance `currentFreq` toward `targetFreq`
  - Use exponential approach: `currentFreq += (targetFreq - currentFreq) * (1.0 - exp(-dt / tau))`
  - `tau` = portamento time constant (e.g. 0.05s to 0.2s)

---

## 3. Voice Management (Monophonic)

- Only **one** active voice at a time
- On new `noteOn` while already playing:
  - Do NOT restart from scratch
  - Just update `targetFreq` → smooth glide from current pitch
- On `noteOff`:
  - Enter release phase
  - If new `noteOn` arrives during release:
    - Cancel release
    - Update `targetFreq` to new note
    - Restart attack phase

---

## 4. Amplitude Envelope

Simple ADSR, independent of pitch glide:

| Stage    | Time   | Level        |
|----------|--------|--------------|
| Attack   | 10 ms  | 0 → 1.0      |
| Decay    | 50 ms  | 1.0 → 0.7    |
| Sustain  | —      | 0.7 (hold)   |
| Release  | 200 ms | 0.7 → 0.0    |

---

## 5. Integration into AudioGraph

- Register in `InstrumentRepository` with id `"mono_osc"`
- Lazy-create on first `setInstrument(channel, "mono_osc")`
- Wire to a MIDI channel (e.g. channel 0 to replace piano, or channel 2 for testing)

---

## 6. UI (Optional for Testing)

- Add a 4th chip in `DrumEngineSelector`: `[ Mono ]`
- Or temporarily replace piano on channel 0 for quick testing

---

## 7. Files to Create / Modify

| File | Action |
|------|--------|
| `app/src/main/cpp/instruments/MonoOscillator.h` | Create |
| `app/src/main/cpp/instruments/MonoOscillator.cpp` | Create |
| `app/src/main/cpp/InstrumentRepository.h` | Add include + pointer |
| `app/src/main/cpp/InstrumentRepository.cpp` | Register `"mono_osc"` |
| `app/src/main/cpp/AudioGraph.cpp` | (Optional) wire default |
| `app/src/main/java/.../ui/DrumEngineSelector.kt` | (Optional) add chip |

---

## 8. Testing Checklist

- [ ] Press C4 on piano keyboard → hear steady tone
- [ ] Press E4 while C4 is held → pitch glides up smoothly
- [ ] Press C4 again → pitch glides back down
- [ ] Rapid C-E-C-E → smooth tracking, no clicks
- [ ] Short taps → clean attack/release, no artifacts
- [ ] Change portamento time → faster/slower glide
- [ ] No DC offset or clicks at note boundaries
