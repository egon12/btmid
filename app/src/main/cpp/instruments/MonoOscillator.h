#pragma once

#include "../Instrument.h"
#include "../SpscRing.h"
#include "../AudioConfig.h"
#include <cstdint>
#include <cmath>

class MonoOscillator : public Instrument {
public:
    enum class Waveform { Sine, Saw, Square };

    explicit MonoOscillator(float portamentoTau = 0.15f,
                            Waveform waveform   = Waveform::Sine);

    void noteOn(int channel, int note, int velocity) override;
    void noteOff(int channel, int note) override;
    void controlChange(int channel, int cc, int value) override;
    void render(float* buffer, int32_t frames) override;

    void setPortamentoTime(float tau);

private:
    static constexpr int kAttackSamples = static_cast<int>(0.010f * kSampleRate);
    static constexpr int kDecaySamples  = static_cast<int>(0.050f * kSampleRate);
    static constexpr float kSustainLevel = 0.70f;
    static constexpr float kReleaseTime = 0.200f;
    static const float kReleaseCoeff;

    enum class EnvPhase { Idle, Attack, Decay, Sustain, Release };

    struct NoteOnEvent  { int note; int velocity; };
    struct NoteOffEvent { int note; };
    struct CcEvent      { int cc; int value; };

    static constexpr int kQueueCap = 32;
    SpscRing<NoteOnEvent,  kQueueCap> mOnQueue;
    SpscRing<NoteOffEvent, kQueueCap> mOffQueue;
    SpscRing<CcEvent,      kQueueCap> mCcQueue;

    EnvPhase mPhase{EnvPhase::Idle};
    int   mEnvSamples{0};
    float mGain{0.0f};
    float mTargetPeak{0.0f};

    float mCurrentFreq{0.0f};
    float mTargetFreq{0.0f};
    float mPhaseAccum{0.0f};
    float mPortamentoCoeff{0.0f};
    Waveform mWaveform{Waveform::Sine};

    void handleNoteOn(int note, int velocity);
    void handleNoteOff(int note);
    void recalcPortamentoCoeff(float tau);
    static float midiToFreq(int note);
};
