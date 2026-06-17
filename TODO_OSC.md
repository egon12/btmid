# TODO: PolyOscillator Implementation Plan

## Goal
Create a simple, easy-to-understand polyphonic oscillator instrument that can generate pure Sine, Saw, and Square waves, replacing the complex multi-harmonic Piano on channel 0.

## Core Design Principles
1. **Simplicity**: Use straightforward phase-accumulation logic for waveform generation.
2. **Clean Interface**: No `setWaveform` on the base `Instrument` interface. Each oscillator instance is locked to a waveform via its constructor.
3. **Polyphony & Envelopes**: 8-voice polyphony with voice stealing and a standard ADSR envelope (Attack 5ms, Decay 80ms, Sustain 60%, Release 300ms) to prevent clicks/pops.
4. **Thread Safety**: Reuse the existing `SpscRing` queue pattern for lock-free MIDI event handling from the audio thread.

---

## 1. Create `PolyOscillator.h`
**Path**: `app/src/main/cpp/instruments/PolyOscillator.h`

```cpp
#pragma once

#include "../Instrument.h"
#include "../SpscRing.h"
#include "../AudioConfig.h"
#include <array>
#include <cstdint>
#include <cmath>

class PolyOscillator : public Instrument {
public:
    enum class Waveform { Sine, Saw, Square };

    explicit PolyOscillator(Waveform wf) : mWaveform(wf) {}

    void noteOn(int channel, int note, int velocity) override;
    void noteOff(int channel, int note) override;
    void controlChange(int channel, int cc, int value) override;
    void render(float* buffer, int32_t frames) override;

private:
    static constexpr int kMaxVoices = 8;
    static constexpr int kAttackSamples = static_cast<int>(0.005f * kSampleRate);
    static constexpr int kDecaySamples = static_cast<int>(0.080f * kSampleRate);
    static constexpr float kSustainLevel = 0.60f;
    // not constexpr — std::exp is not constexpr in C++17/C++20; defined in .cpp
    static const float kReleaseCoeff;

    enum class EnvPhase { Attack, Decay, Sustain, Release, Done };

    struct Voice {
        bool active{false};
        int note{0};
        uint32_t timestamp{0};
        float gain{0.0f};
        float targetPeak{0.0f};
        EnvPhase envPhase{EnvPhase::Attack};
        int envSamples{0};
        float phase{0.0f};
        float freq{0.0f};
    };

    struct NoteOnEvent { int note; int velocity; };
    struct NoteOffEvent { int note; };
    struct CcEvent { int cc; int value; };

    static constexpr int kQueueCap = 32;
    SpscRing<NoteOnEvent, kQueueCap> mOnQueue;
    SpscRing<NoteOffEvent, kQueueCap> mOffQueue;
    SpscRing<CcEvent, kQueueCap> mCcQueue;

    std::array<Voice, kMaxVoices> mVoices{};
    uint32_t mTimestamp{0};
    Waveform mWaveform;

    bool mSustainHeld{false};
    bool mSustainedNotes[128]{};

    void addVoice(int note, int velocity);
    void releaseVoice(int note);
    void renderVoice(Voice& v, float* buffer, int32_t frames);
    void setSustain(bool on);
    float processSample(Voice& v) const;
};
```

---

## 2. Create `PolyOscillator.cpp`
**Path**: `app/src/main/cpp/instruments/PolyOscillator.cpp`

```cpp
#include "PolyOscillator.h"

const float PolyOscillator::kReleaseCoeff = std::exp(-1.0f / (0.300f * kSampleRate));

void PolyOscillator::noteOn(int, int note, int velocity) {
    mOnQueue.push({note, velocity});
}

void PolyOscillator::noteOff(int, int note) {
    mOffQueue.push({note});
}

void PolyOscillator::controlChange(int, int cc, int value) {
    mCcQueue.push({cc, value});
}

void PolyOscillator::addVoice(int note, int velocity) {
    float peak = std::pow(static_cast<float>(velocity) / 127.0f, 1.5f) * 0.7f;
    float freq = 440.0f * std::pow(2.0f, static_cast<float>(note - 69) / 12.0f);

    // Retrigger existing note
    for (auto& v : mVoices) {
        if (v.active && v.note == note) {
            v.active = false;
            mSustainedNotes[note] = false;
            break;
        }
    }

    // Find free slot or steal oldest
    Voice* slot = nullptr;
    for (auto& v : mVoices) {
        if (!v.active) { slot = &v; break; }
    }
    if (!slot) {
        uint32_t oldest = UINT32_MAX;
        for (auto& v : mVoices) {
            if (v.timestamp < oldest) {
                oldest = v.timestamp;
                slot = &v;
            }
        }
        mSustainedNotes[slot->note] = false;
    }

    slot->active = true;
    slot->note = note;
    slot->timestamp = mTimestamp++;
    slot->targetPeak = peak;
    slot->gain = 0.0f;
    slot->envPhase = EnvPhase::Attack;
    slot->envSamples = 0;
    slot->phase = 0.0f;
    slot->freq = freq;
}

void PolyOscillator::releaseVoice(int note) {
    for (auto& v : mVoices) {
        if (v.active && v.note == note && v.envPhase != EnvPhase::Release) {
            if (mSustainHeld) {
                mSustainedNotes[note] = true;
            } else {
                v.envPhase = EnvPhase::Release;
            }
        }
    }
}

void PolyOscillator::setSustain(bool on) {
    mSustainHeld = on;
    if (!on) {
        for (int n = 0; n < 128; ++n) {
            if (mSustainedNotes[n]) {
                releaseVoice(n);
                mSustainedNotes[n] = false;
            }
        }
    }
}

// Core oscillator logic: simple phase accumulation
float PolyOscillator::processSample(Voice& v) const {
    v.phase += v.freq / kSampleRate;
    if (v.phase >= 1.0f) v.phase -= 1.0f;

    switch (mWaveform) {
        case Waveform::Sine:
            return std::sinf(2.0f * static_cast<float>(M_PI) * v.phase);
        case Waveform::Saw:
            return 2.0f * v.phase - 1.0f;
        case Waveform::Square:
            return (v.phase < 0.5f) ? 1.0f : -1.0f;
    }
    return 0.0f;
}

void PolyOscillator::renderVoice(Voice& v, float* buffer, int32_t frames) {
    for (int i = 0; i < frames; ++i) {
        // 1. Envelope progression
        switch (v.envPhase) {
            case EnvPhase::Attack:
                v.gain = (static_cast<float>(v.envSamples) / kAttackSamples) * v.targetPeak;
                if (++v.envSamples >= kAttackSamples) {
                    v.gain = v.targetPeak;
                    v.envPhase = EnvPhase::Decay;
                    v.envSamples = 0;
                }
                break;
            case EnvPhase::Decay: {
                float t = static_cast<float>(v.envSamples) / kDecaySamples;
                v.gain = v.targetPeak * (1.0f - t * (1.0f - kSustainLevel));
                if (++v.envSamples >= kDecaySamples) {
                    v.gain = v.targetPeak * kSustainLevel;
                    v.envPhase = EnvPhase::Sustain;
                }
                break;
            }
            case EnvPhase::Sustain:
                break;
            case EnvPhase::Release:
                v.gain *= kReleaseCoeff;
                if (v.gain < 1e-4f) {
                    v.active = false;
                    return;
                }
                break;
            case EnvPhase::Done:
                v.active = false;
                return;
        }

        // 2. Generate sample and apply envelope
        buffer[i] += v.gain * processSample(v);
    }
}

void PolyOscillator::render(float* buffer, int32_t frames) {
    NoteOnEvent onEv{};
    while (mOnQueue.pop(onEv)) addVoice(onEv.note, onEv.velocity);

    NoteOffEvent offEv{};
    while (mOffQueue.pop(offEv)) releaseVoice(offEv.note);

    CcEvent ccEv{};
    while (mCcQueue.pop(ccEv)) {
        if (ccEv.cc == 64) setSustain(ccEv.value >= 64);
    }

    for (auto& v : mVoices) {
        if (v.active) renderVoice(v, buffer, frames);
    }
}
```

---

## 3. Update `CMakeLists.txt`
**Path**: `app/src/main/cpp/CMakeLists.txt`

Add `instruments/PolyOscillator.cpp` to the `add_library` source list:

```cmake
add_library(btmid SHARED
    MidiParser.cpp
    ChannelEngine.cpp
    OboeEngine.cpp
    WifiEngine.cpp
    InstrumentRepository.cpp
    AudioGraph.cpp
    instruments/Piano.cpp
    instruments/NoiseDrum.cpp
    instruments/FmDrum.cpp
    instruments/SampleDrum.cpp
    instruments/PianoSinTable.cpp
    instruments/PolyOscillator.cpp   # <-- add this
    PianoBenchmark.cpp
    jni_bridge.cpp
)
```

---

## 5. Update `InstrumentRepository.cpp`
**Path**: `app/src/main/cpp/InstrumentRepository.cpp`

Add the include and register the three new variants in `getOrCreate`:

```cpp
#include "instruments/PolyOscillator.h" // <-- Add this

// ... inside InstrumentRepository::getOrCreate ...
    } else if (id == "sine_oscillator") {
        inst = std::make_unique<PolyOscillator>(PolyOscillator::Waveform::Sine);
    } else if (id == "saw_oscillator") {
        inst = std::make_unique<PolyOscillator>(PolyOscillator::Waveform::Saw);
    } else if (id == "square_oscillator") {
        inst = std::make_unique<PolyOscillator>(PolyOscillator::Waveform::Square);
    }
// ...
```

---

## 6. Update `AudioGraph.cpp` Default
**Path**: `app/src/main/cpp/AudioGraph.cpp`

Change the default instrument on channel 0 from `"piano"` to `"saw_oscillator"`:

```cpp
AudioGraph::AudioGraph() : mEngine(std::make_unique<OboeEngine>()) {
    mRepository.setInstrument(*mEngine, 0, "saw_oscillator"); // Changed from "piano"
    mRepository.setInstrument(*mEngine, 9, "noise_drum");
}
```

---

## 7. Future Kotlin UI Steps (To be reviewed)
Since `NativeAudioEngine.setInstrument(0, name)` already exists and works dynamically, we can add a UI toggle in `MainViewModel.kt` and `MainScreen.kt` to call it with `"sine_oscillator"`, `"saw_oscillator"`, or `"square_oscillator"` without adding new JNI methods.

- Add `val activeOscillator: String = "saw_oscillator"` to `UiState`.
- Add `fun setOscillator(type: String)` which calls `NativeAudioEngine.setInstrument(0, type)` and updates the state.
- Add a `FilterChip` row above the `PianoKeyboard`: `[ Sine ] [ Saw ] [ Square ]`.

---

## Review Checklist
- [ ] Is the phase accumulation logic exactly as requested? (Yes)
- [ ] Are there any unnecessary complexities? (No, harmonic mixing removed)
- [ ] Does it respect the existing `Instrument` interface cleanly? (Yes)
- [ ] Is voice stealing and ADSR preserved for musicality? (Yes)