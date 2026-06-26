#include "UICallback.h"
#include <android/log.h>

#define LOG_TAG "UICallback"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

void UICallback::setCallback(JNIEnv *env, jobject callback) {
    env->GetJavaVM(&mJvm);
    if (mMidiCallback) env->DeleteGlobalRef(mMidiCallback);
    mMidiCallback = env->NewGlobalRef(callback);
    jclass cls = env->GetObjectClass(callback);
    mOnMidiEventId = env->GetMethodID(cls, "onMidiEvent", "(IIII)V");
    mOnLoopStateId = env->GetMethodID(cls, "onLoopState", "(I)V");
    env->DeleteLocalRef(cls);
}

void UICallback::start() {
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
                if (evt.channel == 0xFF) {
                    if (env && mMidiCallback && mOnLoopStateId) {
                        env->CallVoidMethod(mMidiCallback, mOnLoopStateId,
                                            (jint) evt.type);
                    }
                } else {
                    if (env && mMidiCallback && mOnMidiEventId) {
                        env->CallVoidMethod(mMidiCallback, mOnMidiEventId,
                                            (jint) evt.channel, (jint) evt.type,
                                            (jint) evt.data1, (jint) evt.data2);
                    }
                }
            }
            if (!any) {
                struct timespec ts{0, 1'000'000};
                nanosleep(&ts, nullptr);
            }
        }

        if (mJvm && env) mJvm->DetachCurrentThread();
    });
    LOGD("UICallback dispatch thread started");
}

void UICallback::stop() {
    if (mDispatchRunning.exchange(false)) {
        if (mDispatchThread.joinable()) mDispatchThread.join();
    }
    if (mJvm && mMidiCallback) {
        JNIEnv *env = nullptr;
        bool attached = mJvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) == JNI_EDETACHED;
        if (attached) mJvm->AttachCurrentThread(&env, nullptr);
        if (env) env->DeleteGlobalRef(mMidiCallback);
        if (attached) mJvm->DetachCurrentThread();
        mMidiCallback = nullptr;
    }
    mJvm = nullptr;
    mOnMidiEventId = nullptr;
    mOnLoopStateId = nullptr;
    LOGD("UICallback dispatch thread stopped");
}

void UICallback::onMidiEvent(MidiEvt evt) {
    mEventQueue.push(evt);
}