#pragma once
#include <oboe/Oboe.h>
#include <memory>
#include <vector>
#include <atomic>
#include "AudioRenderer.h"
#include "renderers/PianoRenderer.h"
#include "renderers/NoiseDrumRenderer.h"
#include "renderers/FmDrumRenderer.h"
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
    void setDrumBackend(int backendId); // 0=Noise, 1=Fm, 2=Sample

    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream* stream,
            void* audioData,
            int32_t numFrames) override;

private:
    std::shared_ptr<oboe::AudioStream>          mStream;
    std::vector<std::unique_ptr<AudioRenderer>> mRenderers;

    // Raw pointers into mRenderers — owned by the vector
    PianoRenderer*       mPiano             {nullptr};
    AudioRenderer*       mDrumRenderers[3]  {};        // [0]=Noise, [1]=Fm, [2]=Sample
    SampleDrumRenderer*  mSampleDrumRenderer{nullptr};
    std::atomic<int>     mActiveDrum        {0};
};
