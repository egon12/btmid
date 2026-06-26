#include "AudioGraph.h"
#include "outputs/OboeOutput.h"
#include "outputs/WifiEngine.h"
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
    if (mOboeOutput) {
        mOboeOutput->stop();
    }
}

void AudioGraph::setEngine(int engineId, const std::string& host, int port) {
    if (mOboeOutput) {
        mOboeOutput->stop();
        mOboeOutput.reset();
    }
    mMidiEngine->clearOutputPort();
    mUICallback->stop();

    if (engineId == 1) {
        mMidiEngine = std::make_shared<MidiEngine>();
        mMidiEngine->setUICallback(mUICallback.get());
        mOboeOutput = std::make_unique<OboeOutput>(mMidiEngine);
        mOboeOutput->start();
    } else if (engineId == 2) {
        auto wifi = std::make_shared<WifiEngine>(host, port);
        wifi->setUICallback(mUICallback.get());
        wifi->start();
        mMidiEngine = wifi;
    }

    mRepository.setInstrument(*mMidiEngine, 0, "piano");
    mRepository.setInstrument(*mMidiEngine, 9, "noise_drum");
}

void AudioGraph::setInstrument(int channel, const std::string& id) {
    mRepository.setInstrument(*mMidiEngine, channel, id);
}

void AudioGraph::loadDrumSample(int id, const float* data, int len) {
    mRepository.loadDrumSample(id, data, len);
    mMidiEngine->loadSample(id, data, len);
}

void AudioGraph::noteOn(int ch, int note, int vel) {
    mMidiEngine->noteOn(ch, note, vel);
    mMidiEngine->loopRecordEvent(MidiMsgType::NoteOn, ch, note, vel);
}

void AudioGraph::noteOff(int ch, int note) {
    mMidiEngine->noteOff(ch, note);
    mMidiEngine->loopRecordEvent(MidiMsgType::NoteOff, ch, note, 0);
}

void AudioGraph::controlChange(int ch, int cc, int v) {
    mMidiEngine->controlChange(ch, cc, v);
}

void AudioGraph::openMidiDevice(JNIEnv* env, jobject jDevice, jobject jCallback) {
    mMidiEngine->setOutputPort(env, jDevice, jCallback);
}

void AudioGraph::closeMidiDevice() {
    mMidiEngine->clearOutputPort();
}

void AudioGraph::loopStartRecord() { mMidiEngine->loopStartRecord(); }
void AudioGraph::loopStopRecord()  { mMidiEngine->loopStopRecord();  }
void AudioGraph::loopClear()       { mMidiEngine->loopClear();       }
int  AudioGraph::loopState()       { return mMidiEngine->loopState(); }
void AudioGraph::loopRecordEvent(uint8_t type, uint8_t note, uint8_t vel) {
    mMidiEngine->loopRecordEvent(static_cast<MidiMsgType>(type), 0, note, vel);
}