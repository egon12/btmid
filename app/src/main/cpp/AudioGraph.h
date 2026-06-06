#pragma once
#include <jni.h>
#include <memory>
#include <string>
#include "AudioEngine.h"
#include "InstrumentRepository.h"

class AudioGraph {
public:
    AudioGraph();
    ~AudioGraph();

    AudioGraph(const AudioGraph&) = delete;
    AudioGraph& operator=(const AudioGraph&) = delete;

    void start();
    void stop();

    // Swap to a different engine. The old engine is stopped before replacement.
    // Caller is responsible for any engine-specific setup (e.g. startUdp) afterwards.
    void setEngine(std::unique_ptr<AudioEngine> engine);

    void setInstrument(int channel, const std::string& id);
    void loadDrumSample(int id, const float* data, int len);

    void noteOn(int channel, int note, int velocity);
    void noteOff(int channel, int note);
    void controlChange(int channel, int cc, int value);

    void openMidiDevice(JNIEnv* env, jobject jDevice, jobject jCallback);
    void closeMidiDevice();

private:
    // mRepository declared first so it outlives mEngine (reverse destruction order).
    // OboeEngine holds raw Instrument* from mRepository; engine must die first.
    InstrumentRepository         mRepository;
    std::unique_ptr<AudioEngine> mEngine;
};
