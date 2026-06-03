#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>
#include <algorithm>

inline void writeWav(const char* path, const float* samples, int numSamples, int sampleRate) {
    std::vector<int16_t> pcm(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        float s = std::max(-1.0f, std::min(1.0f, samples[i]));
        pcm[i] = static_cast<int16_t>(s * 32767.0f);
    }

    uint32_t dataSize   = static_cast<uint32_t>(numSamples) * sizeof(int16_t);
    uint32_t fileSize   = 36 + dataSize;
    uint16_t channels   = 1;
    uint16_t bits       = 16;
    uint32_t sr         = static_cast<uint32_t>(sampleRate);
    uint32_t byteRate   = sr * bits / 8;
    uint16_t blockAlign = bits / 8;
    uint16_t pcmFormat  = 1;
    uint32_t fmtSize    = 16;

    FILE* f = fopen(path, "wb");
    if (!f) { perror("writeWav: fopen"); return; }

    fwrite("RIFF",    1, 4, f); fwrite(&fileSize,   4, 1, f);
    fwrite("WAVE",    1, 4, f);
    fwrite("fmt ",    1, 4, f); fwrite(&fmtSize,    4, 1, f);
    fwrite(&pcmFormat,2, 1, f); fwrite(&channels,   2, 1, f);
    fwrite(&sr,        4, 1, f); fwrite(&byteRate,   4, 1, f);
    fwrite(&blockAlign,2, 1, f); fwrite(&bits,       2, 1, f);
    fwrite("data",    1, 4, f); fwrite(&dataSize,   4, 1, f);
    fwrite(pcm.data(), sizeof(int16_t), numSamples, f);

    fclose(f);
}
