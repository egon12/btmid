#pragma once
#include "../AudioRenderer.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <cmath>

class NoiseDrumRenderer : public AudioRenderer {
public:
    void noteOn(int channel, int note, int velocity) override;
    void noteOff(int channel, int note) override;
    void render(float* buffer, int32_t frames) override;

    static constexpr int    kSampleRate = 44100;

private:
    static constexpr int    kMaxVoices  = 16;
    static constexpr double kTwoPi      = 2.0 * M_PI;

    enum class VoiceType { BassDrum, Snare, ClosedHat, Noise, Ride };

    struct Voice {
        bool      active    {false};
        VoiceType type      {};
        float     gain      {0.0f};
        float     decayCoeff{0.0f};
        double    phase     {0.0};
        double    phaseInc  {0.0};
        float     prevNoise {0.0f};
    };

    struct PendingNote { int note; int velocity; };

    // SPSC lock-free ring buffer: noteOn = sole writer, render = sole reader
    static constexpr int kQueueCap = 32; // must be power-of-2
    std::array<PendingNote, kQueueCap> mQueue{};
    std::atomic<int> mQueueHead{0};
    std::atomic<int> mQueueTail{0};

    std::array<Voice, kMaxVoices> mVoices{};
    uint32_t mRng{0x12345678u}; // xorshift32, render-thread only

    float nextRandom();
    Voice makeVoice(int note, float gain);
    float renderVoiceSample(Voice& v);
};
