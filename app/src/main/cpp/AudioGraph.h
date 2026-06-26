#pragma once
#include <jni.h>
#include <memory>
#include <string>
#include "AudioEngine.h"
#include "InstrumentRepository.h"

class MidiEngine;
class UICallback;
class OboeOutput;

class AudioGraph {
public:
    AudioGraph();
    ~AudioGraph();

    AudioGraph(const AudioGraph&) = delete;
    AudioGraph& operator=(const AudioGraph&) = delete;

    void start();
    void stop();

    void openMidiDevice(JNIEnv* env, jobject jDevice, jobject jCallback);
    void closeMidiDevice();

    void setEngine(int engineId, const std::string& host, int port);

    void setInstrument(int channel, const std::string& id);
    void loadDrumSample(int id, const float* data, int len);

    void noteOn(int channel, int note, int velocity);
    void noteOff(int channel, int note);
    void controlChange(int channel, int cc, int value);

    void loopStartRecord();
    void loopStopRecord();
    void loopClear();
    int  loopState();
    void loopRecordEvent(uint8_t type, uint8_t note, uint8_t vel);

private:
    InstrumentRepository         mRepository;
    std::unique_ptr<UICallback>  mUICallback;
    std::shared_ptr<MidiEngine>  mMidiEngine;
    std::unique_ptr<OboeOutput>  mOboeOutput;
};
