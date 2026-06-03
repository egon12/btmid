#pragma once

#include "../AudioRenderer.h"
#include <atomic>
#include <array>
#include <cstdint>
#include <cmath>

class PianoSinTableRenderer : public AudioRenderer {

public:
    ~PianoSinTableRenderer() override = default;

    void initSinTable();

    void noteOn(int channel, int note, int velocity) override;

    void noteOff(int channel, int note) override;

    void render(float *buffer, int32_t frames) override;

private:
    static constexpr int   kMaxVoices = 8;
    static constexpr int   kHarmonics = 5;
    static constexpr float kSustainLevel  = 0.60f;
    static constexpr double kTwoPi        = 2.0 * M_PI;
    static constexpr float kHarmonicAmps[kHarmonics] = {1.00f, 0.50f, 0.25f, 0.12f, 0.06f};

    static constexpr int kSampleRate    = 44100;
    static constexpr int kAttackSamples = (int)(0.005 * kSampleRate); // 220
    static constexpr int kDecaySamples  = (int)(0.080 * kSampleRate); // 3528

    // exp(-1 / (0.3 * 44100)) — not constexpr, initialised in .cpp
    static const float kReleaseCoeff;

    struct NoteOnEvent {
        int note;
        int velocity;
    };

    struct NoteOffEvent {
        int note;
    };

    static const int kQueueCap = 32;

    std::atomic<int> mOnHead = {0};
    std::atomic<int> mOnTail = {0};
    std::array<NoteOnEvent, kQueueCap> mOnQueue{};

    std::atomic<int> mOffHead = {0};
    std::atomic<int> mOffTail = {0};
    std::array<NoteOffEvent, kQueueCap> mOffQueue{};


    enum Phase { Attack, Decay, Sustain, Release, Done };

    int mTimestamp;

    struct Voice {
        bool     active     {false};
        int      note       {0};
        uint32_t timestamp  {0};   // lowest = oldest, used for voice stealing
        float    peak       {0.0f};
        float    gain       {0.0f};
        Phase    phase      {Phase::Attack};
        int      envSamples {0};
        uint32_t phaseIdx[kHarmonics];   // current index into sinTable
        uint32_t phaseIncIdx[kHarmonics]; // increment per sample, in fixed-point
    };

    std::array<Voice, kMaxVoices> mVoices{};

    static int next(int index);

    void addVoice(int note, int velocity);

    void renderVoice(Voice &voice, float *pDouble, int32_t i);

    void releaseVoice(int note);

    static constexpr uint32_t kTableSize = 1024;
    float sinTable[kTableSize];

};