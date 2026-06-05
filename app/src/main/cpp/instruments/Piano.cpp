#include "Piano.h"

const float Piano::kReleaseCoeff =
    (float)std::exp(-1.0 / (0.300 * kSampleRate));

void Piano::noteOn(int, int note, int velocity) {
    int head     = mOnHead.load(std::memory_order_relaxed);
    int nextHead = (head + 1) & (kQueueCap - 1);
    if (nextHead != mOnTail.load(std::memory_order_acquire)) {
        mOnQueue[head] = {note, velocity};
        mOnHead.store(nextHead, std::memory_order_release);
    }
}

void Piano::noteOff(int, int note) {
    int head     = mOffHead.load(std::memory_order_relaxed);
    int nextHead = (head + 1) & (kQueueCap - 1);
    if (nextHead != mOffTail.load(std::memory_order_acquire)) {
        mOffQueue[head] = {note};
        mOffHead.store(nextHead, std::memory_order_release);
    }
}

void Piano::addVoice(int note, int velocity) {
    float  peak = std::pow(velocity / 127.0f, 1.5f) * 0.7f;
    double freq = 440.0 * std::pow(2.0, (note - 69) / 12.0);

    // Retrigger: deactivate any existing voice for this note
    for (auto& v : mVoices) {
        if (v.active && v.note == note) {
            v.active = false;
            mSustainedNotes[note] = false;
            break;
        }
    }

    // Find a free slot, or steal the oldest active voice
    Voice* slot = nullptr;
    for (auto& v : mVoices) {
        if (!v.active) { slot = &v; break; }
    }
    if (!slot) {
        uint32_t oldest = UINT32_MAX;
        for (auto& v : mVoices) {
            if (v.timestamp < oldest) { oldest = v.timestamp; slot = &v; }
        }
        mSustainedNotes[slot->note] = false;
    }

    slot->active     = true;
    slot->note       = note;
    slot->timestamp  = mTimestamp++;
    slot->peak       = peak;
    slot->gain       = 0.0f;
    slot->phase      = Phase::Attack;
    slot->envSamples = 0;
    for (int h = 0; h < kHarmonics; ++h) {
        slot->phases[h]    = 0.0;
        slot->phaseIncs[h] = kTwoPi * freq * (h + 1) / kSampleRate;
    }
}

void Piano::releaseVoice(int note) {
    for (auto& v : mVoices) {
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

void Piano::controlChange(int, int cc, int value) {
    int head     = mCcHead.load(std::memory_order_relaxed);
    int nextHead = (head + 1) & (kQueueCap - 1);
    if (nextHead != mCcTail.load(std::memory_order_acquire)) {
        mCcQueue[head] = {cc, value};
        mCcHead.store(nextHead, std::memory_order_release);
    }
}

void Piano::renderVoice(Voice& v, float* buffer, int32_t frames) {
    for (int i = 0; i < frames; ++i) {
        switch (v.phase) {
            case Phase::Attack:
                v.gain = ((float)v.envSamples / kAttackSamples) * v.peak;
                if (++v.envSamples >= kAttackSamples) {
                    v.gain       = v.peak;
                    v.phase      = Phase::Decay;
                    v.envSamples = 0;
                }
                break;
            case Phase::Decay: {
                float t = (float)v.envSamples / kDecaySamples;
                v.gain  = v.peak * (1.0f - t * (1.0f - kSustainLevel));
                if (++v.envSamples >= kDecaySamples) {
                    v.gain  = v.peak * kSustainLevel;
                    v.phase = Phase::Sustain;
                }
                break;
            }
            case Phase::Sustain:
                break;
            case Phase::Release:
                v.gain *= kReleaseCoeff;
                if (v.gain < 1e-4f) { v.active = false; return; }
                break;
            case Phase::Done:
                v.active = false;
                return;
        }

        float sample = 0.0f;
        for (int h = 0; h < kHarmonics; ++h) {
            sample        += kHarmonicAmps[h] * (float)std::sin(v.phases[h]);
            v.phases[h]   += v.phaseIncs[h];
        }
        buffer[i] += v.gain * sample;
    }
}

void Piano::render(float* buffer, int32_t frames) {
    {
        int tail = mOnTail.load(std::memory_order_relaxed);
        int head = mOnHead.load(std::memory_order_acquire);
        while (tail != head) {
            addVoice(mOnQueue[tail].note, mOnQueue[tail].velocity);
            tail = (tail + 1) & (kQueueCap - 1);
        }
        mOnTail.store(tail, std::memory_order_release);
    }
    {
        int tail = mOffTail.load(std::memory_order_relaxed);
        int head = mOffHead.load(std::memory_order_acquire);
        while (tail != head) {
            releaseVoice(mOffQueue[tail].note);
            tail = (tail + 1) & (kQueueCap - 1);
        }
        mOffTail.store(tail, std::memory_order_release);
    }
    {
        int tail = mCcTail.load(std::memory_order_relaxed);
        int head = mCcHead.load(std::memory_order_acquire);
        while (tail != head) {
            if (mCcQueue[tail].cc == 64) setSustain(mCcQueue[tail].value >= 64);
            tail = (tail + 1) & (kQueueCap - 1);
        }
        mCcTail.store(tail, std::memory_order_release);
    }

    for (auto& v : mVoices) {
        if (v.active) renderVoice(v, buffer, frames);
    }
}
