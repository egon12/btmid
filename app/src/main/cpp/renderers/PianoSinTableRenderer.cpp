#include "PianoSinTableRenderer.h"

const float PianoSinTableRenderer::kReleaseCoeff =
        (float) std::exp(-1.0 / (0.300 * kSampleRate));

void PianoSinTableRenderer::noteOn(int channel, int note, int velocity) {
    int head = mOnHead.load(std::memory_order_relaxed);
    int tail = mOnTail.load(std::memory_order_acquire);

    int nextHead = next(head);
    if (nextHead == tail) return;

    mOnQueue[head] = {note, velocity};
    mOnHead.store(nextHead, std::memory_order_release);
}

void PianoSinTableRenderer::noteOff(int channel, int note) {
    int head = mOffHead.load(std::memory_order_relaxed);
    int tail = mOffTail.load(std::memory_order_acquire);

    int nextHead = next(head);
    if (nextHead == tail) return;

    mOffQueue[head] = {note};
    mOffHead.store(nextHead, std::memory_order_release);
}

void PianoSinTableRenderer::render(float *buffer, int32_t frames) {

    {
        int tail = mOnTail.load(std::memory_order_relaxed);
        int head = mOnHead.load(std::memory_order_acquire);
        while (tail != head) {
            addVoice(mOnQueue[tail].note, mOnQueue[tail].velocity);
            tail = next(tail);
        }
        mOnTail.store(tail, std::memory_order_release);
    }
    {
        int tail = mOffTail.load(std::memory_order_relaxed);
        int head = mOffHead.load(std::memory_order_acquire);
        while (tail != head) {
            releaseVoice(mOffQueue[tail].note);
            tail = next(tail);
        }
        mOffTail.store(tail, std::memory_order_release);
    }

    for (auto &v: mVoices) {
        if (v.active) renderVoice(v, buffer, frames);
    }
}

inline int PianoSinTableRenderer::next(int index) {
    return (index + 1) & (kQueueCap - 1);
}

void PianoSinTableRenderer::addVoice(int note, int velocity) {
    float peak = velocity / 127.0f * 0.7f;
    double freq = 440.0 * std::pow(2.0, (note - 69) / 12.0);

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

        for (int h = 0; h < kHarmonics; ++h) {
            double freq_h = 440.0 * std::pow(2.0, (note - 69) / 12.0) * (h + 1);
            double phaseIncRad = kTwoPi * freq_h / kSampleRate;
            slot->phaseIncIdx[h] = (uint32_t) (phaseIncRad * kTableSize * (1.0 / (2 * M_PI)) *
                                               (1 << 16));
            slot->phaseIdx[h] = 0;
        }
    }

}

void PianoSinTableRenderer::releaseVoice(int note) {
    for (auto &v: mVoices) {
        if (v.active && v.note == note && v.phase != Phase::Release)
            v.phase = Phase::Release;
    }
}

void PianoSinTableRenderer::renderVoice(Voice &v, float *buffer, int32_t frames) {
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

void PianoSinTableRenderer::initSinTable() {
    for (int i = 0; i < kTableSize; ++i) {
        sinTable[i] = std::sin(2.0f * M_PI * i / kTableSize);
    }
}
