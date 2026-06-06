#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include "Instrument.h"

class NativeEngine;
class SampleDrum;

class InstrumentRepository {
public:
    InstrumentRepository();

    // Lazy-create instrument by id and register it on the given channel.
    // Known ids: "piano", "noise_drum", "fm_drum", "sample_drum"
    void setInstrument(NativeEngine& engine, int channel, const std::string& id);

    void loadDrumSample(int id, const float* data, int len);

private:
    Instrument* getOrCreate(const std::string& id);

    std::unordered_map<std::string, std::unique_ptr<Instrument>> mInstruments;
    SampleDrum* mSampleDrum {nullptr};
};
