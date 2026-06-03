#pragma once
#include "../AudioRenderer.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <cmath>

class PianoRenderer : public AudioRenderer {
public:
    void noteOn(int channel, int note, int velocity) override;
    void noteOff(int channel, int note) override;
    void render(float* buffer, int32_t frames) override;

    static constexpr int kSampleRate    = 44100;
    static constexpr int kAttackSamples = (int)(0.005 * kSampleRate); // 220
    static constexpr int kDecaySamples  = (int)(0.080 * kSampleRate); // 3528

private:
    static constexpr int   kMaxVoices = 8;
    static constexpr int   kHarmonics = 5;
    static constexpr float kSustainLevel  = 0.60f;
    static constexpr double kTwoPi        = 2.0 * M_PI;
    static constexpr float kHarmonicAmps[kHarmonics] = {1.00f, 0.50f, 0.25f, 0.12f, 0.06f};

    // exp(-1 / (0.3 * 44100)) — not constexpr, initialised in .cpp
    static const float kReleaseCoeff;

    enum class Phase { Attack, Decay, Sustain, Release, Done };

    struct Voice {
        bool     active     {false};
        int      note       {0};
        uint32_t timestamp  {0};   // lowest = oldest, used for voice stealing
        float    peak       {0.0f};
        float    gain       {0.0f};
        Phase    phase      {Phase::Attack};
        int      envSamples {0};
        double   phases[kHarmonics]   {};
        double   phaseIncs[kHarmonics]{};
    };

    struct NoteOnEvent  { int note; int velocity; };
    struct NoteOffEvent { int note; };

    static constexpr int kQueueCap = 32; // must be power-of-2
    std::array<NoteOnEvent,  kQueueCap> mOnQueue{};
    std::array<NoteOffEvent, kQueueCap> mOffQueue{};
    std::atomic<int> mOnHead{0},  mOnTail{0};
    std::atomic<int> mOffHead{0}, mOffTail{0};

    std::array<Voice, kMaxVoices> mVoices{};
    uint32_t mTimestamp{0};

    void addVoice(int note, int velocity);
    void releaseVoice(int note);
    void renderVoice(Voice& v, float* buffer, int32_t frames);
};
