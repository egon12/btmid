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

    for (auto& v : mVoices) {
        if (v.active && v.note == note) {
            v.active = false;
            mSustainedNotes[note] = false;
            break;
        }
    }

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

float PolyOscillator::processSample(Voice& v) const {
    v.phase += v.freq / kSampleRate;
    if (v.phase >= 1.0f) v.phase -= 1.0f;

    static constexpr float kNormSine   = 1.000f;
    static constexpr float kNormSaw    = 1.225f;
    static constexpr float kNormSquare = 0.707f;

    switch (mWaveform) {
        case Waveform::Sine:
            return std::sinf(2.0f * static_cast<float>(M_PI) * v.phase) * kNormSine;
        case Waveform::Saw:
            return (2.0f * v.phase - 1.0f) * kNormSaw;
        case Waveform::Square:
            return ((v.phase < 0.5f) ? 1.0f : -1.0f) * kNormSquare;
    }
    return 0.0f;
}

void PolyOscillator::renderVoice(Voice& v, float* buffer, int32_t frames) {
    for (int i = 0; i < frames; ++i) {
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
