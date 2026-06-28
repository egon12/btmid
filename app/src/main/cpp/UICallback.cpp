#include "UICallback.h"
#include <android/log.h>
#include <thread>
#include <atomic>
#include <ctime>

#define LOG_TAG "UICallback"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

UICallback::~UICallback() {
    stop();
}

void UICallback::setMidiEventListener(JNIEnv *env, jobject listener) {
    // Stop the dispatch thread before modifying the global ref to prevent race conditions
    if (mDispatchRunning.exchange(false)) {
        if (mDispatchThread.joinable()) mDispatchThread.join();
    }

    if (mMidiCallback) {
        env->DeleteGlobalRef(mMidiCallback);
        mMidiCallback = nullptr;
    }

    if (listener) {
        mMidiCallback = env->NewGlobalRef(listener);
        jclass cls = env->GetObjectClass(listener);
        mOnMidiEventId = env->GetMethodID(cls, "onMidiEvent", "(IIII)V");
        env->DeleteLocalRef(cls);
    } else {
        mOnMidiEventId = nullptr;
    }

    dispatchLoop(env);
}

void UICallback::onMidiEvent(MidiEvt evt) {
    mEventQueue.push(evt);
}

void UICallback::clearMidiEventListener() {
    if (!mJvm) return;

    if (!mMidiCallback) return;

    if (mDispatchRunning.exchange(false)) {
        if (mDispatchThread.joinable()) mDispatchThread.join();
    }

    JNIEnv *env = nullptr;
    bool attached =
            mJvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) == JNI_EDETACHED;
    if (attached) mJvm->AttachCurrentThread(&env, nullptr);

    if (env) env->DeleteGlobalRef(mMidiCallback);

    if (attached) mJvm->DetachCurrentThread();
    mMidiCallback = nullptr;
    mOnMidiEventId = nullptr;

    dispatchLoop(env);
}

void UICallback::setLoopStateListener(JNIEnv *env, jobject listener) {
    // Stop the dispatch thread before modifying the global ref to prevent race conditions
    if (mDispatchRunning.exchange(false)) {
        if (mDispatchThread.joinable()) mDispatchThread.join();
    }

    if (mLoopStateListener) {
        env->DeleteGlobalRef(mLoopStateListener);
        mLoopStateListener = nullptr;
    }

    if (listener) {
        mLoopStateListener = env->NewGlobalRef(listener);
        jclass cls = env->GetObjectClass(mLoopStateListener);
        mOnLoopStateId = env->GetMethodID(cls, "onLoopState", "(I)V");
        env->DeleteLocalRef(cls);
    } else {
        mOnLoopStateId = nullptr;
    }

    dispatchLoop(env);
}

void UICallback::onLoopState(LoopRecorder::State s) {
    mLoopStateQueue.push(s);
}

void UICallback::clearLoopStateListener() {
    if (!mJvm) return;

    if (!mLoopStateListener) return;

    if (mDispatchRunning.exchange(false)) {
        if (mDispatchThread.joinable()) mDispatchThread.join();
    }
    JNIEnv *env = nullptr;
    bool attached =
            mJvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) == JNI_EDETACHED;
    if (attached) mJvm->AttachCurrentThread(&env, nullptr);

    if (env) env->DeleteGlobalRef(mLoopStateListener);

    if (attached) mJvm->DetachCurrentThread();
    mLoopStateListener = nullptr;
    mOnLoopStateId = nullptr;

    // We don't run the dispatchLoop because clearLoopStateListener should be when all is done
    //dispatchLoop(env);
}

void UICallback::stop() {
    // Clearing listeners safely stops the thread and deletes global refs
    clearLoopStateListener();
    clearMidiEventListener();

    mDispatchRunning.store(false);

    // Final safety check to ensure thread is stopped
    if (mDispatchThread.joinable()) mDispatchThread.join();

    mJvm = nullptr;
}

void UICallback::dispatchLoop(JNIEnv *env) {
    if (!mMidiCallback && !mLoopStateListener) return;

    if (mDispatchRunning.load(std::memory_order_relaxed)) return;

    // Ensure any previous thread is joined before starting a new one to prevent std::terminate
    if (mDispatchThread.joinable()) {
        mDispatchThread.join();
    }

    mDispatchRunning.store(true, std::memory_order_relaxed);

    if (env && !mJvm) {
        env->GetJavaVM(&mJvm);
    }

    mDispatchThread = std::thread([this]() {
        JNIEnv *env = nullptr;
        if (mJvm) mJvm->AttachCurrentThread(&env, nullptr);

        while (mDispatchRunning.load(std::memory_order_acquire)) {
            bool hasMidi = consumeMidi(env);
            bool hasLoopState = consumeLoopState(env);

            // Sleep briefly to avoid busy-waiting if queues are empty
            if (!hasMidi && !hasLoopState) {
                struct timespec ts{0, 1'000'000}; // 1ms
                nanosleep(&ts, nullptr);
            }
        }

        if (mJvm && env) mJvm->DetachCurrentThread();
    });
}

bool UICallback::consumeMidi(JNIEnv *env) {
    bool any = false;
    MidiEvt evt{};

    while (mEventQueue.pop(evt)) {
        any = true;
        if (env && mMidiCallback && mOnMidiEventId) {
            env->CallVoidMethod(mMidiCallback, mOnMidiEventId,
                                (jint) evt.channel, (jint) evt.type,
                                (jint) evt.data1, (jint) evt.data2);

            if (env->ExceptionCheck()) {
                env->ExceptionClear();
            }
        }
    }
    return any;
}

bool UICallback::consumeLoopState(JNIEnv *env) {
    bool any = false;
    LoopRecorder::State state{};

    while (mLoopStateQueue.pop(state)) {
        any = true;
        if (env && mLoopStateListener && mOnLoopStateId) {
            env->CallVoidMethod(mLoopStateListener, mOnLoopStateId, (jint) state);

            if (env->ExceptionCheck()) {
                env->ExceptionClear();
            }
        }
    }
    return any;
}