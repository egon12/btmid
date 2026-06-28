#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <vector>
#include <ctime>
#include "SpscRing.h"
#include "MidiParser.h"
#include "AudioConfig.h"

struct TimestampedMidiMsg {
    int64_t timestamp;
    MidiMsg msg;
};

struct FrameMidiMsg {
    int32_t frame;
    MidiMsg msg;
};

class LoopRecorder {
public:
    enum class State {
        Idle = 0, Recording = 1, Playing = 2, Armed = 3, Overdubbing = 4,
    };

    void rec();

    void play();

    void stop();

    void clear();

    std::function<void(State)> onStateChange;

    State state() const { return mState.load(std::memory_order_acquire); }

    void advance(int32_t frames, const std::function<void(MidiMsg)> &fire);

    void onMidiEvent(MidiMsg msg, int64_t timestamp);

    void onUiMidiEvent(MidiMsg m);

private:
    const float mTimestampToFrame = static_cast<float>(kSampleRate) / 1'000'000'000.0;

    std::atomic<State> mState{State::Idle};

    void changeState(State newState);

    int64_t mStartRecordNs;
    int64_t mStopRecordNs;

    std::vector<TimestampedMidiMsg> mEventsRecorded;
    std::vector<FrameMidiMsg> mEventsOverdubbed;

    std::shared_ptr<std::vector<FrameMidiMsg>> mPlayEventsPtr;

    int32_t mEndFrame{0};
    int32_t mCurrentFrame{0};
    int mPlayIndex{0};

    void storeRecord();

    void storeOverdub();

    static int64_t nowInNs();

    int32_t nsToFrame(int64_t ns) const;
};
