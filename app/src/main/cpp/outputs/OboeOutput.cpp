#include "OboeOutput.h"
#include <android/log.h>

#define LOG_TAG "OboeOutput"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

OboeOutput::~OboeOutput() {
    stop();
}

void OboeOutput::start() {
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
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

void OboeOutput::stop() {
    if (mStream) {
        mStream->requestStop();
        mStream->close();
        mStream.reset();
    }
}

oboe::DataCallbackResult
OboeOutput::onAudioReady(oboe::AudioStream *, void *audioData, int32_t numFrames) {
    auto *buf = static_cast<float *>(audioData);
    for (int i = 0; i < numFrames; ++i) buf[i] = 0.0f;

    mEngine->render(buf, numFrames);
    return oboe::DataCallbackResult::Continue;
}

OboeOutput::OboeOutput(MidiEngine *engine) : mEngine(engine) {}