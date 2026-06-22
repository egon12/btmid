#include "LoopRecorder.h"
#include <android/log.h>

#define LOG_TAG "LoopRecorder"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

void LoopRecorder::startRecording() {
    timespec start{};
    auto res = clock_gettime(CLOCK_MONOTONIC, &start);
    mStartRecordNs = start.tv_sec * 1000000000L + start.tv_nsec;
    mState.store(State::Recording, std::memory_order_release);
}

void LoopRecorder::stopRecording() {
    timespec stop{};
    auto res = clock_gettime(CLOCK_MONOTONIC, &stop);
    mStopRecordNs = stop.tv_sec * 1000000000L + stop.tv_nsec;
    mState.store(State::Playing, std::memory_order_release);
    map_timestamp_to_frame();
}

void LoopRecorder::startRecordOnPlay() {
    mState.store(State::StartRecordOnPlay, std::memory_order_release);
}

void LoopRecorder::clear() {
    mEventsRecorded.clear();
    mEventsPlay.clear();
    mState.store(State::Idle, std::memory_order_release);
}

void LoopRecorder::onMidiEvent(MidiMsg msg, int64_t timestamp) {
    if (mState.load(std::memory_order_acquire) == State::Idle) {
        if (msg.type == MidiMsgType::CC && msg.channel == 0 && msg.data1 == 95 && msg.data2 == 0) {
            startRecordOnPlay();
        }
        return;
    }

    if (mState.load(std::memory_order_acquire) == State::StartRecordOnPlay) {
        mStartRecordNs = timestamp;
        mState.store(State::Recording, std::memory_order_release);
        mEventsRecorded.push_back({timestamp+100, msg});
        return;
    }

    if (mState.load(std::memory_order_acquire) != State::Recording) return;
    if (msg.type == MidiMsgType::CC && msg.channel == 0 && msg.data1 == 93 && msg.data2 == 0) {
        stopRecording();
        return;
    }
    mEventsRecorded.push_back({timestamp, msg});
}

void LoopRecorder::onUiMidiEvent(MidiMsgType type, uint8_t note, uint8_t vel) {
    timespec t{};
    auto res = clock_gettime(CLOCK_MONOTONIC, &t);

    mEventsRecorded.push_back({t.tv_nsec, MidiMsg{type, 0, note, vel}});
}

void LoopRecorder::advance(int32_t frames, const std::function<void(MidiMsg)> &fire) {
    State s = mState.load(std::memory_order_acquire);

    if (s != State::Playing) return;

    int32_t endFrame = current + frames;

    for (int i = playedIndex + 1; i < mEventsPlay.size(); i++) {
        auto event = mEventsPlay.at(i);
        if (event.frame <= endFrame) {
            fire(event.msg);
        } else if (event.frame > endFrame) {
            playedIndex = i - 1;
            break;
        }
    }

    current = endFrame;
    if (current > mLoopLength) {
        current -= mLoopLength;
        playedIndex = -1;
    }
}

void LoopRecorder::map_timestamp_to_frame() {

    mEventsPlay.clear();

    auto stop = mStopRecordNs;
    auto start = mStartRecordNs;
    auto duration = stop - start;

    LOGD("start: %lld stop: %lld duration: %lld", start, stop, duration);

    const auto multiplier = static_cast<float>(kSampleRate) / 1000000000.0;
    LOGD("multiplier: %f", multiplier);

    // number of frames = duration / (1000 * 1000 * 1000L) * 48000L
    mLoopLength = static_cast<int32_t>(static_cast<float>(duration) * multiplier);
    for (auto e: mEventsRecorded) {
        auto t = e.timestamp - start;
        LOGD("timestamp: %lld %d %d %d", t, e.msg.channel, e.msg.type, e.msg.data1);

        auto frame = static_cast<int32_t>(static_cast<float>(t) * multiplier);

        mEventsPlay.push_back({frame, e.msg});
        LOGD("framed.  : %d %d %d %d", frame, e.msg.channel, e.msg.type, e.msg.data1);
    }
}
