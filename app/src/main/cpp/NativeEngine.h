#pragma once
#include <oboe/Oboe.h>
#include <amidi/AMidi.h>
#include <jni.h>
#include <atomic>
#include <memory>
#include <thread>
#include "Instrument.h"
#include "MidiParser.h"
#include "SpscRing.h"

struct MidiEvt { uint8_t channel; uint8_t type; uint8_t data1; uint8_t data2; };

class NativeEngine : public oboe::AudioStreamDataCallback {
public:
    NativeEngine();
    ~NativeEngine() override;

    NativeEngine(const NativeEngine&) = delete;
    NativeEngine& operator=(const NativeEngine&) = delete;

    void start();
    void stop();

    // Called by InstrumentRepository to wire a channel to an instrument.
    void setInstrument(int channel, Instrument* instrument);

    void noteOn(int channel, int note, int velocity);
    void noteOff(int channel, int note);
    void controlChange(int channel, int cc, int value);

    void setOutputPort(JNIEnv* env, jobject jDevice, jobject jCallback);
    void clearOutputPort();

    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream* stream,
            void* audioData,
            int32_t numFrames) override;

private:
    void dispatchLoop();

    std::shared_ptr<oboe::AudioStream> mStream;
    std::atomic<Instrument*>           mChannels[16] {};

    // AMidi — mMidiPort atomic so onAudioReady and clearOutputPort don't race
    AMidiDevice*                  mNativeDevice  {nullptr};
    std::atomic<AMidiOutputPort*> mMidiPort      {nullptr};
    uint8_t                       mRunningStatus {0};

    // Lock-free queue: audio thread pushes, dispatch thread pops
    SpscRing<MidiEvt, 256> mEventQueue;

    // JNI callback for UI event log
    JavaVM*              mJvm              {nullptr};
    jobject              mMidiCallback     {nullptr};
    jmethodID            mOnMidiEventId    {nullptr};
    std::thread          mDispatchThread;
    std::atomic<bool>    mDispatchRunning  {false};
};
