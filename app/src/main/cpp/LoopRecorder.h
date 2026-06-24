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

struct FrameMidiMeg {
    int32_t frame;
    MidiMsg msg;
};

class LoopRecorder {
public:
    enum class State {
        Idle = 0, Recording = 1, Playing = 2, StartRecordOnPlay = 3,
    };

    void startRecording();

    void stopRecording();

    void clear();

    void startRecordOnPlay();

    std::function<void(State)> onStateChange;

    State state() const { return mState.load(std::memory_order_acquire); }

    void advance(int32_t frames, const std::function<void(MidiMsg)> &fire);

    void onMidiEvent(MidiMsg msg, int64_t timestamp);

    void onUiMidiEvent(MidiMsgType type, uint8_t channel, uint8_t note, uint8_t vel);

private:

    std::atomic<State> mState{State::Idle};
    void changeState(State newState);

    int64_t mStartRecordNs;
    int64_t mStopRecordNs;

    std::vector<TimestampedMidiMsg> mEventsRecorded;
    std::vector<FrameMidiMeg> mEventsPlay;

    int32_t mLoopLength{0};
    int32_t current{0};
    int mStartPlayIndex{0};

    void map_timestamp_to_frame();
};
