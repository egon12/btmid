#pragma once
#include <amidi/AMidi.h>
#include <jni.h>
#include <atomic>
#include "AudioEngine.h"
#include "SpscRing.h"
#include "LoopRecorder.h"

// Partial AudioEngine implementation shared by OboeEngine and WifiEngine:
//   - channel-indexed instrument routing (mChannels[16])
//   - AMidi port open/close
//   - JNI callback registration
//   - pollMidi() / renderChannels() helpers for subclass render loops
class ChannelEngine : public AudioEngine {
public:
    ChannelEngine() = default;
    ~ChannelEngine() override = default;

    ChannelEngine(const ChannelEngine&) = delete;
    ChannelEngine& operator=(const ChannelEngine&) = delete;

    void setInstrument(int channel, Instrument* instrument) override;
    void noteOn(int channel, int note, int velocity) override;
    void noteOff(int channel, int note) override;
    void controlChange(int channel, int cc, int value) override;
    void loopStartRecord() override;
    void loopStopRecord()  override;
    void loopClear()       override;
    int  loopState()       override;
    void loopRecordEvent(MidiMsgType type, uint8_t channel, uint8_t note, uint8_t vel) override;
    void setOutputPort(JNIEnv* env, jobject jDevice, jobject jCallback) override;
    void clearOutputPort() override;

protected:
    // Drain all pending AMidi messages, route to noteOn/Off/CC, push to mEventQueue.
    void pollMidi();
    // Render each unique instrument in mChannels into buf (deduplicates by pointer).
    void renderChannels(float* buf, int32_t frames);

    std::atomic<Instrument*>      mChannels[16] {};

    AMidiDevice*                  mNativeDevice  {nullptr};
    std::atomic<AMidiOutputPort*> mMidiPort      {nullptr};
    uint8_t                       mRunningStatus {0};

    SpscRing<MidiEvt, 256>        mEventQueue;

    void advanceLoop(int32_t frames);

    LoopRecorder mLoopRecorder;

    JavaVM*    mJvm           {nullptr};
    jobject    mMidiCallback  {nullptr};
    jmethodID  mOnMidiEventId {nullptr};
};
