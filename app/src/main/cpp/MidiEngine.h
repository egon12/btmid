#pragma once

#include <amidi/AMidi.h>
#include <jni.h>
#include <atomic>
#include "AudioEngine.h"
#include "SpscRing.h"
#include "LoopRecorder.h"

class UICallback;

// Partial AudioEngine implementation shared by OboeOutput and WifiEngine:
//   - channel-indexed instrument routing (mChannels[16])
//   - AMidi port open/close
//   - pollMidi() / renderChannels() helpers for subclass render loops
class MidiEngine : public AudioEngine {
public:
    MidiEngine();

    ~MidiEngine() override = default;

    MidiEngine(const MidiEngine &) = delete;

    MidiEngine &operator=(const MidiEngine &) = delete;

    void start() override {}

    void stop() override {}

    void setInstrument(int channel, Instrument *instrument) override;

    void noteOn(int channel, int note, int velocity) override;

    void noteOff(int channel, int note) override;

    void controlChange(int channel, int cc, int value) override;

    void loopStartRecord() override;

    void loopStopRecord() override;

    void loopClear() override;

    int loopState() override;

    void loopRecordEvent(MidiMsgType type, uint8_t channel, uint8_t note, uint8_t vel) override;

    void setOutputPort(JNIEnv *env, jobject jDevice, jobject jCallback) override;

    void clearOutputPort() override;

    void setUICallback(UICallback *callback);

    void pollMidi();

    // Drain all pending AMidi messages, route to noteOn/Off/CC, push to mEventQueue.
    // Render each unique instrument in mChannels into buf (deduplicates by pointer).
    void render(float *buf, int32_t frames);

protected:

    std::atomic<Instrument *> mChannels[16]{};

    AMidiDevice *mNativeDevice{nullptr};
    std::atomic<AMidiOutputPort *> mMidiPort{nullptr};
    uint8_t mRunningStatus{0};

    SpscRing<MidiEvt, 256> mEventQueue;

    void advanceLoop(int32_t frames);

    LoopRecorder mLoopRecorder;

    UICallback *mUICallback{nullptr};
};