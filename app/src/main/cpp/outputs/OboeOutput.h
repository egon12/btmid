#pragma once

#include <oboe/Oboe.h>
#include <memory>
#include "../MidiEngine.h"

class OboeOutput : public oboe::AudioStreamDataCallback {
public:
    OboeOutput(MidiEngine *engine);

    ~OboeOutput() override;

    OboeOutput(const OboeOutput &) = delete;

    OboeOutput &operator=(const OboeOutput &) = delete;

    void start();

    void stop();

    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *stream,
            void *audioData,
            int32_t numFrames) override;

private:
    MidiEngine *mEngine;

    std::shared_ptr<oboe::AudioStream> mStream;
};