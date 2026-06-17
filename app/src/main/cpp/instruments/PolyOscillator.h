#pragma once

#include "../Instrument.h"
#include "../SpscRing.h"
#include "../AudioConfig.h"
#include <array>
#include <cstdint>
#include <cmath>

class PolyOscillator : public Instrument {
public:
    enum class Waveform { Sine, Saw, Square };

    explicit PolyOscillator(Waveform wf) : mWaveform(wf) {}

    void noteOn(int channel, int note, int velocity) override;
    void noteOff(int channel, int note) override;
    void controlChange(int channel, int cc, int value) override;
    void render(float* buffer, int32_t frames) override;

private:
    static constexpr int kMaxVoices = 8;
    static constexpr int kAttackSamples = static_cast<int>(0.005f * kSampleRate);
    static constexpr int kDecaySamples = static_cast<int>(0.080f * kSampleRate);
    static constexpr float kSustainLevel = 0.60f;
    static const float kReleaseCoeff;

    enum class EnvPhase { Attack, Decay, Sustain, Release, Done };

    struct Voice {
        bool active{false};
        int note{0};
        uint32_t timestamp{0};
        float gain{0.0f};
        float targetPeak{0.0f};
        EnvPhase envPhase{EnvPhase::Attack};
        int envSamples{0};
        float phase{0.0f};
        float freq{0.0f};
    };

    struct NoteOnEvent { int note; int velocity; };
    struct NoteOffEvent { int note; };
    struct CcEvent { int cc; int value; };

    static constexpr int kQueueCap = 32;
    SpscRing<NoteOnEvent, kQueueCap> mOnQueue;
    SpscRing<NoteOffEvent, kQueueCap> mOffQueue;
    SpscRing<CcEvent, kQueueCap> mCcQueue;

    std::array<Voice, kMaxVoices> mVoices{};
    uint32_t mTimestamp{0};
    Waveform mWaveform;

    bool mSustainHeld{false};
    bool mSustainedNotes[128]{};

    void addVoice(int note, int velocity);
    void releaseVoice(int note);
    void renderVoice(Voice& v, float* buffer, int32_t frames);
    void setSustain(bool on);
    float processSample(Voice& v) const;
};
