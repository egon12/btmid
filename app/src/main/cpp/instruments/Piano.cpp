#include "Piano.h"

const float Piano::kReleaseCoeff =
        (float) std::exp(-1.0 / (0.300 * kSampleRate));

void Piano::noteOn(int, int note, int velocity) {
    mOnQueue.push({note, velocity});
}

void Piano::noteOff(int, int note) { mOffQueue.push({note}); }

void Piano::controlChange(int, int cc, int value) {
    mCcQueue.push({cc, value});
}


void Piano::addVoice(int note, int velocity) {
    float peak = std::pow(static_cast<float>(velocity) / 127.0f, 1.5f) * 0.7f;
    float freq = 440.0f * std::powf(2.0f, static_cast<float>(note - 69) / 12.0f);

    // Retrigger: deactivate any existing voice for this note
    for (auto &v: mVoices) {
        if (v.active && v.note == note) {
            v.active = false;
            mSustainedNotes[note] = false;
            break;
        }
    }

    // Find a free slot, or steal the oldest active voice
    Voice *slot = nullptr;
    for (auto &v: mVoices) {
        if (!v.active) {
            slot = &v;
            break;
        }
    }
    if (!slot) {
        uint32_t oldest = UINT32_MAX;
        for (auto &v: mVoices) {
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
    slot->peak = peak;
    slot->gain = 0.0f;
    slot->phase = Phase::Attack;
    slot->envSamples = 0;
    for (int h = 0; h < kHarmonics; ++h) {
        slot->phases[h] = 0.0;
        slot->phaseIncs[h] = kTwoPi * freq * static_cast<float>(h + 1) / kSampleRate;
    }
}

void Piano::releaseVoice(int note) {
    for (auto &v: mVoices) {
        if (v.active && v.note == note && v.phase != Phase::Release) {
            if (mSustainHeld)
                mSustainedNotes[note] = true;
            else
                v.phase = Phase::Release;
        }
    }
}

void Piano::setSustain(bool on) {
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


void Piano::renderVoice(Voice &v, float *buffer, int32_t frames) {
    for (int i = 0; i < frames; ++i) {
        switch (v.phase) {
            case Phase::Attack:
                v.gain = ((float) v.envSamples / kAttackSamples) * v.peak;
                if (++v.envSamples >= kAttackSamples) {
                    v.gain = v.peak;
                    v.phase = Phase::Decay;
                    v.envSamples = 0;
                }
                break;
            case Phase::Decay: {
                float t = (float) v.envSamples / kDecaySamples;
                v.gain = v.peak * (1.0f - t * (1.0f - kSustainLevel));
                if (++v.envSamples >= kDecaySamples) {
                    v.gain = v.peak * kSustainLevel;
                    v.phase = Phase::Sustain;
                }
                break;
            }
            case Phase::Sustain:
                break;
            case Phase::Release:
                v.gain *= kReleaseCoeff;
                if (v.gain < 1e-4f) {
                    v.active = false;
                    return;
                }
                break;
            case Phase::Done:
                v.active = false;
                return;
        }

        float sample = 0.0f;
        for (int h = 0; h < kHarmonics; ++h) {
            sample += kHarmonicAmps[h] * (float) std::sinf(v.phases[h]);
            v.phases[h] += v.phaseIncs[h];
        }
        buffer[i] += v.gain * sample;
    }
}

void Piano::render(float *buffer, int32_t frames) {
    {
        NoteOnEvent ev{};
        while (mOnQueue.pop(ev)) addVoice(ev.note, ev.velocity);
    }
    {
        NoteOffEvent ev{};
        while (mOffQueue.pop(ev)) releaseVoice(ev.note);
    }
    {
        CcEvent ev{};
        while (mCcQueue.pop(ev))
            if (ev.cc == 64) setSustain(ev.value >= 64);
    }

    for (auto &v: mVoices) {
        if (v.active) renderVoice(v, buffer, frames);
    }
}
