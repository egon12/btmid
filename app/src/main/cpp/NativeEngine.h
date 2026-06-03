#pragma once
#include <oboe/Oboe.h>
#include <memory>
#include <vector>
#include "AudioRenderer.h"
#include "renderers/SampleDrumRenderer.h"

class NativeEngine : public oboe::AudioStreamDataCallback {
public:
    NativeEngine();
    ~NativeEngine() override;

    NativeEngine(const NativeEngine&) = delete;
    NativeEngine& operator=(const NativeEngine&) = delete;

    void start();
    void stop();
    void noteOn(int channel, int note, int velocity);
    void noteOff(int channel, int note);
    void loadSample(int sampleId, const float* data, int length);

    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream* stream,
            void* audioData,
            int32_t numFrames) override;

private:
    std::shared_ptr<oboe::AudioStream> mStream;
    std::vector<std::unique_ptr<AudioRenderer>> mRenderers;
    SampleDrumRenderer* mSampleDrumRenderer{nullptr}; // owned by mRenderers
};
