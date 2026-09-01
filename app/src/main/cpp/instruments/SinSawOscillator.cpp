#include "SinSawOscillator.h"

const float SinSawOscillator::kReleaseCoeff =
        std::exp(-1.0f / (kReleaseTime * kSampleRate));

SinSawOscillator::SinSawOscillator(float portamentoTau) {
    recalcPortamentoCoeff(portamentoTau);
}

void SinSawOscillator::noteOn(int, int note, int velocity) {
    mOnQueue.push({note, velocity});
}

void SinSawOscillator::noteOff(int, int note) {
    mOffQueue.push({note});
}

void SinSawOscillator::controlChange(int, int cc, int value) {
    mCcQueue.push({cc, value});
}

void SinSawOscillator::setPortamentoTime(float tau) {
    recalcPortamentoCoeff(tau);
}

void SinSawOscillator::recalcPortamentoCoeff(float tau) {
    if (tau <= 0.0f) tau = 0.001f;
    mPortamentoCoeff = std::exp(-1.0f / (kSampleRate * tau));
}

float SinSawOscillator::midiToFreq(int note) {
    return 440.0f * std::pow(2.0f, static_cast<float>(note - 69) / 12.0f);
}

void SinSawOscillator::handleNoteOn(int note, int velocity) {
    lastNote = note;
    float freq = midiToFreq(note);
    float peak = std::pow(static_cast<float>(velocity) / 127.0f, 1.5f) * 0.7f;
    mTargetFreq = freq;

    switch (mPhase) {
        case EnvPhase::Idle:
            mCurrentFreq = freq;
            mPhase = EnvPhase::Attack;
            mEnvSamples = 0;
            mTargetPeak = peak;
            mGain = 0.0f;
            mPhaseAccum = 0.0f;
            break;

        case EnvPhase::Release:
            mPhase = EnvPhase::Attack;
            mEnvSamples = 0;
            mTargetPeak = peak;
            mGain = 0.0f;
            break;

        case EnvPhase::Attack:
        case EnvPhase::Decay:
        case EnvPhase::Sustain:
            break;
    }
}

void SinSawOscillator::handleNoteOff(int note) {
    if (note != lastNote) return;

    if (mPhase != EnvPhase::Idle && mPhase != EnvPhase::Release) {
        mPhase = EnvPhase::Release;
        mEnvSamples = 0;
    }
}

void SinSawOscillator::render(float* buffer, int32_t frames) {
    NoteOnEvent onEv{};
    while (mOnQueue.pop(onEv)) handleNoteOn(onEv.note, onEv.velocity);

    NoteOffEvent offEv{};
    while (mOffQueue.pop(offEv)) handleNoteOff(offEv.note);

    CcEvent ccEv{};
    while (mCcQueue.pop(ccEv)) {
        if (ccEv.cc == 74) {
            float tau = 0.005f + (static_cast<float>(ccEv.value) / 127.0f) * 0.395f;
            setPortamentoTime(tau);
        }
    }

    if (mPhase == EnvPhase::Idle) return;

    for (int32_t i = 0; i < frames; ++i) {
        switch (mPhase) {
            case EnvPhase::Attack:
                mGain = (static_cast<float>(mEnvSamples) / kAttackSamples) * mTargetPeak;
                if (++mEnvSamples >= kAttackSamples) {
                    mGain = mTargetPeak;
                    mPhase = EnvPhase::Decay;
                    mEnvSamples = 0;
                }
                break;

            case EnvPhase::Decay: {
                float t = static_cast<float>(mEnvSamples) / kDecaySamples;
                mGain = mTargetPeak * (1.0f - t * (1.0f - kSustainLevel));
                if (++mEnvSamples >= kDecaySamples) {
                    mGain = mTargetPeak * kSustainLevel;
                    mPhase = EnvPhase::Sustain;
                }
                break;
            }

            case EnvPhase::Sustain:
                break;

            case EnvPhase::Release:
                mGain *= kReleaseCoeff;
                if (mGain < 1e-4f) {
                    mPhase = EnvPhase::Idle;
                    return;
                }
                break;

            default:
                break;
        }

        mPhaseAccum += mCurrentFreq / kSampleRate;
        if (mPhaseAccum >= 1.0f) mPhaseAccum -= 1.0f;

        mCurrentFreq = mTargetFreq + (mCurrentFreq - mTargetFreq) * mPortamentoCoeff;

	float saw_wave;
	if (mPhaseAccum < 0.5f) {
		saw_wave = mPhaseAccum * 2.0f;
	} else {
		saw_wave = (mPhaseAccum-1.0f) * 2.0f;
	}
	float sin_wave = std::sinf(2.0f * static_cast<float>(M_PI) * mPhaseAccum);

        float raw = sin_wave + saw_wave

        buffer[i] += mGain * raw;
    }
}
