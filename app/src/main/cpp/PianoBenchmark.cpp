#include "PianoBenchmark.h"
#include "instruments/Piano.h"
#include "instruments/PianoSinTable.h"
#include <chrono>
#include <cstring>
#include <cstdio>

// Renders kRuns * kSampleRate frames and returns average ms per 1-second render.
static double timeRender(Instrument& inst, float* buf) {
    constexpr int kFrames = kSampleRate;
    constexpr int kRuns   = 10;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int r = 0; r < kRuns; ++r) {
        std::memset(buf, 0, kFrames * sizeof(float));
        inst.render(buf, kFrames);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count() / kRuns;
}

std::string runPianoBenchmark() {
    // 192 KB — keep in BSS to avoid stack overflow
    static float buf[kSampleRate];

    // --- Piano: double phases + std::sin() ---
    Piano piano;
    for (int i = 0; i < 8; ++i) piano.noteOn(0, 60 + i, 100);
    piano.render(buf, 1); // flush noteOn queue into voices
    double pianoMs = timeRender(piano, buf);

    // --- PianoSinTable: fixed-point phase + table lookup ---
    PianoSinTable sinTable;
    sinTable.initSinTable();
    for (int i = 0; i < 8; ++i) sinTable.noteOn(0, 60 + i, 100);
    sinTable.render(buf, 1); // flush noteOn queue into voices
    double sinTableMs = timeRender(sinTable, buf);

    bool tableWins = sinTableMs < pianoMs;
    double ratio   = tableWins ? pianoMs / sinTableMs : sinTableMs / pianoMs;
    const char* winner = tableWins ? "SinTable faster" : "sin() faster!";

    char result[256];
    std::snprintf(result, sizeof(result),
        "Piano(sin):   %.2f ms/s\n"
        "SinTable:     %.2f ms/s\n"
        "Ratio: %.2fx  (%s)",
        pianoMs, sinTableMs, ratio, winner);
    return result;
}
