#include "InstrumentRepository.h"
#include "instruments/Piano.h"
#include "instruments/NoiseDrum.h"
#include "instruments/FmDrum.h"
#include "instruments/SampleDrum.h"
#include "instruments/PolyOscillator.h"
#include "instruments/MonoOscillator.h"
#include "instruments/SfInstrument.h"

InstrumentRepository::InstrumentRepository() {
    // Create SampleDrum eagerly so loadDrumSample() always has a target, regardless
    // of whether samples finish loading before or after setInstrument("sample_drum").
    auto sdr = std::make_unique<SampleDrum>();
    mSampleDrum = sdr.get();
    mInstruments["sample_drum"] = std::move(sdr);

    // Same rationale for the SoundFont: loadSoundFont() always has a target.
    auto sf = std::make_unique<SfInstrument>();
    mSoundFont = sf.get();
    mInstruments["soundfont"] = std::move(sf);
}

Instrument* InstrumentRepository::getOrCreate(const std::string& id) {
    auto it = mInstruments.find(id);
    if (it != mInstruments.end()) return it->second.get();

    std::unique_ptr<Instrument> inst;
    if (id == "piano") {
        inst = std::make_unique<Piano>();
    } else if (id == "noise_drum") {
        inst = std::make_unique<NoiseDrum>();
    } else if (id == "fm_drum") {
        inst = std::make_unique<FmDrum>();
    } else if (id == "sample_drum") {
        auto sdr = std::make_unique<SampleDrum>();
        mSampleDrum = sdr.get();
        inst = std::move(sdr);
    } else if (id == "sine_polysynth") {
        inst = std::make_unique<PolyOscillator>(PolyOscillator::Waveform::Sine);
    } else if (id == "saw_polysynth") {
        inst = std::make_unique<PolyOscillator>(PolyOscillator::Waveform::Saw);
    } else if (id == "square_polysynth") {
        inst = std::make_unique<PolyOscillator>(PolyOscillator::Waveform::Square);
    } else if (id == "sine_monosynth") {
        inst = std::make_unique<MonoOscillator>(0.15f, MonoOscillator::Waveform::Sine);
    } else if (id == "saw_monosynth") {
        inst = std::make_unique<MonoOscillator>(0.15f, MonoOscillator::Waveform::Saw);
    } else if (id == "square_monosynth") {
        inst = std::make_unique<MonoOscillator>(0.15f, MonoOscillator::Waveform::Square);
    }
    if (!inst) return nullptr;

    Instrument* ptr = inst.get();
    mInstruments[id] = std::move(inst);
    return ptr;
}

void InstrumentRepository::setInstrument(MidiEngine& engine, int channel, const std::string& id) {
    Instrument* inst = getOrCreate(id);
    if (inst) engine.setInstrument(channel, inst);
}

void InstrumentRepository::loadDrumSample(int id, const float* data, int len) {
    if (mSampleDrum) mSampleDrum->loadSample(id, data, len);
}

void InstrumentRepository::loadSoundFont(const uint8_t* data, int len) {
    if (mSoundFont) mSoundFont->loadSoundFont(data, len);
}
