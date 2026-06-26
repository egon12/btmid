#pragma once

#include <thread>
#include <atomic>
#include <jni.h>
#include "AudioEngine.h"
#include "SpscRing.h"

class UICallback {
public:
    void setCallback(JNIEnv *env, jobject callback);

    void start();

    void stop();

    void onMidiEvent(MidiEvt evt);

private:
    SpscRing<MidiEvt, 256> mEventQueue;

    std::thread mDispatchThread;
    std::atomic<bool> mDispatchRunning{false};

    JavaVM *mJvm{nullptr};
    jobject mMidiCallback{nullptr};
    jmethodID mOnMidiEventId{nullptr};
    jmethodID mOnLoopStateId{nullptr};
};