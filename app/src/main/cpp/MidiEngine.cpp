#include "MidiEngine.h"
#include "UICallback.h"
#include "MidiParser.h"
#include <android/log.h>

#define LOG_TAG "ChannelEngine"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

MidiEngine::MidiEngine() {
    loopRecorder.onStateChange = [this](LoopRecorder::State s) {
        uiCallback.onLoopState(s);
    };
}

void MidiEngine::setInstrument(int channel, Instrument *instrument) {
    if (channel >= 0 && channel < 16)
        mChannels[channel].store(instrument, std::memory_order_release);
}

void MidiEngine::noteOn(int channel, int note, int velocity) {
    if (channel < 0 || channel >= 16) return;
    Instrument *inst = mChannels[channel].load(std::memory_order_acquire);
    if (inst) inst->noteOn(channel, note, velocity);

    auto msg = MidiMsg{MidiMsgType::NoteOn,
                       static_cast<uint8_t>(channel),
                       static_cast<uint8_t>(note),
                       static_cast<uint8_t>(velocity)};

    loopRecorder.onUiMidiEvent(msg);
}

void MidiEngine::noteOff(int channel, int note) {
    if (channel < 0 || channel >= 16) return;
    Instrument *inst = mChannels[channel].load(std::memory_order_acquire);
    if (inst) inst->noteOff(channel, note);

    auto msg = MidiMsg{MidiMsgType::NoteOff,
                       static_cast<uint8_t>(channel),
                       static_cast<uint8_t>(note),
                       static_cast<uint8_t>(0)};

    loopRecorder.onUiMidiEvent(msg);

}

void MidiEngine::controlChange(int channel, int cc, int value) {
    if (channel < 0 || channel >= 16) return;
    Instrument *inst = mChannels[channel].load(std::memory_order_acquire);
    if (inst) inst->controlChange(channel, cc, value);

    auto msg = MidiMsg{MidiMsgType::NoteOff,
                       static_cast<uint8_t>(channel),
                       static_cast<uint8_t>(cc),
                       static_cast<uint8_t>(value)};

    loopRecorder.onUiMidiEvent(msg);
}

void MidiEngine::openMidiDevice(JNIEnv *env, jobject jDevice, jobject jListener) {
    closeMidiDevice();

    media_status_t status = AMidiDevice_fromJava(env, jDevice, &mNativeDevice);
    if (status != AMEDIA_OK) {
        LOGE("AMidiDevice_fromJava failed: %d", status);
        return;
    }
    AMidiOutputPort *port = nullptr;
    status = AMidiOutputPort_open(mNativeDevice, 0, &port);
    if (status != AMEDIA_OK) {
        LOGE("AMidiOutputPort_open failed: %d", status);
        AMidiDevice_release(mNativeDevice);
        mNativeDevice = nullptr;
        return;
    }
    mMidiPort.store(port, std::memory_order_release);
    mRunningStatus = 0;

    uiCallback.setMidiEventListener(env, jListener);

    LOGD("AMidi output port set");
}

void MidiEngine::closeMidiDevice() {
    uiCallback.clearMidiEventListener();

    AMidiOutputPort *port = mMidiPort.exchange(nullptr, std::memory_order_acq_rel);
    if (port) AMidiOutputPort_close(port);
    if (mNativeDevice) {
        AMidiDevice_release(mNativeDevice);
        mNativeDevice = nullptr;
    }
    mRunningStatus = 0;
}

void MidiEngine::pollMidi() {
    AMidiOutputPort *midiPort = mMidiPort.load(std::memory_order_acquire);
    if (!midiPort) return;

    uint8_t midiBuf[64];
    int32_t opcode;
    size_t numBytes;
    int64_t timestamp;
    ssize_t n;
    while ((n = AMidiOutputPort_receive(
            midiPort, &opcode, midiBuf, sizeof(midiBuf), &numBytes, &timestamp)) > 0) {
        MidiMsg msgs[16];
        int count = parseMidi(midiBuf, numBytes, msgs, 16, mRunningStatus);
        for (int k = 0; k < count; ++k) {
            const MidiMsg &m = msgs[k];
            if (m.type == MidiMsgType::NoteOn)
                noteOn(m.channel, m.data1, m.data2);
            else if (m.type == MidiMsgType::NoteOff)
                noteOff(m.channel, m.data1);
            else if (m.type == MidiMsgType::CC)
                controlChange(m.channel, m.data1, m.data2);

            loopRecorder.onMidiEvent(m, timestamp);

            MidiEvt evt{m.channel, static_cast<uint8_t>(m.type),
                        m.data1, m.data2};
            mEventQueue.push(evt);
            uiCallback.onMidiEvent(evt);
        }
    }
}

void MidiEngine::render(float *buf, int32_t frames) {
    pollMidi();
    advanceLoop(frames);
    renderAudio(buf, frames);
}

void MidiEngine::renderAudio(float *buf, int32_t frames) {
    Instrument *rendered[16]{};
    int renderCount = 0;
    for (auto &ch: mChannels) {
        Instrument *inst = ch.load(std::memory_order_acquire);
        if (!inst) continue;
        bool seen = false;
        for (int j = 0; j < renderCount; ++j)
            if (rendered[j] == inst) {
                seen = true;
                break;
            }
        if (!seen) {
            rendered[renderCount++] = inst;
            inst->render(buf, frames);
        }
    }
}

void MidiEngine::advanceLoop(int32_t frames) {
    loopRecorder.advance(frames, [this](MidiMsg msg) {
        Instrument *inst = mChannels[msg.channel].load(std::memory_order_acquire);
        if (!inst) return;

        switch (msg.type) {
            case MidiMsgType::NoteOff:
                inst->noteOff(msg.channel, msg.data1);
                break;
            case MidiMsgType::NoteOn:
                inst->noteOn(msg.channel, msg.data1, msg.data2);
                break;
            case MidiMsgType::CC:
                inst->controlChange(msg.channel, msg.data1, msg.data2);
                break;
        }
    });
}
