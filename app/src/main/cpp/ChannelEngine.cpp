#include "ChannelEngine.h"
#include "MidiParser.h"
#include <android/log.h>

#define LOG_TAG "ChannelEngine"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

void ChannelEngine::setInstrument(int channel, Instrument *instrument) {
    if (channel >= 0 && channel < 16)
        mChannels[channel].store(instrument, std::memory_order_release);
}

void ChannelEngine::noteOn(int channel, int note, int velocity) {
    if (channel < 0 || channel >= 16) return;
    Instrument *inst = mChannels[channel].load(std::memory_order_acquire);
    if (inst) inst->noteOn(channel, note, velocity);
}

void ChannelEngine::noteOff(int channel, int note) {
    if (channel < 0 || channel >= 16) return;
    Instrument *inst = mChannels[channel].load(std::memory_order_acquire);
    if (inst) inst->noteOff(channel, note);
}

void ChannelEngine::controlChange(int channel, int cc, int value) {
    if (channel < 0 || channel >= 16) return;
    Instrument *inst = mChannels[channel].load(std::memory_order_acquire);
    if (inst) inst->controlChange(channel, cc, value);
}

void ChannelEngine::pollMidi() {
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

            mLoopRecorder.onMidiEvent(m, timestamp);

            mEventQueue.push({m.channel, static_cast<uint8_t>(m.type),
                              m.data1, m.data2});
        }
    }
}

void ChannelEngine::renderChannels(float *buf, int32_t frames) {
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

void ChannelEngine::setOutputPort(JNIEnv *env, jobject jDevice, jobject jCallback) {
    clearOutputPort();

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

    env->GetJavaVM(&mJvm);
    mMidiCallback = env->NewGlobalRef(jCallback);
    jclass cls = env->GetObjectClass(jCallback);
    mOnMidiEventId = env->GetMethodID(cls, "onMidiEvent", "(IIII)V");
    env->DeleteLocalRef(cls);

    LOGD("AMidi output port set");
}

void ChannelEngine::loopStartRecord() { mLoopRecorder.startRecordOnPlay(); }

void ChannelEngine::loopStopRecord() { mLoopRecorder.stopRecording(); }

void ChannelEngine::loopClear() { mLoopRecorder.clear(); }

int ChannelEngine::loopState() { return static_cast<int>(mLoopRecorder.state()); }

void ChannelEngine::loopRecordEvent(MidiMsgType type, uint8_t note, uint8_t vel) {
    mLoopRecorder.onUiMidiEvent(type, note, vel);
}

void ChannelEngine::advanceLoop(int32_t frames) {
    mLoopRecorder.advance(frames, [this](MidiMsg msg) {
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

void ChannelEngine::clearOutputPort() {
    AMidiOutputPort *port = mMidiPort.exchange(nullptr, std::memory_order_acq_rel);
    if (port) AMidiOutputPort_close(port);
    if (mNativeDevice) {
        AMidiDevice_release(mNativeDevice);
        mNativeDevice = nullptr;
    }
    mRunningStatus = 0;

    if (mMidiCallback && mJvm) {
        JNIEnv *env = nullptr;
        bool attached = mJvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) == JNI_EDETACHED;
        if (attached) mJvm->AttachCurrentThread(&env, nullptr);
        if (env) env->DeleteGlobalRef(mMidiCallback);
        if (attached) mJvm->DetachCurrentThread();
        mMidiCallback = nullptr;
    }
    mJvm = nullptr;
    mOnMidiEventId = nullptr;
}
