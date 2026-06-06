#pragma once
#include <oboe/Oboe.h>
#include <memory>
#include <atomic>
#include <thread>
#include "ChannelEngine.h"

class OboeEngine : public ChannelEngine, public oboe::AudioStreamDataCallback {
public:
    OboeEngine() = default;
    ~OboeEngine() override;

    OboeEngine(const OboeEngine&) = delete;
    OboeEngine& operator=(const OboeEngine&) = delete;

    void start() override;
    void stop() override;

    // Overrides ChannelEngine to also start/stop the dispatch thread.
    void setOutputPort(JNIEnv* env, jobject jDevice, jobject jCallback) override;
    void clearOutputPort() override;

    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream* stream,
            void* audioData,
            int32_t numFrames) override;

private:
    void dispatchLoop();

    std::shared_ptr<oboe::AudioStream> mStream;
    std::thread                        mDispatchThread;
    std::atomic<bool>                  mDispatchRunning {false};
};
