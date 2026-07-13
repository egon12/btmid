#pragma once

#include <atomic>
#include <cstdint>

class Metronome {
public:
    void setParams(bool enabled, int bpm, int beatsPerBar);
    void render(float *buf, int32_t frames);

private:
    std::atomic<bool> mEnabled{false};
    std::atomic<bool> mJustEnabled{false};
    std::atomic<int>  mBpm{120};
    std::atomic<int>  mBeatsPerBar{4};

    // Audio-thread-only state:
    int32_t mFrameCounter{0};
    int     mCurrentBeat{0};
    int32_t mClickRemaining{0};
    float   mClickPhase{0.0f};
    float   mClickFreqStep{0.0f};
    float   mClickEnv{0.0f};
    float   mClickDecay{0.0f};
    float   mClickAmp{0.0f};
};
