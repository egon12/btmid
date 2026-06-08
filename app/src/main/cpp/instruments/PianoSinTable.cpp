#include "PianoSinTable.h"

const float PianoSinTable::kReleaseCoeff =
        (float) std::exp(-1.0 / (0.300 * kSampleRate));

void PianoSinTable::noteOn(int, int note, int velocity) {
    mOnQueue.push({note, velocity});
}

void PianoSinTable::noteOff(int, int note) { mOffQueue.push({note}); }

void PianoSinTable::controlChange(int, int cc, int value) {
    mCcQueue.push({cc, value});
}


void PianoSinTable::render(float *buffer, int32_t frames) {
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
            continue;
            //if (ev.cc == 64) setSustain(ev.value >= 64);
    }
    for (auto &v: mVoices) {
        if (v.active) renderVoice(v, buffer, frames);
    }
}

inline int PianoSinTable::next(int index) {
    return (index + 1) & (kQueueCap - 1);
}

void PianoSinTable::addVoice(int note, int velocity) {
    float peak = static_cast<float>(velocity) / 127.0f * 0.7f;
    float freq = 440.0f * std::powf(2.0, static_cast<float>(note - 69) / 12.0f);

    // Retrigger: deactivate any existing voice for this note
    for (auto &v: mVoices) {
        if (v.active && v.note == note) {
            v.active = false;
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
    }

    slot->active = true;
    slot->note = note;
    slot->timestamp = mTimestamp++;
    slot->peak = peak;
    slot->gain = 0.0f;
    slot->phase = Phase::Attack;
    slot->envSamples = 0;
    for (int h = 0; h < kHarmonics; ++h) {
        double freq_h = 440.0 * std::pow(2.0, (note - 69) / 12.0) * (h + 1);
        double phaseIncRad = kTwoPi * freq_h / kSampleRate;
        slot->phaseIncIdx[h] = (uint32_t) (phaseIncRad * kTableSize * (1.0 / (2 * M_PI)) *
                                           (1 << 16));
        slot->phaseIdx[h] = 0;
    }

}

void PianoSinTable::releaseVoice(int note) {
    for (auto &v: mVoices) {
        if (v.active && v.note == note && v.phase != Phase::Release)
            v.phase = Phase::Release;
    }
}

void PianoSinTable::renderVoice(Voice &v, float *buffer, int32_t frames) {
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
            int idx = (v.phaseIdx[h] >> 16) & (kTableSize - 1);
            int frac = (v.phaseIdx[h] >> 8) & 0xFF; // example 8-bit fraction
            float s = sinTable[idx] + (sinTable[idx + 1] - sinTable[idx]) * (frac / 256.0f);
            sample += kHarmonicAmps[h] * s;
            v.phaseIdx[h] += v.phaseIncIdx[h];
        }
        buffer[i] += v.gain * sample;
    }
}

void PianoSinTable::initSinTable() {
    for (int i = 0; i < kTableSize; ++i) {
        sinTable[i] = std::sin(2.0f * M_PI * i / kTableSize);
    }
}
