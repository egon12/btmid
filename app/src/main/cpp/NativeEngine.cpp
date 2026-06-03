#include "NativeEngine.h"
#include "SineTestRenderer.h"
#include <android/log.h>

#define LOG_TAG "NativeEngine"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

NativeEngine::NativeEngine() {
    mRenderers.push_back(std::make_unique<SineTestRenderer>());
}

NativeEngine::~NativeEngine() {
    stop();
}

void NativeEngine::start() {
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
           ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
           ->setSharingMode(oboe::SharingMode::Exclusive)
           ->setFormat(oboe::AudioFormat::Float)
           ->setChannelCount(oboe::ChannelCount::Mono)
           ->setSampleRate(44100)
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

void NativeEngine::stop() {
    if (mStream) {
        mStream->requestStop();
        mStream->close();
        mStream.reset();
    }
}

void NativeEngine::noteOn(int channel, int note, int velocity) {
    for (auto& r : mRenderers) r->noteOn(channel, note, velocity);
}

void NativeEngine::noteOff(int channel, int note) {
    for (auto& r : mRenderers) r->noteOff(channel, note);
}

oboe::DataCallbackResult NativeEngine::onAudioReady(
        oboe::AudioStream*, void* audioData, int32_t numFrames) {
    auto* buf = static_cast<float*>(audioData);
    for (int i = 0; i < numFrames; ++i) buf[i] = 0.0f;
    for (auto& r : mRenderers) r->render(buf, numFrames);
    return oboe::DataCallbackResult::Continue;
}
