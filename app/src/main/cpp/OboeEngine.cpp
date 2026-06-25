#include "OboeEngine.h"
#include <android/log.h>
#include <ctime>

#define LOG_TAG "OboeEngine"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

OboeEngine::~OboeEngine() {
    stop();
    clearOutputPort();
}

void OboeEngine::start() {
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
           ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
           ->setSharingMode(oboe::SharingMode::Exclusive)
           // TODO experiment with oboe::AudioFormat::I16
           ->setFormat(oboe::AudioFormat::Float)
           ->setChannelCount(oboe::ChannelCount::Mono)
           ->setSampleRate(kSampleRate)
           ->setDataCallback(this);

    oboe::Result result = builder.openStream(mStream);
    if (result != oboe::Result::OK) {
        LOGE("openStream failed: %s", oboe::convertToText(result));
        return;
    }
    result = mStream->requestStart();
    if (result != oboe::Result::OK) {
        LOGE("requestStart failed: %s", oboe::convertToText(result));
        return;
    }
    LOGD("Oboe stream started — sampleRate=%d framesPerBurst=%d",
         mStream->getSampleRate(), mStream->getFramesPerBurst());
}

void OboeEngine::stop() {
    if (mStream) {
        mStream->requestStop();
        mStream->close();
        mStream.reset();
    }
}

void OboeEngine::setOutputPort(JNIEnv* env, jobject jDevice, jobject jCallback) {
    ChannelEngine::setOutputPort(env, jDevice, jCallback);
    if (mMidiPort.load(std::memory_order_acquire)) {
        mDispatchRunning.store(true, std::memory_order_relaxed);
        mDispatchThread = std::thread(&OboeEngine::dispatchLoop, this);
    }
}

void OboeEngine::clearOutputPort() {
    if (mDispatchRunning.exchange(false)) {
        if (mDispatchThread.joinable()) mDispatchThread.join();
    }
    ChannelEngine::clearOutputPort();
}

void OboeEngine::dispatchLoop() {
    JNIEnv* env = nullptr;
    if (mJvm) mJvm->AttachCurrentThread(&env, nullptr);

    while (mDispatchRunning.load(std::memory_order_acquire)) {
        MidiEvt evt{};
        bool    any = false;
        while (mEventQueue.pop(evt)) {
            any = true;
            if (evt.channel == 0xFF) {
                if (env && mMidiCallback && mOnLoopStateId) {
                    env->CallVoidMethod(mMidiCallback, mOnLoopStateId,
                                       (jint)evt.type);
                }
            } else {
                if (env && mMidiCallback && mOnMidiEventId) {
                    env->CallVoidMethod(mMidiCallback, mOnMidiEventId,
                                       (jint)evt.channel, (jint)evt.type,
                                       (jint)evt.data1,   (jint)evt.data2);
                }
            }
        }
        if (!any) {
            struct timespec ts { 0, 1'000'000 };  // 1 ms
            nanosleep(&ts, nullptr);
        }
    }

    if (mJvm && env) mJvm->DetachCurrentThread();
}

oboe::DataCallbackResult OboeEngine::onAudioReady(
        oboe::AudioStream*, void* audioData, int32_t numFrames) {
    pollMidi();
    advanceLoop(numFrames);

    auto* buf = static_cast<float*>(audioData);
    for (int i = 0; i < numFrames; ++i) buf[i] = 0.0f;
    renderChannels(buf, numFrames);

    return oboe::DataCallbackResult::Continue;
}
