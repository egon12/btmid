#include "UICallback.h"
#include <android/log.h>

#define LOG_TAG "UICallback"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

UICallback::~UICallback() {
    stop();
}

void UICallback::setMidiEventListener(JNIEnv *env, jobject listener) {
    if (mMidiCallback) env->DeleteGlobalRef(mMidiCallback);
    mMidiCallback = env->NewGlobalRef(listener);
    jclass cls = env->GetObjectClass(listener);
    mOnMidiEventId = env->GetMethodID(cls, "onMidiEvent", "(IIII)V");
    env->DeleteLocalRef(cls);

    if (mDispatchRunning.load(std::memory_order_relaxed)) return;
    mDispatchRunning.store(true, std::memory_order_relaxed);
    mDispatchThread = std::thread([this]() {
        JNIEnv *env = nullptr;
        if (mJvm) mJvm->AttachCurrentThread(&env, nullptr);

        while (mDispatchRunning.load(std::memory_order_acquire)) {
            MidiEvt evt{};
            bool any = false;
            while (mEventQueue.pop(evt)) {
                any = true;
                if (env && mMidiCallback && mOnMidiEventId) {
                    env->CallVoidMethod(mMidiCallback, mOnMidiEventId,
                                        (jint) evt.channel, (jint) evt.type,
                                        (jint) evt.data1, (jint) evt.data2);
                }
            }
            if (!any) {
                struct timespec ts{0, 1'000'000};
                nanosleep(&ts, nullptr);
            }
        }

        if (mJvm && env) mJvm->DetachCurrentThread();
    });
}

void UICallback::onMidiEvent(MidiEvt evt) {
    mEventQueue.push(evt);
}

void UICallback::clearMidiEventListener() {
    if (mDispatchRunning.exchange(false)) {
        if (mDispatchThread.joinable()) mDispatchThread.join();
    }
    if (mJvm && mMidiCallback) {
        JNIEnv *env = nullptr;
        bool attached =
                mJvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) == JNI_EDETACHED;
        if (attached) mJvm->AttachCurrentThread(&env, nullptr);
        if (env) env->DeleteGlobalRef(mMidiCallback);
        if (attached) mJvm->DetachCurrentThread();
        mMidiCallback = nullptr;
    }
    mJvm = nullptr;
    mOnMidiEventId = nullptr;
    LOGD("UICallback dispatch thread stopped");
}

void UICallback::setLoopStateListener(JNIEnv *env, jobject listener) {
    env->GetJavaVM(&mJvm);
    if (mLoopStateListener) env->DeleteGlobalRef(mLoopStateListener);
    mLoopStateListener = env->NewGlobalRef(listener);
    jclass cls = env->GetObjectClass(mLoopStateListener);
    mOnLoopStateId = env->GetMethodID(cls, "onLoopState", "(I)V");
    env->DeleteLocalRef(cls);

    LOGD("setLoopStateListener: %p %p %p", mJvm, mLoopStateListener, mOnLoopStateId);

    if (mLoopStateDispatchRunning.load(std::memory_order_relaxed)) return;
    mLoopStateDispatchRunning.store(true, std::memory_order_relaxed);
    mLoopStateDispatchThread = std::thread([this]() {
        JNIEnv *env = nullptr;
        if (mJvm) mJvm->AttachCurrentThread(&env, nullptr);

        while (mLoopStateDispatchRunning.load(std::memory_order_acquire)) {
            LoopRecorder::State evt{};
            bool any = false;
            while (mLoopStateQueue.pop(evt)) {
                LOGD("LoopState from Queue: %d", evt);
                any = true;
                if (env && mLoopStateListener && mOnLoopStateId) {
                    env->CallVoidMethod(mLoopStateListener, mOnLoopStateId, (jint) evt);
                }
            }
            if (!any) {
                struct timespec ts{0, 1'000'000};
                nanosleep(&ts, nullptr);
            }
        }

        if (mJvm && env) mJvm->DetachCurrentThread();
    });
    LOGD("LoopState dispatch thread started");
}


void UICallback::onLoopState(LoopRecorder::State s) {
    mLoopStateQueue.push(s);
}

void UICallback::clearLoopStateListener() {
    if (mLoopStateDispatchRunning.exchange(false)) {
        if (mLoopStateDispatchThread.joinable()) mLoopStateDispatchThread.join();
    }
    if (mJvm && mLoopStateListener) {
        JNIEnv *env = nullptr;
        bool attached =
                mJvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) == JNI_EDETACHED;
        if (attached) mJvm->AttachCurrentThread(&env, nullptr);
        if (env) env->DeleteGlobalRef(mLoopStateListener);
        if (attached) mJvm->DetachCurrentThread();
        mLoopStateListener = nullptr;
    }
    mOnLoopStateId = nullptr;
}

void UICallback::stop() {
    clearLoopStateListener();
    clearMidiEventListener();
    mJvm = nullptr;
}

