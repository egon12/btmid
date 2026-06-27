#include "AudioGraph.h"
#include "outputs/OboeOutput.h"
#include "outputs/WifiOutput.h"
#include "UICallback.h"

AudioGraph::AudioGraph()
        : mUICallback(std::make_unique<UICallback>()),
          mMidiEngine(std::make_shared<MidiEngine>()) {
    mMidiEngine->setUICallback(mUICallback.get());
    mOboeOutput = std::make_unique<OboeOutput>(mMidiEngine);
    mRepository.setInstrument(*mMidiEngine, 0, "piano");
    mRepository.setInstrument(*mMidiEngine, 9, "noise_drum");
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
    mMidiEngine->clearOutputPort();
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

void AudioGraph::setEngine(int engineId, const std::string &host, int port) {
    if (mWifiOutput) {
        mWifiOutput->stop();
        mWifiOutput.reset();
    }
    if (mOboeOutput) {
        mOboeOutput->stop();
        mOboeOutput.reset();
    }
    mMidiEngine->clearOutputPort();
    mUICallback->stop();

    mMidiEngine = std::make_shared<MidiEngine>();
    mMidiEngine->setUICallback(mUICallback.get());

    if (engineId == 1) {
        mOboeOutput = std::make_unique<OboeOutput>(mMidiEngine);
        mOboeOutput->start();
    } else if (engineId == 2) {
        mWifiOutput = std::make_unique<WifiOutput>(mMidiEngine, host, port);
        mWifiOutput->start();
    }

    mRepository.setInstrument(*mMidiEngine, 0, "piano");
    mRepository.setInstrument(*mMidiEngine, 9, "noise_drum");
}

void AudioGraph::setInstrument(int channel, const std::string &id) {
    mRepository.setInstrument(*mMidiEngine, channel, id);
}

void AudioGraph::loadDrumSample(int id, const float *data, int len) {
    mRepository.loadDrumSample(id, data, len);
}

void AudioGraph::noteOn(int ch, int note, int vel) {
    mMidiEngine->noteOn(ch, note, vel);
}

void AudioGraph::noteOff(int ch, int note) {
    mMidiEngine->noteOff(ch, note);
}

void AudioGraph::controlChange(int ch, int cc, int v) {
    mMidiEngine->controlChange(ch, cc, v);
}

void AudioGraph::openMidiDevice(JNIEnv *env, jobject jDevice, jobject jCallback) {
    mMidiEngine->setOutputPort(env, jDevice, jCallback);
}

void AudioGraph::closeMidiDevice() {
    mMidiEngine->clearOutputPort();
}

void AudioGraph::setLoopStateListener(JNIEnv *env, jobject jCallback) {
    mMidiEngine->uiCallback->setLoopStateListener(env, jCallback);
}


void AudioGraph::loopStartRecord() { mMidiEngine->loopRecorder.startRecording(); }

void AudioGraph::loopStopRecord() { mMidiEngine->loopRecorder.stopRecording(); }

void AudioGraph::loopClear() { mMidiEngine->loopRecorder.clear(); }

int AudioGraph::loopState() { return static_cast<int>(mMidiEngine->loopRecorder.state()); }
