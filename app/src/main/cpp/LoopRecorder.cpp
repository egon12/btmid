#include "LoopRecorder.h"
#include <android/log.h>

#define LOG_TAG "LoopRecorder"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

void LoopRecorder::rec() {
    auto s = state();
    if (s == State::Idle) {
        changeState(State::Armed);
        return;
    } else if (s == State::Playing) {
        mEventsOverdubbed.clear();
        changeState(State::Overdubbing);
        return;
    }
}

void LoopRecorder::play() {
    auto s = state();
    if (s == State::Idle) {
        changeState(State::Playing);
    } else if (s == State::Recording) {
        mStopRecordNs = nowInNs();
        storeRecord();
        changeState(State::Playing);
    } else if (s == State::Overdubbing) {
        storeOverdub();
        changeState(State::Playing);
    }
}

void LoopRecorder::stop() {
    auto s = state();
    if (s == State::Playing) {
        changeState(State::Idle);
    } else if (s == State::Recording) {
        mStopRecordNs = nowInNs();
        storeRecord();
        changeState(State::Playing);
    } else if (s == State::Overdubbing) {
        storeOverdub();
        changeState(State::Playing);
    }
}


void LoopRecorder::clear() {
    mEventsRecorded.clear();
    changeState(State::Idle);

    mPlayIndex = 0;
    mCurrentFrame = 0;
    mEndFrame = 0;
}

void LoopRecorder::onMidiEvent(MidiMsg msg, int64_t timestamp) {
    auto s = state();

    if (msg.type == MidiMsgType::CC && msg.channel == 0 && msg.data1 == 95 && msg.data2 == 0) {
        rec();
        return;
    }

    if (msg.type == MidiMsgType::CC && msg.channel == 0 && msg.data1 == 94 && msg.data2 == 0) {
        play();
        return;
    }

    if (msg.type == MidiMsgType::CC && msg.channel == 0 && msg.data1 == 93 && msg.data2 == 0) {
        stop();
        return;
    }

    if (s == State::Armed) {
        mStartRecordNs = timestamp;
        changeState(State::Recording);
        mEventsRecorded.push_back({timestamp + 100, msg});
        return;
    }

    if (s == State::Recording) {
        mEventsRecorded.push_back({timestamp, msg});
        return;
    }

    if (s == State::Overdubbing) {
        auto elapsed = timestamp - mStartRecordNs;
        auto frame = nsToFrame(elapsed) % mEndFrame;
        mEventsOverdubbed.push_back({frame, msg});
        return;
    }
}

void LoopRecorder::onUiMidiEvent(MidiMsg m) {
    auto now = nowInNs();
    onMidiEvent(m, now);
}

void LoopRecorder::advance(int32_t frames, const std::function<void(MidiMsg)> &fire) {
    auto s = state();

    if (!(s == State::Playing || s == State::Overdubbing)) return;

    int32_t currentEndFrame = mCurrentFrame + frames;

    auto eventsPtr = std::atomic_load(&mPlayEventsPtr);
    if (!eventsPtr || eventsPtr->empty()) return;

    for (int i = mPlayIndex; i < eventsPtr->size(); i++) {
        auto event = eventsPtr->at(i);

        if (event.frame <= currentEndFrame) {
            fire(event.msg);
        } else {
            mPlayIndex = i;
            break;
        }
    }

    mCurrentFrame = currentEndFrame;
    if (mCurrentFrame >= mEndFrame) {
        mCurrentFrame -= mEndFrame;
        mPlayIndex = 0;
    }

    int progress = mCurrentFrame / (mEndFrame / 4);
    if (onProgress) onProgress(progress);
}

void LoopRecorder::storeRecord() {
    auto vec = std::make_shared<std::vector<FrameMidiMsg>>();
    vec->reserve(mEventsRecorded.size());

    auto stop = mStopRecordNs;
    auto start = mStartRecordNs;
    auto duration = stop - start;

    mEndFrame = nsToFrame(duration);
    for (auto e: mEventsRecorded) {
        auto t = e.timestamp - start;
        auto frame = nsToFrame(t);
        vec->push_back({frame, e.msg});
    }

    std::atomic_store(&mPlayEventsPtr, vec);
}

void LoopRecorder::storeOverdub() {
    auto currentPtr = std::atomic_load(&this->mPlayEventsPtr);
    auto newVec = std::__ndk1::make_shared<std::__ndk1::vector<FrameMidiMsg>>();
    newVec->reserve(currentPtr->size() + this->mEventsOverdubbed.size());

    std::sort(this->mEventsOverdubbed.begin(), this->mEventsOverdubbed.end(),
              [](const FrameMidiMsg &a, const FrameMidiMsg &b) { return a.frame < b.frame; });

    // Merge two sorted vectors (both sorted by frame)
    std::merge(currentPtr->begin(), currentPtr->end(),
               this->mEventsOverdubbed.begin(), this->mEventsOverdubbed.end(),
               std::back_inserter(*newVec),
               [](const FrameMidiMsg &a, const FrameMidiMsg &b) { return a.frame < b.frame; });


    int startPlayAt = 0;
    for (int i = 0; i < newVec->size(); i++) {
        auto event = newVec->at(i);
        if (event.frame >= this->mCurrentFrame) {
            startPlayAt = i;
            break;
        }
    }

    std::atomic_store(&this->mPlayEventsPtr, newVec);
    this->mPlayIndex = startPlayAt;
    this->mEventsOverdubbed.clear();
}

void LoopRecorder::changeState(State newState) {
    if (onStateChange) onStateChange(newState);
    mState.store(newState, std::memory_order_release);
}

int32_t LoopRecorder::nsToFrame(int64_t ns) const {
    return static_cast<int32_t>(static_cast<float>(ns) * mTimestampToFrame);
}

int64_t LoopRecorder::nowInNs() {
    timespec time{};
    auto res = clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec * 1000000000L + time.tv_nsec;
}
