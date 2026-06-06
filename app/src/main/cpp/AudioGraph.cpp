#include "AudioGraph.h"
#include "OboeEngine.h"

AudioGraph::AudioGraph() : mEngine(std::make_unique<OboeEngine>()) {
    mRepository.setInstrument(*mEngine, 0, "piano");
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

void AudioGraph::noteOn(int ch, int note, int vel)    { mEngine->noteOn(ch, note, vel);      }
void AudioGraph::noteOff(int ch, int note)            { mEngine->noteOff(ch, note);           }
void AudioGraph::controlChange(int ch, int cc, int v) { mEngine->controlChange(ch, cc, v);    }

void AudioGraph::openMidiDevice(JNIEnv* env, jobject jDevice, jobject jCallback) {
    mEngine->setOutputPort(env, jDevice, jCallback);
}

void AudioGraph::closeMidiDevice() {
    mEngine->clearOutputPort();
}
