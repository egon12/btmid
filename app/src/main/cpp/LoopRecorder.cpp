#include "LoopRecorder.h"

void LoopRecorder::startRecording() {
    mShouldStop.store(false,  std::memory_order_relaxed);
    mShouldClear.store(true,  std::memory_order_relaxed);
    mState.store(State::Recording, std::memory_order_release);
}

void LoopRecorder::stopRecording() {
    mShouldStop.store(true, std::memory_order_release);
}

void LoopRecorder::clear() {
    State s = mState.load(std::memory_order_acquire);
    if (s == State::Idle) {
        mEvents.clear();
        mLoopLength = mRecordFrame = mPlayFrame = 0;
        mPlayIdx = 0;
        mShouldClear.store(false, std::memory_order_relaxed);
    } else {
        mShouldClear.store(true, std::memory_order_release);
    }
}

void LoopRecorder::onMidiEvent(uint8_t type, uint8_t note, uint8_t velocity) {
    if (mState.load(std::memory_order_acquire) != State::Recording) return;
    if (mShouldStop.load(std::memory_order_acquire))  return;
    if (mShouldClear.load(std::memory_order_acquire)) return;
    mEvents.push_back({ mRecordFrame, type, note, velocity });
}

void LoopRecorder::onUiMidiEvent(uint8_t type, uint8_t note, uint8_t vel) {
    mUiEventQueue.push({type, note, vel});
}

void LoopRecorder::advance(int32_t frames,
                           const std::function<void(uint8_t,uint8_t,uint8_t)>& fire) {
    State s = mState.load(std::memory_order_acquire);

    if (s == State::Recording) {
        if (mShouldClear.load(std::memory_order_acquire)) {
            mEvents.clear();
            mRecordFrame = 0;
            mShouldClear.store(false, std::memory_order_release);
        }

        PendingLoopEvent uiEv;
        while (mUiEventQueue.pop(uiEv)) {
            if (mState.load(std::memory_order_acquire) == State::Recording &&
                !mShouldStop.load(std::memory_order_acquire) &&
                !mShouldClear.load(std::memory_order_acquire)) {
                mEvents.push_back({ mRecordFrame, uiEv.type, uiEv.note, uiEv.vel });
            }
        }

        if (mShouldStop.load(std::memory_order_acquire)) {
            mLoopLength = mRecordFrame;
            mPlayFrame  = 0;
            mPlayIdx    = 0;
            mShouldStop.store(false, std::memory_order_release);
            State next = (!mEvents.empty() && mLoopLength > 0)
                         ? State::Playing : State::Idle;
            mState.store(next, std::memory_order_release);
        } else {
            mRecordFrame += frames;
        }
        return;
    }

    if (s == State::Playing) {
        if (mShouldClear.load(std::memory_order_acquire)) {
            mEvents.clear();
            mLoopLength = mRecordFrame = mPlayFrame = 0;
            mPlayIdx = 0;
            mShouldClear.store(false, std::memory_order_release);
            mState.store(State::Idle, std::memory_order_release);
            return;
        }

        int32_t playEnd = mPlayFrame + frames;

        if (playEnd < mLoopLength) {
            while (mPlayIdx < mEvents.size() &&
                   mEvents[mPlayIdx].frameOffset < playEnd) {
                const auto& e = mEvents[mPlayIdx++];
                fire(e.type, e.note, e.velocity);
            }
            mPlayFrame = playEnd;
        } else {
            while (mPlayIdx < mEvents.size()) {
                const auto& e = mEvents[mPlayIdx++];
                fire(e.type, e.note, e.velocity);
            }
            int32_t overflow = playEnd - mLoopLength;
            mPlayFrame = overflow;
            mPlayIdx   = 0;
            while (mPlayIdx < mEvents.size() &&
                   mEvents[mPlayIdx].frameOffset < overflow) {
                const auto& e = mEvents[mPlayIdx++];
                fire(e.type, e.note, e.velocity);
            }
        }
    }
}
