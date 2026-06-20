#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <vector>
#include "SpscRing.h"

struct LoopEvent {
    int32_t frameOffset;
    uint8_t type;
    uint8_t note;
    uint8_t velocity;
};

struct PendingLoopEvent { uint8_t type; uint8_t note; uint8_t vel; };

class LoopRecorder {
public:
    enum class State { Idle = 0, Recording = 1, Playing = 2 };

    void startRecording();
    void stopRecording();
    void clear();

    State state() const { return mState.load(std::memory_order_acquire); }

    void advance(int32_t frames,
                 const std::function<void(uint8_t, uint8_t, uint8_t)>& fire);

    void onMidiEvent(uint8_t type, uint8_t note, uint8_t velocity);
    void onUiMidiEvent(uint8_t type, uint8_t note, uint8_t vel);

private:
    std::atomic<State> mState       { State::Idle };
    std::atomic<bool>  mShouldStop  { false };
    std::atomic<bool>  mShouldClear { false };

    std::vector<LoopEvent> mEvents;
    int32_t mRecordFrame { 0 };
    int32_t mLoopLength  { 0 };
    int32_t mPlayFrame   { 0 };
    size_t  mPlayIdx     { 0 };

    SpscRing<PendingLoopEvent, 64> mUiEventQueue;
};
