#include "AudioGraph.h"
#include "outputs/OboeOutput.h"
#include "outputs/WifiOutput.h"
#include "UICallback.h"

AudioGraph::AudioGraph() {
    mOboeOutput = std::make_unique<OboeOutput>(&mMidiEngine);
    mRepository.setInstrument(mMidiEngine, 0, "piano");
    mRepository.setInstrument(mMidiEngine, 9, "noise_drum");
}

AudioGraph::~AudioGraph() {
    if (mWifiOutput) {
        mWifiOutput->stop();
        mWifiOutput.reset();
    }
    if (mOboeOutput) {
        mOboeOutput->stop();
        mOboeOutput.reset();
    }
    mMidiEngine.closeMidiDevice();
}

void AudioGraph::start() {
    if (mOboeOutput) mOboeOutput->start();
}

void AudioGraph::stop() {
    if (mWifiOutput) {
        mWifiOutput->stop();
        mWifiOutput.reset();
    }
    if (mOboeOutput) {
        mOboeOutput->stop();
        mOboeOutput.reset();
    }
}

void AudioGraph::setOutput(int outputId, const std::string &host, int port) {
    if (mWifiOutput) {
        mWifiOutput->stop();
        mWifiOutput.reset();
    }
    if (mOboeOutput) {
        mOboeOutput->stop();
        mOboeOutput.reset();
    }

    if (outputId == 1) {
        mOboeOutput = std::make_unique<OboeOutput>(&mMidiEngine);
        mOboeOutput->start();
    } else if (outputId == 2) {
        mWifiOutput = std::make_unique<WifiOutput>(&mMidiEngine, host, port);
        mWifiOutput->start();
    }
}

void AudioGraph::setInstrument(int channel, const std::string &id) {
    mRepository.setInstrument(mMidiEngine, channel, id);
}

void AudioGraph::setChannelGain(int channel, float gain) {
    mMidiEngine.setChannelGain(channel, gain);
}

void AudioGraph::loadDrumSample(int id, const float *data, int len) {
    mRepository.loadDrumSample(id, data, len);
}

void AudioGraph::noteOn(int ch, int note, int vel) {
    mMidiEngine.noteOn(ch, note, vel);
}

void AudioGraph::noteOff(int ch, int note) {
    mMidiEngine.noteOff(ch, note);
}

void AudioGraph::controlChange(int ch, int cc, int v) {
    mMidiEngine.controlChange(ch, cc, v);
}

void AudioGraph::openMidiDevice(JNIEnv *env, jobject jDevice, jobject jListener) {
    mMidiEngine.openMidiDevice(env, jDevice, jListener);
}

void AudioGraph::closeMidiDevice() {
    mMidiEngine.closeMidiDevice();
}

void AudioGraph::setLoopStateListener(JNIEnv *env, jobject jCallback) {
    mMidiEngine.uiCallback.setLoopStateListener(env, jCallback);
}

void AudioGraph::loopRecord() { mMidiEngine.loopRecorder.rec(); }

void AudioGraph::loopPlay() { mMidiEngine.loopRecorder.play(); }

void AudioGraph::loopStop() { mMidiEngine.loopRecorder.stop(); }

void AudioGraph::loopClear() { mMidiEngine.loopRecorder.clear(); }

int AudioGraph::loopState() const { return static_cast<int>(mMidiEngine.loopRecorder.state()); }
