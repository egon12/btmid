#pragma once
#include <oboe/Oboe.h>
#include <amidi/AMidi.h>
#include <jni.h>
#include <memory>
#include <vector>
#include <atomic>
#include <thread>
#include "AudioRenderer.h"
#include "MidiParser.h"
#include "SpscRing.h"
#include "renderers/PianoRenderer.h"
#include "renderers/NoiseDrumRenderer.h"
#include "renderers/FmDrumRenderer.h"
#include "renderers/SampleDrumRenderer.h"

struct MidiEvt { uint8_t channel; uint8_t type; uint8_t data1; uint8_t data2; };

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
    void setDrumBackend(int backendId);

    void setOutputPort(JNIEnv* env, jobject jPort, jobject jCallback);
    void clearOutputPort();

    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream* stream,
            void* audioData,
            int32_t numFrames) override;

private:
    void dispatchLoop();

    std::shared_ptr<oboe::AudioStream>          mStream;
    std::vector<std::unique_ptr<AudioRenderer>> mRenderers;

    PianoRenderer*       mPiano             {nullptr};
    AudioRenderer*       mDrumRenderers[3]  {};
    SampleDrumRenderer*  mSampleDrumRenderer{nullptr};
    std::atomic<int>     mActiveDrum        {0};

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
