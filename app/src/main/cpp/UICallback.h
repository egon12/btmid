#pragma once

#include <thread>
#include <atomic>
#include <jni.h>
#include "SpscRing.h"
#include "MidiEvt.h"
#include "LoopRecorder.h"

class UICallback {
public:
    UICallback() = default;

    ~UICallback();

    void setMidiEventListener(JNIEnv *env, jobject listener);

    void onMidiEvent(MidiEvt evt);

    void clearMidiEventListener();

    void setLoopStateListener(JNIEnv *env, jobject listener);

    void onLoopState(LoopRecorder::State s);

    void clearLoopStateListener();

    void stop();

private:
    SpscRing<MidiEvt, 256> mEventQueue;

    std::thread mDispatchThread;
    std::atomic<bool> mDispatchRunning{false};

    JavaVM *mJvm{nullptr};
    jobject mMidiCallback{nullptr};
    jmethodID mOnMidiEventId{nullptr};

    SpscRing<LoopRecorder::State, 16> mLoopStateQueue;
    jobject mLoopStateListener{nullptr};
    jmethodID mOnLoopStateId{nullptr};
    std::thread mLoopStateDispatchThread;
    std::atomic<bool> mLoopStateDispatchRunning{false};
};