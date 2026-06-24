# Test: Portamento Glide — Physics & Implementation

## What is Portamento?

Portamento (Italian: "carrying") is a continuous glide from one pitch to another.
Unlike a discrete pitch jump, the frequency slides smoothly over time.

In synthesizers, this is also called "glide."

---

## The Math of the Glide

### Exponential Approach Model

The most natural-sounding portamento uses an **exponential approach**:

```
f(t) = f_target + (f_start - f_target) * exp(-t / tau)
```

Where:
- `f(t)` = frequency at time `t`
- `f_start` = frequency at the start of the glide
- `f_target` = target frequency (new note)
- `tau` = time constant (in seconds)
- `t` = elapsed time since note change

### Why Exponential?

- Human perception of pitch is logarithmic (frequency ratios, not differences)
- Exponential approach in linear frequency space ≈ linear approach in pitch space
- Sounds natural and smooth, like a finger sliding on a fretless string
- Avoids the "mechanical" feel of linear interpolation

---

## Discrete Implementation (Sample-by-Sample)

In code, we don't compute `exp()` per sample. Instead, we use the **recursive form**:

```cpp
// Once per audio frame (or once per sample):
currentFreq += (targetFreq - currentFreq) * coefficient;
```

Where `coefficient` is derived from the time constant:

```cpp
// coefficient = 1 - exp(-dt / tau)
// dt = time per sample = 1.0 / sampleRate
double coefficient = 1.0 - exp(-1.0 / (tau * sampleRate));
```

### Precompute the Coefficient

Compute `coefficient` once (when `tau` or `sampleRate` changes), not per sample:

```cpp
class MonoOscillator {
    double currentFreq = 440.0;
    double targetFreq = 440.0;
    double coeff;  // precomputed glide coefficient

    void setSampleRate(double sr) {
        coeff = 1.0 - exp(-1.0 / (tau * sr));
    }

    void setPortamentoTime(double seconds) {
        tau = seconds;
        coeff = 1.0 - exp(-1.0 / (tau * sampleRate));
    }
};
```

### Per-Sample Update

```cpp
// Inside render loop, per sample:
currentFreq += (targetFreq - currentFreq) * coeff;
double phase_inc = currentFreq / sampleRate;
phase += phase_inc;
if (phase >= 1.0) phase -= 1.0;
float sample = sinf(2.0f * M_PI * phase);
```

---

## Does Portamento Time Affect the Glide?

**Yes, absolutely.** The portamento time (`tau`) is the primary factor controlling glide speed.

### What `tau` Controls

| `tau` value | Behavior |
|-------------|----------|
| 0.01s (10ms) | Very fast glide, almost instant |
| 0.05s (50ms) | Quick, subtle glide |
| 0.1s (100ms) | Moderate, musical glide |
| 0.2s (200ms) | Slow, expressive glide |
| 0.5s (500ms) | Very slow, dramatic sweep |

### Time to "Reach" Target

The exponential approach never truly reaches the target (asymptotic), but practically:

| Elapsed time | % of glide completed |
|--------------|---------------------|
| 1 × tau      | ~63%                |
| 2 × tau      | ~86%                |
| 3 × tau      | ~95%                |
| 4 × tau      | ~98%                |
| 5 × tau      | ~99%                |

**Rule of thumb:** The glide is effectively complete after **3–5 × tau**.

So if `tau = 0.1s`, the pitch reaches ~95% of the target in 0.3s.

### Distance-Dependent vs Fixed Time

The exponential model has a property: **the time to reach a given percentage is constant**, regardless of how far the pitch must travel.

- Glide from C4 to C5 (octave): 95% in 0.3s
- Glide from C4 to D4 (whole step): 95% in 0.3s

This sounds natural for small intervals but can feel slow for large jumps.

**Alternative: distance-dependent portamento**
```cpp
// Adjust tau based on interval size
double semitones = abs(log2(targetFreq / currentFreq)) * 12.0;
double adaptiveTau = baseTau * (1.0 + semitones * 0.1);
```

This makes large intervals take slightly longer, which can sound more natural.

---

## Pitch Perception: Linear vs Logarithmic

### The Problem

Human pitch perception is **logarithmic**. An octave is a 2× frequency ratio, not a fixed Hz difference.

- C4 = 261.6 Hz, C5 = 523.2 Hz (difference: 261.6 Hz)
- C5 = 523.2 Hz, C6 = 1046.4 Hz (difference: 523.2 Hz)

If we glide linearly in Hz, the perceived speed is uneven — fast at low notes, slow at high notes.

### The Solution: Glide in Pitch Space

Convert to a logarithmic representation (MIDI note number or cents), glide linearly there, then convert back:

```cpp
// Convert to pitch space (MIDI note number, fractional)
double currentPitch = 69.0 + 12.0 * log2(currentFreq / 440.0);
double targetPitch = 69.0 + 12.0 * log2(targetFreq / 440.0);

// Glide in pitch space
currentPitch += (targetPitch - currentPitch) * coeff;

// Convert back to frequency
currentFreq = 440.0 * pow(2.0, (currentPitch - 69.0) / 12.0);
```

This gives **perceptually linear** glides — equal perceived speed regardless of register.

---

## Avoiding Artifacts

### Clicks at Note Boundaries

**Cause:** Discontinuity in the waveform phase when switching notes.

**Fix:** Never reset `phase` on note change. Let `currentFreq` glide; the phase accumulator handles continuity automatically.

```cpp
// BAD: causes click
phase = 0.0;  // don't do this on noteOn

// GOOD: phase continues, only frequency changes
currentFreq = targetFreq;  // or start gliding
```

### DC Offset

**Cause:** Asymmetric waveform or abrupt amplitude changes.

**Fix:**
- Use a proper sine oscillator (no DC)
- Apply envelope to amplitude, not frequency
- Fade in/out over a few samples if needed

### Zipper Noise

**Cause:** Updating `currentFreq` too infrequently (e.g., once per buffer instead of per sample).

**Fix:** Update frequency **per sample**, not per buffer.

```cpp
// BAD: once per buffer
for (int i = 0; i < numFrames; i++) {
    buffer[i] = sinf(phase);
    phase += phase_inc;
}
currentFreq += (targetFreq - currentFreq) * coeff;  // too late!

// GOOD: per sample
for (int i = 0; i < numFrames; i++) {
    currentFreq += (targetFreq - currentFreq) * coeff;
    double phase_inc = currentFreq / sampleRate;
    buffer[i] = sinf(2.0f * M_PI * phase);
    phase += phase_inc;
    if (phase >= 1.0) phase -= 1.0;
}
```

---

## Summary: Key Parameters

| Parameter | Effect | Typical Range |
|-----------|--------|---------------|
| `tau` (portamento time) | Glide speed | 0.01s – 0.5s |
| Glide model | Exponential vs linear | Exponential (natural) |
| Pitch space | Linear Hz vs log pitch | Log (perceptually even) |
| Update rate | Per buffer vs per sample | Per sample (no zipper) |
| Phase continuity | Reset vs continue | Continue (no clicks) |

---

## References

- Dodge, C. & Jerse, T. — *Computer Music: Synthesis, Composition, and Performance*
- Roads, C. — *The Computer Music Tutorial*
- Android Oboe documentation — real-time audio best practices
