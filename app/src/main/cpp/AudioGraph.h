#pragma once

#include <jni.h>
#include <memory>
#include <string>
#include "InstrumentRepository.h"

class MidiEngine;

class UICallback;

class OboeOutput;

class WifiOutput;

class AudioGraph {
public:
    AudioGraph();

    ~AudioGraph();

    AudioGraph(const AudioGraph &) = delete;

    AudioGraph &operator=(const AudioGraph &) = delete;

    void start();

    void stop();

    void openMidiDevice(JNIEnv *env, jobject jDevice, jobject jListener);

    void closeMidiDevice();

    void setOutput(int outputId, const std::string &host, int port);

    void setInstrument(int channel, const std::string &id);

    void loadDrumSample(int id, const float *data, int len);

    void noteOn(int channel, int note, int velocity);

    void noteOff(int channel, int note);

    void controlChange(int channel, int cc, int value);

    void setLoopStateListener(JNIEnv *env, jobject jCallback);

    void loopRecord();

    void loopPlay();

    void loopStop();

    void loopClear();

    int loopState() const;

private:
    InstrumentRepository mRepository;
    MidiEngine mMidiEngine;
    std::unique_ptr<OboeOutput> mOboeOutput;
    std::unique_ptr<WifiOutput> mWifiOutput;
};
