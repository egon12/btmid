#include "NoiseDrum.h"

static float makeDecayCoeff(double decayMs) {
    return (float)std::exp(-1.0 / (decayMs / 1000.0 * NoiseDrum::kSampleRate));
}

float NoiseDrum::nextRandom() {
    mRng ^= mRng << 13;
    mRng ^= mRng >> 17;
    mRng ^= mRng << 5;
    return (float)(int32_t)mRng / (float)0x80000000u;
}

void NoiseDrum::noteOn(int, int note, int velocity) {
    mQueue.push({note, velocity});
}

void NoiseDrum::noteOff(int, int) {}

NoiseDrum::Voice NoiseDrum::makeVoice(int note, float gain) {
    Voice v;
    v.active = true;
    v.gain   = gain;
    switch (note) {
        case 35: case 36:
            v.type      = VoiceType::BassDrum;
            v.decayCoeff = makeDecayCoeff(150.0);
            v.phaseInc  = kTwoPi * 60.0 / kSampleRate;
            break;
        case 38: case 40:
            v.type      = VoiceType::Snare;
            v.decayCoeff = makeDecayCoeff(100.0);
            v.phaseInc  = kTwoPi * 200.0 / kSampleRate;
            break;
        case 42:
            v.type      = VoiceType::ClosedHat;
            v.decayCoeff = makeDecayCoeff(50.0);
            break;
        case 46:
            v.type      = VoiceType::Noise;
            v.decayCoeff = makeDecayCoeff(300.0);
            break;
        case 49: case 57:
            v.type      = VoiceType::Noise;
            v.decayCoeff = makeDecayCoeff(800.0);
            break;
        case 51:
            v.type      = VoiceType::Ride;
            v.decayCoeff = makeDecayCoeff(400.0);
            v.phaseInc  = kTwoPi * 600.0 / kSampleRate;
            break;
        default:
            v.type      = VoiceType::Noise;
            v.decayCoeff = makeDecayCoeff(100.0);
            break;
    }
    return v;
}

float NoiseDrum::renderVoiceSample(Voice& v) {
    float s;
    switch (v.type) {
        case VoiceType::BassDrum: {
            s = v.gain * (float)std::sin(v.phase);
            v.phase += v.phaseInc;
            break;
        }
        case VoiceType::Snare: {
            float noise = nextRandom();
            float tone  = (float)std::sin(v.phase);
            s = v.gain * (0.7f * noise + 0.3f * tone);
            v.phase += v.phaseInc;
            break;
        }
        case VoiceType::ClosedHat: {
            float x  = nextRandom();
            float y  = x - v.prevNoise;
            v.prevNoise = x;
            s = v.gain * y;
            break;
        }
        case VoiceType::Noise: {
            s = v.gain * nextRandom();
            break;
        }
        case VoiceType::Ride: {
            float noise = nextRandom();
            float tone  = (float)std::sin(v.phase);
            s = v.gain * (0.6f * noise + 0.4f * tone);
            v.phase += v.phaseInc;
            break;
        }
        default:
            s = 0.0f;
            break;
    }
    v.gain *= v.decayCoeff;
    if (v.gain < 1e-4f) v.active = false;
    return s;
}

void NoiseDrum::render(float* buffer, int32_t frames) {
    PendingNote pn;
    while (mQueue.pop(pn)) {
        float gain = pn.velocity / 127.0f * 0.7f;
        Voice v    = makeVoice(pn.note, gain);
        for (auto& slot : mVoices) {
            if (!slot.active) { slot = v; break; }
        }
    }

    for (int i = 0; i < frames; ++i) {
        for (auto& v : mVoices) {
            if (v.active) buffer[i] += renderVoiceSample(v);
        }
    }
}
