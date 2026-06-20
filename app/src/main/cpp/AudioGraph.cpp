#include "AudioGraph.h"
#include "OboeEngine.h"

AudioGraph::AudioGraph() : mEngine(std::make_unique<OboeEngine>()) {
    mRepository.setInstrument(*mEngine, 0, "mono_osc");
    mRepository.setInstrument(*mEngine, 9, "noise_drum");
}

AudioGraph::~AudioGraph() {
    // Explicitly stop the engine before members destruct, ensuring the audio
    // thread and dispatch thread are dead before instruments are destroyed.
    mEngine->stop();
    mEngine->clearOutputPort();
}

void AudioGraph::start() { mEngine->start(); }
void AudioGraph::stop()  { mEngine->stop();  }

void AudioGraph::setEngine(std::unique_ptr<AudioEngine> engine) {
    mEngine->stop();
    mEngine->clearOutputPort();
    mEngine = std::move(engine);
}

void AudioGraph::setInstrument(int channel, const std::string& id) {
    mRepository.setInstrument(*mEngine, channel, id);
}

void AudioGraph::loadDrumSample(int id, const float* data, int len) {
    mRepository.loadDrumSample(id, data, len);
    mEngine->loadSample(id, data, len);
}

void AudioGraph::noteOn(int ch, int note, int vel) {
    mEngine->noteOn(ch, note, vel);
    if (ch == 9) mEngine->loopRecordEvent(0x90, note, vel);
}
void AudioGraph::noteOff(int ch, int note) {
    mEngine->noteOff(ch, note);
    if (ch == 9) mEngine->loopRecordEvent(0x80, note, 0);
}
void AudioGraph::controlChange(int ch, int cc, int v) { mEngine->controlChange(ch, cc, v);    }

void AudioGraph::openMidiDevice(JNIEnv* env, jobject jDevice, jobject jCallback) {
    mEngine->setOutputPort(env, jDevice, jCallback);
}

void AudioGraph::closeMidiDevice() {
    mEngine->clearOutputPort();
}

void AudioGraph::loopStartRecord() { mEngine->loopStartRecord(); }
void AudioGraph::loopStopRecord()  { mEngine->loopStopRecord();  }
void AudioGraph::loopClear()       { mEngine->loopClear();       }
int  AudioGraph::loopState()       { return mEngine->loopState(); }
void AudioGraph::loopRecordEvent(uint8_t type, uint8_t note, uint8_t vel) {
    mEngine->loopRecordEvent(type, note, vel);
}
