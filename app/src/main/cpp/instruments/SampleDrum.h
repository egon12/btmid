#pragma once
#include "../Instrument.h"
#include <array>
#include <atomic>
#include <vector>
#include <cstdint>

class SampleDrum : public Instrument {
public:
    SampleDrum();

    void noteOn(int channel, int note, int velocity) override;
    void noteOff(int channel, int note) override;
    void render(float* buffer, int32_t frames) override;

    // Called from JNI/Kotlin thread after OGG decode.
    // Each sampleId is written exactly once before first noteOn for that sound.
    void loadSample(int sampleId, const float* data, int length);

    enum SampleId {
        Kick = 0, Snare, ClosedHat, OpenHat, Crash, Ride, Tom,
        kNumSamples
    };

    static int noteToSampleId(int note);
    static int nameToSampleId(const char* name);

private:
    static constexpr int kMaxVoices = 16;
    static constexpr int kQueueCap  = 32; // must be power-of-2

    struct Voice {
        bool         active{false};
        const float* pcm   {nullptr};
        int          length{0};
        int          pos   {0};
        float        gain  {0.0f};
    };

    struct PendingNote { int note; int velocity; };

    std::array<PendingNote, kQueueCap> mQueue{};
    std::atomic<int> mQueueHead{0};
    std::atomic<int> mQueueTail{0};

    std::array<Voice, kMaxVoices> mVoices{};

    // Written once per id from Kotlin IO thread; read by audio thread.
    // mSampleLen release-store gates all access to mStorage[id].
    std::array<std::vector<float>, kNumSamples> mStorage{};
    std::atomic<int> mSampleLen[kNumSamples]; // initialised to 0 in constructor
};
