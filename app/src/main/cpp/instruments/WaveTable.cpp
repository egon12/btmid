//
// Created by Egon on 7/6/26.
//

#include "WaveTable.h"
#include "../AudioConfig.h"
#include <cstdio>
#include <cstdint>

static constexpr float kTwoPi = 2.0 * M_PI;
static constexpr uint32_t kTableSize = 1024;

static std::once_flag sinTableInitFlag;
static float sinTable[kTableSize];

// sinTable using interpolation linear
float WaveTable::sin(float freq, float t) {
    std::call_once(sinTableInitFlag, initSinTable);

    constexpr uint32_t tableMask = kTableSize - 1; // 0x3FF

    float cycles = freq * t;
    float frac = cycles - floorf(cycles);

    // Map [0, 1) to full uint32 range
    auto phase32 = static_cast<uint32_t>(frac * 4294967296.0f); // 2^32

    auto idx1 = (phase32 >> 22) & tableMask;
    auto idx2 = (idx1 + 1) & tableMask;

    // Fractional part: lower 22 bits, normalized to [0, 1)
    float f = static_cast<float>(phase32 & 0x3FFFFF) * (1.0f / 4194304.0f);

    return sinTable[idx1] + f * (sinTable[idx2] - sinTable[idx1]);
}

float WaveTable::saw(float freq, float t) {
    float cycles = freq * t;
    float frac = cycles - floorf(cycles);
    return frac * 2.0f - 1.0f;
}

float WaveTable::square(float freq, float t) {
    float cycles = freq * t;
    float frac = cycles - floorf(cycles);
    if (frac < 0.5f) return 1.0f;
    return -1.0f;
}

void WaveTable::initSinTable() {
    for (int i = 0; i < kTableSize; i++) {
        sinTable[i] = std::sinf(static_cast<float>(i) * kTwoPi / kTableSize);
    }
}

