#include "Metronome.h"
#include "AudioConfig.h"
#include <cmath>

static constexpr float kTwoPi = 6.283185307f;

void Metronome::setParams(bool enabled, int bpm, int beatsPerBar) {
    if (bpm < 1) bpm = 1;
    if (beatsPerBar < 1) beatsPerBar = 1;
    mBpm.store(bpm, std::memory_order_relaxed);
    mBeatsPerBar.store(beatsPerBar, std::memory_order_relaxed);
    if (enabled && !mEnabled.load(std::memory_order_relaxed)) {
        mJustEnabled.store(true, std::memory_order_relaxed);
    }
    mEnabled.store(enabled, std::memory_order_release);
}

void Metronome::render(float *buf, int32_t frames) {
    if (!mEnabled.load(std::memory_order_acquire)) {
        mCurrentBeat = 0;
        mFrameCounter = 0;
        mClickRemaining = 0;
        return;
    }

    if (mJustEnabled.exchange(false, std::memory_order_relaxed)) {
        int bpm = mBpm.load(std::memory_order_relaxed);
        mFrameCounter = kSampleRate * 60 / bpm;
        mCurrentBeat = 0;
        mClickRemaining = 0;
    }

    int bpm = mBpm.load(std::memory_order_relaxed);
    int beatsPerBar = mBeatsPerBar.load(std::memory_order_relaxed);
    int32_t framesPerBeat = kSampleRate * 60 / bpm;
    int32_t maxClickFrames = static_cast<int32_t>(0.05f * kSampleRate);

    for (int32_t i = 0; i < frames; i++) {
        if (mClickRemaining > 0) {
            buf[i] += mClickAmp * sinf(mClickPhase) * mClickEnv;
            mClickPhase += mClickFreqStep;
            if (mClickPhase > kTwoPi) mClickPhase -= kTwoPi;
            mClickEnv *= mClickDecay;
            mClickRemaining--;
        }
        mFrameCounter++;
        if (mFrameCounter >= framesPerBeat) {
            mFrameCounter -= framesPerBeat;
            float freq = (mCurrentBeat == 0) ? 1000.0f : 600.0f;
            mClickAmp      = (mCurrentBeat == 0) ? 0.5f : 0.3f;
            mClickFreqStep = kTwoPi * freq / kSampleRate;
            mClickPhase    = 0.0f;
            mClickEnv      = 1.0f;
            mClickDecay    = expf(-1.0f / (0.01f * kSampleRate));
            mClickRemaining = maxClickFrames;
            mCurrentBeat = (mCurrentBeat + 1) % beatsPerBar;
        }
    }
}
