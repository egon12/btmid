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

    void onLoopProgress(int progress);

    void clearLoopStateListener();

    void stop();

private:
    JavaVM *mJvm{nullptr};

    std::thread mDispatchThread;
    std::atomic<bool> mDispatchRunning{false};

    SpscRing<MidiEvt, 256> mEventQueue;
    jobject mMidiCallback{nullptr};
    jmethodID mOnMidiEventId{nullptr};

    jobject mLoopStateListener{nullptr};

    SpscRing<LoopRecorder::State, 16> mLoopStateQueue;
    jmethodID mOnLoopStateChangeId{nullptr};

    SpscRing<int, 16> mLoopProgressQueue;
    std::atomic<int> lastProgress;
    jmethodID mOnLoopProgressId{nullptr};

    void dispatchLoop(JNIEnv *env);

    bool consumeMidi(JNIEnv *env);

    bool consumeLoopState(JNIEnv *env);

    bool consumeLoopProgress(JNIEnv *env);


};
