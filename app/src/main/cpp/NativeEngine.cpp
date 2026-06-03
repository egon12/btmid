#include "NativeEngine.h"
#include <android/log.h>

#define LOG_TAG "NativeEngine"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

NativeEngine::NativeEngine() {
    // Piano — channel 0
    auto piano = std::make_unique<PianoRenderer>();
    mPiano = piano.get();
    mRenderers.push_back(std::move(piano));

    // Drum backends — channel 9; index matches DrumBackend Kotlin enum (0=Noise, 1=Fm, 2=Sample)
    auto noise = std::make_unique<NoiseDrumRenderer>();
    mDrumRenderers[0] = noise.get();
    mRenderers.push_back(std::move(noise));

    auto fm = std::make_unique<FmDrumRenderer>();
    mDrumRenderers[1] = fm.get();
    mRenderers.push_back(std::move(fm));

    auto sdr = std::make_unique<SampleDrumRenderer>();
    mSampleDrumRenderer = sdr.get();
    mDrumRenderers[2]   = sdr.get();
    mRenderers.push_back(std::move(sdr));
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
    if (channel == 0 && mPiano) {
        mPiano->noteOn(channel, note, velocity);
    } else if (channel == 9) {
        int idx = mActiveDrum.load(std::memory_order_relaxed);
        if (mDrumRenderers[idx]) mDrumRenderers[idx]->noteOn(channel, note, velocity);
    }
}

void NativeEngine::noteOff(int channel, int note) {
    if (channel == 0 && mPiano) {
        mPiano->noteOff(channel, note);
    } else if (channel == 9) {
        int idx = mActiveDrum.load(std::memory_order_relaxed);
        if (mDrumRenderers[idx]) mDrumRenderers[idx]->noteOff(channel, note);
    }
}

void NativeEngine::loadSample(int sampleId, const float* data, int length) {
    if (mSampleDrumRenderer) mSampleDrumRenderer->loadSample(sampleId, data, length);
}

void NativeEngine::setDrumBackend(int backendId) {
    if (backendId >= 0 && backendId < 3)
        mActiveDrum.store(backendId, std::memory_order_relaxed);
}

oboe::DataCallbackResult NativeEngine::onAudioReady(
        oboe::AudioStream*, void* audioData, int32_t numFrames) {
    auto* buf = static_cast<float*>(audioData);
    for (int i = 0; i < numFrames; ++i) buf[i] = 0.0f;
    for (auto& r : mRenderers) r->render(buf, numFrames);
    return oboe::DataCallbackResult::Continue;
}
