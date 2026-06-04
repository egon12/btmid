#include "SampleDrum.h"
#include <algorithm>
#include <cstring>

SampleDrum::SampleDrum() {
    for (auto& l : mSampleLen) l.store(0, std::memory_order_relaxed);
}

int SampleDrum::noteToSampleId(int note) {
    switch (note) {
        case 35: case 36:                          return Kick;
        case 38: case 40:                          return Snare;
        case 42:                                   return ClosedHat;
        case 46:                                   return OpenHat;
        case 49: case 57:                          return Crash;
        case 51:                                   return Ride;
        case 41: case 43: case 45:
        case 47: case 48: case 50:                 return Tom;
        default:                                   return -1;
    }
}

int SampleDrum::nameToSampleId(const char* name) {
    if (std::strcmp(name, "kick")       == 0) return Kick;
    if (std::strcmp(name, "snare")      == 0) return Snare;
    if (std::strcmp(name, "closed_hat") == 0) return ClosedHat;
    if (std::strcmp(name, "open_hat")   == 0) return OpenHat;
    if (std::strcmp(name, "crash")      == 0) return Crash;
    if (std::strcmp(name, "ride")       == 0) return Ride;
    if (std::strcmp(name, "tom")        == 0) return Tom;
    return -1;
}

void SampleDrum::loadSample(int id, const float* data, int length) {
    if (id < 0 || id >= kNumSamples || length <= 0) return;
    mStorage[id].assign(data, data + length);
    // Release-store: audio thread acquire-loads this to gate access to mStorage[id]
    mSampleLen[id].store(length, std::memory_order_release);
}

void SampleDrum::noteOn(int, int note, int velocity) {
    int head     = mQueueHead.load(std::memory_order_relaxed);
    int nextHead = (head + 1) & (kQueueCap - 1);
    if (nextHead != mQueueTail.load(std::memory_order_acquire)) {
        mQueue[head] = {note, velocity};
        mQueueHead.store(nextHead, std::memory_order_release);
    }
}

void SampleDrum::noteOff(int, int) {}

void SampleDrum::render(float* buffer, int32_t frames) {
    int tail = mQueueTail.load(std::memory_order_relaxed);
    int head = mQueueHead.load(std::memory_order_acquire);
    while (tail != head) {
        auto& pn  = mQueue[tail];
        int   id  = noteToSampleId(pn.note);
        // Acquire-load: pairs with release-store in loadSample
        int   len = (id >= 0) ? mSampleLen[id].load(std::memory_order_acquire) : 0;
        if (len > 0) {
            for (auto& v : mVoices) {
                if (!v.active) {
                    v.active = true;
                    v.pcm    = mStorage[id].data();
                    v.length = len;
                    v.pos    = 0;
                    v.gain   = pn.velocity / 127.0f * 0.7f;
                    break;
                }
            }
        }
        tail = (tail + 1) & (kQueueCap - 1);
    }
    mQueueTail.store(tail, std::memory_order_release);

    for (auto& v : mVoices) {
        if (!v.active) continue;
        int count = std::min((int32_t)(v.length - v.pos), frames);
        for (int i = 0; i < count; ++i) {
            buffer[i] += v.gain * v.pcm[v.pos++];
        }
        if (v.pos >= v.length) v.active = false;
    }
}
