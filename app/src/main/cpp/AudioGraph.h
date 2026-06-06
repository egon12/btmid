#pragma once
#include <jni.h>
#include <string>
#include "InstrumentRepository.h"
#include "NativeEngine.h"

class AudioGraph {
public:
    AudioGraph();
    ~AudioGraph();

    AudioGraph(const AudioGraph&) = delete;
    AudioGraph& operator=(const AudioGraph&) = delete;

    void start();
    void stop();

    void setInstrument(int channel, const std::string& id);
    void loadDrumSample(int id, const float* data, int len);

    void noteOn(int channel, int note, int velocity);
    void noteOff(int channel, int note);
    void controlChange(int channel, int cc, int value);

    void openMidiDevice(JNIEnv* env, jobject jDevice, jobject jCallback);
    void closeMidiDevice();

private:
    // mRepository declared first: destroyed after mEngine (reverse order).
    // mEngine holds raw Instrument* from mRepository, so engine must die first.
    InstrumentRepository mRepository;
    NativeEngine         mEngine;
};
