#include "FmDrum.h"

static float  envCoeff(double decayMs) {
    return (float)std::exp(-1.0 / (decayMs / 1000.0 * FmDrum::kSampleRate));
}
static double sweepCoeff(double tauMs) {
    return std::exp(-1.0 / (tauMs / 1000.0 * FmDrum::kSampleRate));
}

float FmDrum::nextRandom() {
    mRng ^= mRng << 13;
    mRng ^= mRng >> 17;
    mRng ^= mRng << 5;
    return (float)(int32_t)mRng / (float)0x80000000u;
}

double FmDrum::nextRandomPhase() {
    mRng ^= mRng << 13;
    mRng ^= mRng >> 17;
    mRng ^= mRng << 5;
    return (double)mRng * (kTwoPi / (double)0x100000000LL);
}

void FmDrum::noteOn(int, int note, int velocity) {
    mQueue.push({note, velocity});
}

void FmDrum::noteOff(int, int) {}

FmDrum::Voice FmDrum::makeVoice(int note, float gain) {
    Voice v;
    v.gain = gain;
    switch (note) {
        case 35: case 36:  // Bass drum
            v.active        = true;
            v.type          = VoiceType::Kick;
            v.envCoeff      = envCoeff(180.0);
            v.freq          = 50.0;
            v.freqEnd       = 30.0;
            v.freqDecay     = sweepCoeff(50.0);
            v.modIndex      = 4.0;
            v.modIndexDecay = sweepCoeff(80.0);
            break;
        case 38: case 40:  // Snare
            v.active   = true;
            v.type     = VoiceType::Snare;
            v.envCoeff = envCoeff(120.0);
            v.pi[0]    = kTwoPi * 200.0 / kSampleRate; // carrier
            v.pi[1]    = kTwoPi * 180.0 / kSampleRate; // modulator
            v.modIndex = 2.0;
            break;
        case 42:  // Closed hat
            v.active   = true;
            v.type     = VoiceType::Hat;
            v.envCoeff = envCoeff(45.0);
            v.pi[0]    = kTwoPi * 8000.0 / kSampleRate;
            v.pi[1]    = kTwoPi * 8000.0 * 1.4142 / kSampleRate;
            v.modIndex = 8.0;
            break;
        case 46:  // Open hat
            v.active   = true;
            v.type     = VoiceType::Hat;
            v.envCoeff = envCoeff(350.0);
            v.pi[0]    = kTwoPi * 8000.0 / kSampleRate;
            v.pi[1]    = kTwoPi * 8000.0 * 1.4142 / kSampleRate;
            v.modIndex = 8.0;
            break;
        case 49: case 57:  // Crash — random phases per hit for variation
            v.active    = true;
            v.type      = VoiceType::Crash;
            v.envCoeff  = envCoeff(900.0);
            v.ph[0]     = nextRandomPhase();
            v.ph[1]     = nextRandomPhase();
            v.ph[2]     = nextRandomPhase();
            v.ph[3]     = nextRandomPhase();
            v.pi[0]     = kTwoPi * 6000.0  / kSampleRate;
            v.pi[1]     = kTwoPi * 8485.0  / kSampleRate;
            v.pi[2]     = kTwoPi * 10392.0 / kSampleRate;
            v.pi[3]     = kTwoPi * 12728.0 / kSampleRate;
            v.modIndex  = 5.0;
            break;
        case 51:  // Ride
            v.active   = true;
            v.type     = VoiceType::Ride;
            v.envCoeff = envCoeff(500.0);
            v.pi[0]    = kTwoPi * 600.0  / kSampleRate;
            v.pi[1]    = kTwoPi * 2100.0 / kSampleRate; // 600 × 3.5
            v.pi[2]    = kTwoPi * 3060.0 / kSampleRate; // 600 × 5.1
            v.modIndex = 1.5;
            break;
        case 41: case 43: case 45: case 47: case 48: case 50: {  // Toms
            double startFreq;
            switch (note) {
                case 41: startFreq = 60.0;  break;
                case 43: startFreq = 75.0;  break;
                case 45: startFreq = 90.0;  break;
                case 47: startFreq = 110.0; break;
                case 48: startFreq = 130.0; break;
                default: startFreq = 155.0; break; // 50
            }
            v.active        = true;
            v.type          = VoiceType::Tom;
            v.envCoeff      = envCoeff(150.0);
            v.freq          = startFreq;
            v.freqEnd       = startFreq * 0.5;
            v.freqDecay     = sweepCoeff(60.0);
            v.modIndex      = 3.0;
            v.modIndexDecay = sweepCoeff(80.0);
            break;
        }
        default:
            break; // unknown note: v.active stays false
    }
    return v;
}

float FmDrum::renderVoiceSample(Voice& v) {
    float s;
    switch (v.type) {
        case VoiceType::Kick:
        case VoiceType::Tom: {
            s = v.gain * (float)std::sin(v.ph[0] + v.modIndex * std::sin(v.ph[0]));
            v.ph[0]    += kTwoPi * v.freq / kSampleRate;
            v.freq      = v.freqEnd + (v.freq - v.freqEnd) * v.freqDecay;
            v.modIndex *= v.modIndexDecay;
            break;
        }
        case VoiceType::Snare: {
            double tone = std::sin(v.ph[0] + v.modIndex * std::sin(v.ph[1]));
            float  noise = nextRandom();
            s = v.gain * (0.5f * (float)tone + 0.5f * noise);
            v.ph[0] += v.pi[0];
            v.ph[1] += v.pi[1];
            break;
        }
        case VoiceType::Hat: {
            s = v.gain * (float)std::sin(v.ph[0] + v.modIndex * std::sin(v.ph[1]));
            v.ph[0] += v.pi[0];
            v.ph[1] += v.pi[1];
            break;
        }
        case VoiceType::Crash: {
            // 4-op FM chain: op[3] → op[2] → op[1] → op[0] (carrier)
            double mod = std::sin(v.ph[3]);
            for (int i = 2; i >= 0; --i) mod = std::sin(v.ph[i] + v.modIndex * mod);
            float  noise = nextRandom();
            s = v.gain * (0.7f * (float)mod + 0.3f * noise);
            for (int i = 0; i < 4; ++i) v.ph[i] += v.pi[i];
            break;
        }
        case VoiceType::Ride: {
            double m2 = std::sin(v.ph[2]);
            double m1 = std::sin(v.ph[1] + v.modIndex * m2);
            s = v.gain * (float)std::sin(v.ph[0] + v.modIndex * m1);
            v.ph[0] += v.pi[0];
            v.ph[1] += v.pi[1];
            v.ph[2] += v.pi[2];
            break;
        }
        default:
            s = 0.0f;
            break;
    }
    v.gain *= v.envCoeff;
    if (v.gain < 1e-4f) v.active = false;
    return s;
}

void FmDrum::render(float* buffer, int32_t frames) {
    PendingNote pn;
    while (mQueue.pop(pn)) {
        float gain = pn.velocity / 127.0f * 0.7f;
        Voice v    = makeVoice(pn.note, gain);
        if (v.active) {
            for (auto& slot : mVoices) {
                if (!slot.active) { slot = v; break; }
            }
        }
    }

    for (int i = 0; i < frames; ++i) {
        for (auto& v : mVoices) {
            if (v.active) buffer[i] += renderVoiceSample(v);
        }
    }
}
