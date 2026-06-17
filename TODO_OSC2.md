# TODO: PolyOscillator Improvements (V2)

These improvements enhance audio quality and musicality without adding significant complexity.

---

## 1. Randomize Initial Phase (Prevents "Phase Locking")

**The Problem**: If you press a chord (e.g., 3 notes at once) with a Square or Saw wave, and all voices start at `phase = 0.0f`, their waveforms align perfectly. This causes a massive, harsh volume spike and a "click" on every chord strike.

**The Simple Fix**: Give each new voice a random starting phase between `0.0` and `1.0`.

### Implementation

In `PolyOscillator.cpp`, modify `addVoice()`:

```cpp
void PolyOscillator::addVoice(int note, int velocity) {
    // ... existing code ...

    slot->active = true;
    slot->note = note;
    slot->timestamp = mTimestamp++;
    slot->targetPeak = peak;
    slot->gain = 0.0f;
    slot->envPhase = EnvPhase::Attack;
    slot->envSamples = 0;
    
    // Random phase to prevent phase-locking on chords
    slot->phase = static_cast<float>(std::rand()) / RAND_MAX;
    
    slot->freq = freq;
}
```

**Impact**: Instantly makes chords sound smoother and more "analog" without any performance cost.

---

## 2. Smooth Release from Current Gain (Already Implemented)

**The Problem**: Many naive synth tutorials reset gain to `0` or `peak` when a key is released, causing loud clicks.

**The Current Solution**: Your plan already does this perfectly:
```cpp
case EnvPhase::Release:
    v.gain *= kReleaseCoeff;
    if (v.gain < 1e-4f) {
        v.active = false;
        return;
    }
    break;
```

This starts the exponential decay from *whatever the current volume is*, guaranteeing a click-free release even if the key is tapped quickly during the Attack phase.

**No changes needed** — this is already correct.

---

## 3. Velocity-to-Attack Mapping (Optional "Feel" Improvement)

**The Problem**: A piano or synth feels more expressive when hitting a key harder not only makes it louder, but also makes it speak *faster*.

**The Simple Fix**: Scale the attack time slightly based on velocity.

### Implementation

In `PolyOscillator.cpp`, modify `addVoice()`:

```cpp
void PolyOscillator::addVoice(int note, int velocity) {
    float peak = std::pow(static_cast<float>(velocity) / 127.0f, 1.5f) * 0.7f;
    float freq = 440.0f * std::pow(2.0f, static_cast<float>(note - 69) / 12.0f);
    
    // Dynamic attack: harder press = faster attack
    // velocity 127 -> 2ms attack, velocity 1 -> 15ms attack
    float attackScale = 1.0f - (static_cast<float>(velocity) / 127.0f);
    int dynamicAttack = static_cast<int>((2.0f + 13.0f * attackScale) * (kSampleRate / 1000.0f));

    // ... rest of addVoice ...
}
```

Then in `renderVoice()`, replace `kAttackSamples` with `dynamicAttack` (you'd need to store it in the `Voice` struct).

**Impact**: Makes the on-screen keyboard feel much more responsive and organic.

---

## 4. The "Aliasing" Reality Check

**The Reality**: Naive Saw and Square waves generate infinite harmonics. When `freq * harmonic` exceeds the Nyquist limit (half the sample rate, e.g., 22,050 Hz at 44.1 kHz), those harmonics "fold back" as ugly, inharmonic digital noise (aliasing). This is most noticeable on high notes (e.g., C6 and above).

**The Decision**: For a "simple" synth, this is often accepted as the "retro 8-bit / raw analog" character. We don't *need* to fix it now.

### Future Fix (If Needed)
If we ever want to fix it later, a 3-line "PolyBLEP" function can be added to `processSample` to smooth the corners:

```cpp
float PolyOscillator::polyBlep(float t, float dt) const {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    } else if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}
```

But we can skip this for V1.

---

## Review Checklist

- [ ] Random phase prevents chord clicks? (Yes)
- [ ] Smooth release is already implemented? (Yes)
- [ ] Velocity-to-attack adds expressiveness? (Optional, low priority)
- [ ] Aliasing is acceptable for V1? (Yes, can be fixed later with PolyBLEP)

---

## Implementation Order

1. **Random Phase**: Add immediately (1 line of code, huge impact)
2. **Velocity-to-Attack**: Add later if we want more expressiveness
3. **Aliasing Fix**: Only if high notes sound too harsh in practice