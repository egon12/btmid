#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include "SineTestRenderer.h"
#include "renderers/NoiseDrumRenderer.h"
#include "wav_writer.h"

static int gPass = 0, gFail = 0;

#define CHECK(cond, msg) \
    do { \
        if (cond) { printf("PASS  %s\n", (msg)); ++gPass; } \
        else      { printf("FAIL  %s\n", (msg)); ++gFail; } \
    } while (0)

static float maxAmp(const float* buf, int n) {
    float m = 0.0f;
    for (int i = 0; i < n; ++i) m = std::max(m, std::abs(buf[i]));
    return m;
}

static bool allZero(const float* buf, int n) {
    for (int i = 0; i < n; ++i) if (buf[i] != 0.0f) return false;
    return true;
}

// --- tests ---

static void test_silence_before_noteOn() {
    SineTestRenderer r;
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(allZero(buf, 256), "silence before noteOn");
}

static void test_sound_after_noteOn() {
    SineTestRenderer r;
    r.noteOn(0, 60, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.1f, "audible output after noteOn");
}

static void test_silence_after_500ms() {
    SineTestRenderer r;
    r.noteOn(0, 60, 100);
    // drain exactly 500ms
    const int drain = 44100 / 2;
    std::vector<float> tmp(drain, 0.0f);
    for (int i = 0; i < drain; i += 256) {
        int n = std::min(256, drain - i);
        r.render(tmp.data() + i, n);
    }
    float tail[256] = {};
    r.render(tail, 256);
    CHECK(allZero(tail, 256), "silence after 500 ms decay");
}

static void test_second_noteOn_restarts() {
    SineTestRenderer r;
    r.noteOn(0, 60, 100);
    // drain 400ms (still playing)
    const int drain = 44100 * 4 / 10;
    std::vector<float> tmp(drain, 0.0f);
    for (int i = 0; i < drain; i += 256) {
        int n = std::min(256, drain - i);
        r.render(tmp.data() + i, n);
    }
    // second noteOn resets timer; drain another 450ms
    r.noteOn(0, 60, 100);
    const int drain2 = 44100 * 45 / 100;
    std::vector<float> tmp2(drain2, 0.0f);
    for (int i = 0; i < drain2; i += 256) {
        int n = std::min(256, drain2 - i);
        r.render(tmp2.data() + i, n);
    }
    float check[256] = {};
    r.render(check, 256);
    CHECK(maxAmp(check, 256) > 0.0f, "second noteOn restarts 500 ms window");
}

// --- WAV output for listening ---

static void write_wav_sine() {
    SineTestRenderer r;
    const int samples = 44100;
    std::vector<float> out(samples, 0.0f);
    r.noteOn(0, 60, 100);
    for (int i = 0; i < samples; i += 256) {
        int n = std::min(256, samples - i);
        r.render(out.data() + i, n);
    }
    writeWav("sine_test.wav", out.data(), samples, 44100);
    printf("INFO  wrote sine_test.wav  (open to hear 440 Hz sine, 500 ms)\n");
}

static void write_wav_poly() {
    // Two notes overlapping: noteOn at t=0 and t=250ms
    SineTestRenderer r;
    const int samples = 44100;
    std::vector<float> out(samples, 0.0f);

    r.noteOn(0, 60, 100);
    const int half = samples / 4; // 250ms
    for (int i = 0; i < samples; i += 256) {
        if (i == half) r.noteOn(0, 60, 100); // restart mid-way
        int n = std::min(256, samples - i);
        r.render(out.data() + i, n);
    }
    writeWav("sine_restart.wav", out.data(), samples, 44100);
    printf("INFO  wrote sine_restart.wav (noteOn re-triggered at 250 ms)\n");
}

// --- NoiseDrumRenderer helpers ---

static void renderDrum(NoiseDrumRenderer& r, int note, int velocity,
                       float* out, int samples) {
    r.noteOn(0, note, velocity);
    for (int i = 0; i < samples; i += 256) {
        int n = std::min(256, samples - i);
        r.render(out + i, n);
    }
}

// --- NoiseDrumRenderer tests ---

static void test_drum_silence_before_noteOn() {
    NoiseDrumRenderer r;
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(allZero(buf, 256), "drum: silence before any noteOn");
}

static void test_drum_kick_sound() {
    NoiseDrumRenderer r;
    r.noteOn(0, 36, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.05f, "drum: kick (note 36) produces sound");
}

static void test_drum_snare_sound() {
    NoiseDrumRenderer r;
    r.noteOn(0, 38, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.05f, "drum: snare (note 38) produces sound");
}

static void test_drum_closed_hat_sound() {
    NoiseDrumRenderer r;
    r.noteOn(0, 42, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.01f, "drum: closed hat (note 42) produces sound");
}

static void test_drum_open_hat_sound() {
    NoiseDrumRenderer r;
    r.noteOn(0, 46, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.05f, "drum: open hat (note 46) produces sound");
}

static void test_drum_crash_sound() {
    NoiseDrumRenderer r;
    r.noteOn(0, 49, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.05f, "drum: crash (note 49) produces sound");
}

static void test_drum_ride_sound() {
    NoiseDrumRenderer r;
    r.noteOn(0, 51, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.05f, "drum: ride (note 51) produces sound");
}

static void test_drum_kick_decays() {
    // Kick has 150 ms exponential decay — voice reaches the 1e-4 isDone threshold
    // at ~1.3 s. Drain 1500 ms to confirm the voice has gone fully silent.
    NoiseDrumRenderer r;
    r.noteOn(0, 36, 127);
    const int drain = 44100 * 1500 / 1000;
    std::vector<float> tmp(drain, 0.0f);
    for (int i = 0; i < drain; i += 256) {
        int n = std::min(256, drain - i);
        r.render(tmp.data() + i, n);
    }
    float tail[256] = {};
    r.render(tail, 256);
    CHECK(maxAmp(tail, 256) < 1e-3f, "drum: kick is silent after 1500 ms");
}

// --- WAV output ---

static void write_wav_noise_drums() {
    NoiseDrumRenderer r;
    // Each drum hit at 0.5s intervals; total 4 seconds
    const int sampleRate  = 44100;
    const int totalSamples = sampleRate * 4;
    const int hitInterval  = sampleRate / 2;
    std::vector<float> out(totalSamples, 0.0f);

    // GM notes: kick, snare, closed hat, open hat, crash, ride, tom (47)
    const int hits[] = {36, 38, 42, 46, 49, 51, 47};
    int hitCount = (int)(sizeof(hits) / sizeof(hits[0]));

    for (int i = 0; i < totalSamples; i += 256) {
        int pos = i / hitInterval;
        if (i % hitInterval < 256 && pos < hitCount) {
            r.noteOn(0, hits[pos], 100);
        }
        int n = std::min(256, totalSamples - i);
        r.render(out.data() + i, n);
    }

    writeWav("noise_drums.wav", out.data(), totalSamples, sampleRate);
    printf("INFO  wrote noise_drums.wav  (kick snare hat openhat crash ride tom)\n");
}

int main() {
    printf("=== SineTestRenderer ===\n");
    test_silence_before_noteOn();
    test_sound_after_noteOn();
    test_silence_after_500ms();
    test_second_noteOn_restarts();

    printf("\n=== NoiseDrumRenderer ===\n");
    test_drum_silence_before_noteOn();
    test_drum_kick_sound();
    test_drum_snare_sound();
    test_drum_closed_hat_sound();
    test_drum_open_hat_sound();
    test_drum_crash_sound();
    test_drum_ride_sound();
    test_drum_kick_decays();

    printf("\n=== WAV output ===\n");
    write_wav_sine();
    write_wav_poly();
    write_wav_noise_drums();

    printf("\n%d passed, %d failed\n", gPass, gFail);
    return gFail > 0 ? 1 : 0;
}
