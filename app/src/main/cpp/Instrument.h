#pragma once
#include <cstdint>

class Instrument {
public:
    virtual ~Instrument() = default;
    virtual void noteOn(int channel, int note, int velocity) = 0;
    virtual void noteOff(int channel, int note) = 0;
    virtual void render(float* buffer, int32_t frames) = 0;
};
