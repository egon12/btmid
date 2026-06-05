#include "NativeEngine.h"
#include <android/log.h>
#include <time.h>

#define LOG_TAG "NativeEngine"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

NativeEngine::NativeEngine() {
    auto piano = std::make_unique<Piano>();
    mPiano = piano.get();
    mInstruments.push_back(std::move(piano));

    auto noise = std::make_unique<NoiseDrum>();
    mDrumInstruments[0] = noise.get();
    mInstruments.push_back(std::move(noise));

    auto fm = std::make_unique<FmDrum>();
    mDrumInstruments[1] = fm.get();
    mInstruments.push_back(std::move(fm));

    auto sdr = std::make_unique<SampleDrum>();
    mSampleDrum         = sdr.get();
    mDrumInstruments[2] = sdr.get();
    mInstruments.push_back(std::move(sdr));
}

NativeEngine::~NativeEngine() {
    stop();          // stop audio stream before touching MIDI port
    clearOutputPort();
}

void NativeEngine::start() {
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
           ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
           ->setSharingMode(oboe::SharingMode::Exclusive)
           ->setFormat(oboe::AudioFormat::Float)
           ->setChannelCount(oboe::ChannelCount::Mono)
           ->setSampleRate(44100)
           ->setDataCallback(this);

    oboe::Result result = builder.openStream(mStream);
    if (result != oboe::Result::OK) {
        LOGE("openStream failed: %s", oboe::convertToText(result));
        return;
    }
    result = mStream->requestStart();
    if (result != oboe::Result::OK) {
        LOGE("requestStart failed: %s", oboe::convertToText(result));
        return;
    }
    LOGD("Oboe stream started — sampleRate=%d framesPerBurst=%d",
         mStream->getSampleRate(), mStream->getFramesPerBurst());
}

void NativeEngine::stop() {
    if (mStream) {
        mStream->requestStop();
        mStream->close();
        mStream.reset();
    }
}

void NativeEngine::noteOn(int channel, int note, int velocity) {
    if (channel == 0 && mPiano) {
        mPiano->noteOn(channel, note, velocity);
    } else if (channel == 9) {
        int idx = mActiveDrum.load(std::memory_order_relaxed);
        if (mDrumInstruments[idx]) mDrumInstruments[idx]->noteOn(channel, note, velocity);
    }
}

void NativeEngine::noteOff(int channel, int note) {
    if (channel == 0 && mPiano) {
        mPiano->noteOff(channel, note);
    } else if (channel == 9) {
        int idx = mActiveDrum.load(std::memory_order_relaxed);
        if (mDrumInstruments[idx]) mDrumInstruments[idx]->noteOff(channel, note);
    }
}

void NativeEngine::controlChange(int channel, int cc, int value) {
    if (channel == 0 && mPiano)
        mPiano->controlChange(channel, cc, value);
    else if (channel == 9) {
        int idx = mActiveDrum.load(std::memory_order_relaxed);
        if (mDrumInstruments[idx])
            mDrumInstruments[idx]->controlChange(channel, cc, value);
    }
}

void NativeEngine::loadSample(int sampleId, const float* data, int length) {
    if (mSampleDrum) mSampleDrum->loadSample(sampleId, data, length);
}

void NativeEngine::setDrumBackend(int backendId) {
    if (backendId >= 0 && backendId < 3)
        mActiveDrum.store(backendId, std::memory_order_relaxed);
}

void NativeEngine::setOutputPort(JNIEnv* env, jobject jDevice, jobject jCallback) {
    clearOutputPort();

    media_status_t status = AMidiDevice_fromJava(env, jDevice, &mNativeDevice);
    if (status != AMEDIA_OK) {
        LOGE("AMidiDevice_fromJava failed: %d", status);
        return;
    }
    AMidiOutputPort* port = nullptr;
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

    mDispatchRunning.store(true, std::memory_order_relaxed);
    mDispatchThread = std::thread(&NativeEngine::dispatchLoop, this);

    LOGD("AMidi output port set");
}

void NativeEngine::clearOutputPort() {
    if (mDispatchRunning.exchange(false)) {
        if (mDispatchThread.joinable()) mDispatchThread.join();
    }

    AMidiOutputPort* port = mMidiPort.exchange(nullptr, std::memory_order_acq_rel);
    if (port) AMidiOutputPort_close(port);
    if (mNativeDevice) {
        AMidiDevice_release(mNativeDevice);
        mNativeDevice = nullptr;
    }
    mRunningStatus = 0;

    if (mMidiCallback && mJvm) {
        JNIEnv* env = nullptr;
        mJvm->AttachCurrentThread(&env, nullptr);
        env->DeleteGlobalRef(mMidiCallback);
        mJvm->DetachCurrentThread();
        mMidiCallback  = nullptr;
    }
    mJvm           = nullptr;
    mOnMidiEventId = nullptr;
}

void NativeEngine::dispatchLoop() {
    JNIEnv* env = nullptr;
    if (mJvm) mJvm->AttachCurrentThread(&env, nullptr);

    while (mDispatchRunning.load(std::memory_order_acquire)) {
        MidiEvt evt;
        bool    any = false;
        while (mEventQueue.pop(evt)) {
            any = true;
            if (env && mMidiCallback && mOnMidiEventId) {
                env->CallVoidMethod(mMidiCallback, mOnMidiEventId,
                                   (jint)evt.channel, (jint)evt.type,
                                   (jint)evt.data1,   (jint)evt.data2);
            }
        }
        if (!any) {
            struct timespec ts { 0, 1'000'000 };  // 1 ms
            nanosleep(&ts, nullptr);
        }
    }

    if (mJvm && env) mJvm->DetachCurrentThread();
}

oboe::DataCallbackResult NativeEngine::onAudioReady(
        oboe::AudioStream*, void* audioData, int32_t numFrames) {
    // Drain all pending MIDI messages before rendering
    AMidiOutputPort* midiPort = mMidiPort.load(std::memory_order_acquire);
    if (midiPort) {
        uint8_t  midiBuf[64];
        int32_t  opcode;
        size_t   numBytes;
        int64_t  timestamp;
        ssize_t  n;
        while ((n = AMidiOutputPort_receive(
                midiPort, &opcode, midiBuf, sizeof(midiBuf), &numBytes, &timestamp)) > 0) {
            MidiMsg msgs[16];
            int count = parseMidi(midiBuf, numBytes, msgs, 16, mRunningStatus);
            for (int k = 0; k < count; ++k) {
                const MidiMsg& m = msgs[k];
                if (m.type == MidiMsgType::NoteOn)
                    noteOn(m.channel, m.data1, m.data2);
                else if (m.type == MidiMsgType::NoteOff)
                    noteOff(m.channel, m.data1);
                else if (m.type == MidiMsgType::CC)
                    controlChange(m.channel, m.data1, m.data2);
                mEventQueue.push({ m.channel, static_cast<uint8_t>(m.type),
                                   m.data1, m.data2 });
            }
        }
    }

    auto* buf = static_cast<float*>(audioData);
    for (int i = 0; i < numFrames; ++i) buf[i] = 0.0f;
    for (auto& r : mInstruments) r->render(buf, numFrames);
    return oboe::DataCallbackResult::Continue;
}
