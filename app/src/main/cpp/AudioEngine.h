#pragma once
#include <jni.h>
#include <cstdint>
#include "Instrument.h"
#include "MidiParser.h"

struct MidiEvt { uint8_t channel; uint8_t type; uint8_t data1; uint8_t data2; };

class AudioEngine {
public:
    virtual ~AudioEngine() = default;

    virtual void start() = 0;
    virtual void stop() = 0;

    virtual void noteOn(int channel, int note, int velocity) = 0;
    virtual void noteOff(int channel, int note) = 0;
    virtual void controlChange(int channel, int cc, int value) {}

    // Used by InstrumentRepository to wire externally-owned instruments (OboeEngine).
    // WifiEngine manages its own instruments internally and ignores this.
    virtual void setInstrument(int channel, Instrument* instrument) {}

    // Used by WifiEngine which owns its own SampleDrum.
    // OboeEngine delegates sample loading through InstrumentRepository instead.
    virtual void loadSample(int id, const float* data, int len) {}
    virtual void setDrumBackend(int id) {}

    virtual void setOutputPort(JNIEnv* env, jobject jDevice, jobject jCallback) = 0;
    virtual void clearOutputPort() = 0;

    virtual void loopStartRecord() {}
    virtual void loopStopRecord()  {}
    virtual void loopClear()       {}
    virtual int  loopState()       { return 0; }
    virtual void loopRecordEvent(MidiMsgType type, uint8_t channel, uint8_t note, uint8_t vel) {}
};
