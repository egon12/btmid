#pragma once
#include "AudioRenderer.h"
#include <atomic>
#include <cmath>

class SineTestRenderer : public AudioRenderer {
public:
    void noteOn(int channel, int note, int velocity) override;
    void noteOff(int channel, int note) override;
    void render(float* buffer, int32_t frames) override;

private:
    std::atomic<int> mSamplesRemaining{0};
    double mPhase{0.0};

    static constexpr double kFreq = 440.0;
    static constexpr int kSampleRate = 44100;
    static constexpr int kDurationSamples = kSampleRate / 2; // 500 ms
    static constexpr double kTwoPi = 2.0 * M_PI;
};
