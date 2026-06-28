#pragma once

#include <amidi/AMidi.h>
#include <jni.h>
#include <atomic>
#include "SpscRing.h"
#include "LoopRecorder.h"
#include "UICallback.h"
#include "MidiEvt.h"
#include "Instrument.h"

// Midi Engine
//   - channel-indexed instrument routing (mChannels[16])
//   - AMidi port open/close
//   - pollMidi() / renderChannels() helpers for subclass render loops
class MidiEngine  {
public:
    MidiEngine();

    ~MidiEngine() = default;

    MidiEngine(const MidiEngine &) = delete;

    MidiEngine &operator=(const MidiEngine &) = delete;

    void start() {}

    void stop() {}

    void setInstrument(int channel, Instrument *instrument);

    void noteOn(int channel, int note, int velocity);

    void noteOff(int channel, int note);

    void controlChange(int channel, int cc, int value);

    void openMidiDevice(JNIEnv *env, jobject jDevice, jobject jListener);

    void closeMidiDevice();

    // Drain all pending AMidi messages, route to noteOn/Off/CC, push to mEventQueue.
    // Render each unique instrument in mChannels into buf (deduplicates by pointer).
    void render(float *buf, int32_t frames);

    LoopRecorder loopRecorder{};

    UICallback uiCallback{};

protected:
    std::atomic<Instrument *> mChannels[16]{};

    AMidiDevice *mNativeDevice{nullptr};
    std::atomic<AMidiOutputPort *> mMidiPort{nullptr};
    uint8_t mRunningStatus{0};

    void pollMidi();

    void advanceLoop(int32_t frames);

    void renderAudio(float *buf, int32_t frames);
};