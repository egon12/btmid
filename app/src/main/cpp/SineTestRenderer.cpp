#include "SineTestRenderer.h"
#include <cmath>

void SineTestRenderer::noteOn(int, int, int) {
    mSamplesRemaining.store(kDurationSamples, std::memory_order_relaxed);
}

void SineTestRenderer::noteOff(int, int) {}

void SineTestRenderer::render(float* buffer, int32_t frames) {
    const double phaseInc = kTwoPi * kFreq / kSampleRate;
    int remaining = mSamplesRemaining.load(std::memory_order_relaxed);
    for (int i = 0; i < frames; ++i) {
        if (remaining > 0) {
            buffer[i] += static_cast<float>(std::sin(mPhase) * 0.5);
            mPhase += phaseInc;
            if (mPhase >= kTwoPi) mPhase -= kTwoPi;
            --remaining;
        }
    }
    mSamplesRemaining.store(remaining, std::memory_order_relaxed);
}
