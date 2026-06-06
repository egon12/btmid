#pragma once
#include "../Instrument.h"
#include "../SpscRing.h"
#include <array>
#include <cstdint>
#include <cmath>

class FmDrum : public Instrument {
public:
    void noteOn(int channel, int note, int velocity) override;
    void noteOff(int channel, int note) override;
    void render(float* buffer, int32_t frames) override;


private:
    static constexpr int    kMaxVoices = 16;
    static constexpr double kTwoPi    = 2.0 * M_PI;

    enum class VoiceType { Kick, Snare, Hat, Crash, Ride, Tom };

    struct Voice {
        bool      active        {false};
        VoiceType type          {};
        float     gain          {0.0f};
        float     envCoeff      {0.0f};
        double    ph[4]         {};   // FM operator phases (up to 4 ops)
        double    pi[4]         {};   // FM operator phase increments
        double    modIndex      {0.0};
        double    modIndexDecay {1.0}; // sweep: Kick and Tom only
        double    freq          {0.0}; // current freq for sweep voices
        double    freqEnd       {0.0};
        double    freqDecay     {1.0};
    };

    struct PendingNote { int note; int velocity; };

    static constexpr int kQueueCap = 32; // must be power-of-2
    SpscRing<PendingNote, kQueueCap> mQueue;

    std::array<Voice, kMaxVoices> mVoices{};
    uint32_t mRng{0xDEADBEEFu}; // xorshift32, render-thread only

    float  nextRandom();
    double nextRandomPhase(); // [0, 2π)
    Voice  makeVoice(int note, float gain);
    float  renderVoiceSample(Voice& v);
};
