#include "InstrumentRepository.h"
#include "NativeEngine.h"
#include "instruments/Piano.h"
#include "instruments/NoiseDrum.h"
#include "instruments/FmDrum.h"
#include "instruments/SampleDrum.h"

InstrumentRepository::InstrumentRepository() = default;

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
    }
    if (!inst) return nullptr;

    Instrument* ptr = inst.get();
    mInstruments[id] = std::move(inst);
    return ptr;
}

void InstrumentRepository::setInstrument(NativeEngine& engine, int channel, const std::string& id) {
    Instrument* inst = getOrCreate(id);
    if (inst) engine.setInstrument(channel, inst);
}

void InstrumentRepository::loadDrumSample(int id, const float* data, int len) {
    if (mSampleDrum) mSampleDrum->loadSample(id, data, len);
}
