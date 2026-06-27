#include "LoopRecorder.h"
#include <android/log.h>

#define LOG_TAG "LoopRecorder"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

void LoopRecorder::startRecording() {
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

void LoopRecorder::stopRecording() {
    auto s = state();
    if (s == State::Recording) {
        timespec stop{};
        auto res = clock_gettime(CLOCK_MONOTONIC, &stop);
        mStopRecordNs = stop.tv_sec * 1000000000L + stop.tv_nsec;
        changeState(State::Playing);
        map_timestamp_to_frame();
    } else if (s == State::Overdubbing) {
        auto currentPtr = std::atomic_load(&mPlayEventsPtr);
        auto newVec = std::make_shared<std::vector<FrameMidiMsg>>();
        newVec->reserve(currentPtr->size() + mEventsOverdubbed.size());

        std::sort(mEventsOverdubbed.begin(), mEventsOverdubbed.end(),
                  [](const FrameMidiMsg &a, const FrameMidiMsg &b) { return a.frame < b.frame; });

        // Merge two sorted vectors (both sorted by frame)
        std::merge(currentPtr->begin(), currentPtr->end(),
                   mEventsOverdubbed.begin(), mEventsOverdubbed.end(),
                   std::back_inserter(*newVec),
                   [](const FrameMidiMsg &a, const FrameMidiMsg &b) { return a.frame < b.frame; });

        std::atomic_store(&mPlayEventsPtr, newVec);
        mEventsOverdubbed.clear();
        changeState(State::Playing);
    }
}

void LoopRecorder::clear() {
    mEventsRecorded.clear();
    changeState(State::Idle);

    mStartPlayIndex = 0;
    mLoopLength = 0;
    current = 0;
}

void LoopRecorder::onMidiEvent(MidiMsg msg, int64_t timestamp) {
    auto s = state();
    if (s == State::Idle) {
        if (msg.type == MidiMsgType::CC && msg.channel == 0 && msg.data1 == 95 && msg.data2 == 0) {
            startRecording();
        }
        return;
    }

    if (s == State::Armed) {
        mStartRecordNs = timestamp;
        changeState(State::Recording);
        mEventsRecorded.push_back({timestamp + 100, msg});
        return;
    }

    if (s == State::Recording) {
        if (msg.type == MidiMsgType::CC && msg.channel == 0 && msg.data1 == 93 && msg.data2 == 0) {
            stopRecording();
            return;
        }

        mEventsRecorded.push_back({timestamp, msg});
        return;
    }

    if (s == State::Overdubbing) {
        if (msg.type == MidiMsgType::CC && msg.channel == 0 && msg.data1 == 93 && msg.data2 == 0) {
            stopRecording();
            return;
        }

        auto elapsed = timestamp - mStartRecordNs;
        auto frame =
                static_cast<int32_t>(static_cast<double>(elapsed) * mTimestampToFrame) % mLoopLength;
        mEventsOverdubbed.push_back({frame, msg});
        return;
    }

    if (s == State::Playing) {
        if (msg.type == MidiMsgType::CC && msg.channel == 0 && msg.data1 == 95 && msg.data2 == 0) {
            startRecording();
        }
    }
}

void LoopRecorder::onUiMidiEvent(MidiMsg m) {
    auto s = state();
    if (s == State::Armed) {
        timespec t{};
        auto res = clock_gettime(CLOCK_MONOTONIC, &t);
        auto ns = t.tv_sec * 1000000000L + t.tv_nsec;

        mStartRecordNs = ns;
        changeState(State::Recording);
        mEventsRecorded.push_back({ns + 1000, m});
        return;
    }

    if (s == State::Recording) {
        timespec t{};
        auto res = clock_gettime(CLOCK_MONOTONIC, &t);
        auto ns = t.tv_sec * 1000000000L + t.tv_nsec;
        mEventsRecorded.push_back({ns, m});
    }

    if (s == State::Overdubbing) {
        timespec t{};
        auto res = clock_gettime(CLOCK_MONOTONIC, &t);
        auto ns = t.tv_sec * 1000000000L + t.tv_nsec;

        auto elapsed = ns - mStartRecordNs;
        auto frame =
                static_cast<int32_t>(static_cast<double>(elapsed) * mTimestampToFrame) % mLoopLength;

        mEventsOverdubbed.push_back({frame, m});
        return;
    }
}

void LoopRecorder::advance(int32_t frames, const std::function<void(MidiMsg)> &fire) {
    auto s = state();

    if (!(s == State::Playing || s == State::Overdubbing)) return;

    int32_t endFrame = current + frames;

    auto eventsPtr = std::atomic_load(&mPlayEventsPtr);
    if (!eventsPtr || eventsPtr->empty()) return;

    for (int i = mStartPlayIndex; i < eventsPtr->size(); i++) {
        auto event = eventsPtr->at(i);

        if (event.frame <= endFrame) {
            fire(event.msg);
        } else {
            mStartPlayIndex = i;
            break;
        }
    }

    current = endFrame;
    if (current >= mLoopLength) {
        current -= mLoopLength;
        mStartPlayIndex = 0;
    }
}

void LoopRecorder::map_timestamp_to_frame() {
    auto vec = std::make_shared<std::vector<FrameMidiMsg>>();
    vec->reserve(mEventsRecorded.size());

    auto stop = mStopRecordNs;
    auto start = mStartRecordNs;
    auto duration = stop - start;

    mLoopLength = static_cast<int32_t>(static_cast<float>(duration) * mTimestampToFrame);
    for (auto e: mEventsRecorded) {
        auto t = e.timestamp - start;
        auto frame = static_cast<int32_t>(static_cast<float>(t) * mTimestampToFrame);
        vec->push_back({frame, e.msg});
    }

    std::atomic_store(&mPlayEventsPtr, vec);
}

void LoopRecorder::changeState(State newState) {
    if (onStateChange) onStateChange(newState);
    mState.store(newState, std::memory_order_release);
}