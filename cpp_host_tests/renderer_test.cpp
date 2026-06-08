#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include "instruments/NoiseDrum.h"
#include "instruments/FmDrum.h"
#include "instruments/Piano.h"
#include "instruments/PianoSinTable.h"
#include "instruments/WaveTable.h"
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
static void write_wav_wave_table() {

    const int sampleRate  = kSampleRate;
    const int totalSamples = sampleRate * 2;
    std::vector<float> out_sin(totalSamples, 0.0f);
    std::vector<float> out_saw(totalSamples, 0.0f);
    std::vector<float> out_square(totalSamples, 0.0f);

    const float sampleRateFloat = static_cast<float>(sampleRate);
    for (int i = 0; i < totalSamples; i++) {
	    float t = static_cast<float>(i) / sampleRateFloat;
	    out_sin[i] = 0.7f * WaveTable::sin(440.0f, t);
	    out_saw[i] = 0.7f * WaveTable::saw(440.0f, t);
        out_square[i] = 0.7f * WaveTable::square(440.0f, t);
    }

    auto f = fopen("wave.plt", "wb");
    for (int i = 0; i < 220; i++) {
        fprintf(f, "%f\n", out_saw[i]);
    }
    fclose(f);

    writeWav("wave_table_sin.wav", out_sin.data(), totalSamples, sampleRate);
    printf("INFO  wrote wave_table_sin.wav (sin in 440 Hz)\n");
    writeWav("wave_table_saw.wav", out_saw.data(), totalSamples, sampleRate);
    printf("INFO  wrote wave_table_saw.wav (in 440 Hz)\n");
    writeWav("wave_table_square.wav", out_square.data(), totalSamples, sampleRate);
    printf("INFO  wrote wave_table_square.wav (in 440 Hz)\n");
}


// --- NoiseDrum helpers ---

static void renderDrum(NoiseDrum& r, int note, int velocity,
                       float* out, int samples) {
    r.noteOn(0, note, velocity);
    for (int i = 0; i < samples; i += 256) {
        int n = std::min(256, samples - i);
        r.render(out + i, n);
    }
}

// --- NoiseDrum tests ---

static void test_drum_silence_before_noteOn() {
    NoiseDrum r;
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(allZero(buf, 256), "drum: silence before any noteOn");
}

static void test_drum_kick_sound() {
    NoiseDrum r;
    r.noteOn(0, 36, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.05f, "drum: kick (note 36) produces sound");
}

static void test_drum_snare_sound() {
    NoiseDrum r;
    r.noteOn(0, 38, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.05f, "drum: snare (note 38) produces sound");
}

static void test_drum_closed_hat_sound() {
    NoiseDrum r;
    r.noteOn(0, 42, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.01f, "drum: closed hat (note 42) produces sound");
}

static void test_drum_open_hat_sound() {
    NoiseDrum r;
    r.noteOn(0, 46, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.05f, "drum: open hat (note 46) produces sound");
}

static void test_drum_crash_sound() {
    NoiseDrum r;
    r.noteOn(0, 49, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.05f, "drum: crash (note 49) produces sound");
}

static void test_drum_ride_sound() {
    NoiseDrum r;
    r.noteOn(0, 51, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.05f, "drum: ride (note 51) produces sound");
}

static void test_drum_kick_decays() {
    // Kick has 150 ms exponential decay — voice reaches the 1e-4 isDone threshold
    // at ~1.3 s. Drain 1500 ms to confirm the voice has gone fully silent.
    NoiseDrum r;
    r.noteOn(0, 36, 127);
    const int drain = kSampleRate * 1500 / 1000;
    std::vector<float> tmp(drain, 0.0f);
    for (int i = 0; i < drain; i += 256) {
        int n = std::min(256, drain - i);
        r.render(tmp.data() + i, n);
    }
    float tail[256] = {};
    r.render(tail, 256);
    CHECK(maxAmp(tail, 256) < 1e-3f, "drum: kick is silent after 1500 ms");
}

// --- FmDrum tests ---

static void test_fm_drum_silence_before_noteOn() {
    FmDrum r;
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(allZero(buf, 256), "fm drum: silence before any noteOn");
}

static void test_fm_drum_kick_sound() {
    FmDrum r;
    r.noteOn(0, 36, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.05f, "fm drum: kick (note 36) produces sound");
}

static void test_fm_drum_snare_sound() {
    FmDrum r;
    r.noteOn(0, 38, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.05f, "fm drum: snare (note 38) produces sound");
}

static void test_fm_drum_closed_hat_sound() {
    FmDrum r;
    r.noteOn(0, 42, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.05f, "fm drum: closed hat (note 42) produces sound");
}

static void test_fm_drum_open_hat_sound() {
    FmDrum r;
    r.noteOn(0, 46, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.05f, "fm drum: open hat (note 46) produces sound");
}

static void test_fm_drum_crash_sound() {
    FmDrum r;
    r.noteOn(0, 49, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.05f, "fm drum: crash (note 49) produces sound");
}

static void test_fm_drum_ride_sound() {
    FmDrum r;
    r.noteOn(0, 51, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.05f, "fm drum: ride (note 51) produces sound");
}

static void test_fm_drum_tom_sound() {
    FmDrum r;
    r.noteOn(0, 47, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.05f, "fm drum: tom (note 47) produces sound");
}

static void test_fm_drum_unknown_note_silence() {
    FmDrum r;
    r.noteOn(0, 99, 100); // unmapped note
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(allZero(buf, 256), "fm drum: unknown note produces no sound");
}

static void test_fm_drum_kick_decays() {
    // Kick has 180 ms env decay; voice reaches isDone threshold at ~1.6 s.
    FmDrum r;
    r.noteOn(0, 36, 127);
    const int drain = kSampleRate * 1800 / 1000;
    std::vector<float> tmp(drain, 0.0f);
    for (int i = 0; i < drain; i += 256) {
        int n = std::min(256, drain - i);
        r.render(tmp.data() + i, n);
    }
    float tail[256] = {};
    r.render(tail, 256);
    CHECK(maxAmp(tail, 256) < 1e-3f, "fm drum: kick is silent after 1800 ms");
}

// --- WAV output ---

static void write_wav_noise_drums() {
    NoiseDrum r;

    // Each drum hit at 0.5s intervals; total 4 seconds
    const int sampleRate  = kSampleRate;
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

// --- Piano tests ---

static void test_piano_silence_before_noteOn() {
    Piano r;
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(allZero(buf, 256), "piano: silence before any noteOn");
}

static void test_piano_sound_after_noteOn() {
    Piano r;
    r.noteOn(0, 60, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.01f, "piano: sound during attack");
}

static void test_piano_sustain_audible() {
    // Drain attack (220) + decay (3528) samples, then check sustain is still audible
    Piano r;
    r.noteOn(0, 60, 127);
    const int drain = Piano::kAttackSamples + Piano::kDecaySamples + 256;
    std::vector<float> tmp(drain, 0.0f);
    for (int i = 0; i < drain; i += 256) {
        int n = std::min(256, drain - i);
        r.render(tmp.data() + i, n);
    }
    float check[256] = {};
    r.render(check, 256);
    CHECK(maxAmp(check, 256) > 0.1f, "piano: sustain is audible after attack+decay");
}

static void test_piano_silent_after_release() {
    // Release coeff 300 ms; from sustain gain ~0.42, voice goes silent at ~2.5 s
    Piano r;
    r.noteOn(0, 60, 127);
    // Reach sustain
    const int toSustain = Piano::kAttackSamples + Piano::kDecaySamples + 256;
    std::vector<float> tmp(toSustain, 0.0f);
    for (int i = 0; i < toSustain; i += 256) {
        int n = std::min(256, toSustain - i);
        r.render(tmp.data() + i, n);
    }
    // Trigger release and drain 3 seconds
    r.noteOff(0, 60);
    const int drain = kSampleRate * 3;
    std::vector<float> rel(drain, 0.0f);
    for (int i = 0; i < drain; i += 256) {
        int n = std::min(256, drain - i);
        r.render(rel.data() + i, n);
    }
    float tail[256] = {};
    r.render(tail, 256);
    CHECK(maxAmp(tail, 256) < 1e-3f, "piano: silent after noteOff + 3 s release");
}

static void test_piano_voice_stealing() {
    Piano r;
    // Fill all 8 slots (notes 60–67)
    for (int n = 60; n < 68; ++n) r.noteOn(0, n, 100);
    float buf[256] = {};
    r.render(buf, 256);
    CHECK(maxAmp(buf, 256) > 0.1f, "piano: 8 simultaneous voices audible");

    // 9th note must steal oldest (note 60) and still produce sound
    r.noteOn(0, 68, 100);
    float buf2[256] = {};
    r.render(buf2, 256);
    CHECK(maxAmp(buf2, 256) > 0.1f, "piano: 9th note audible after voice steal");
}

// --- WAV output ---

static void write_wav_piano_chord() {
    Piano r;
    const int sampleRate   = kSampleRate;
    const int totalSamples = sampleRate * 2; // 2 seconds
    std::vector<float> out(totalSamples, 0.0f);

    // C major chord: C4=60, E4=64, G4=67
    r.noteOn(0, 60, 100);
    r.noteOn(0, 64, 100);
    r.noteOn(0, 67, 100);

    const int releaseAt = sampleRate; // noteOff at 1 s

    for (int i = 0; i < totalSamples; i += 256) {
        if (i >= releaseAt && i < releaseAt + 256) {
            r.noteOff(0, 60);
            r.noteOff(0, 64);
            r.noteOff(0, 67);
        }
        int n = std::min(256, totalSamples - i);
        r.render(out.data() + i, n);
    }
    writeWav("piano_chord.wav", out.data(), totalSamples, sampleRate);
    printf("INFO  wrote piano_chord.wav  (C major chord, release at 1 s)\n");
}

static void write_wav_fm_drums() {
    FmDrum r;
    const int sampleRate   = kSampleRate;
    const int totalSamples = sampleRate * 4;
    const int hitInterval  = sampleRate / 2;
    std::vector<float> out(totalSamples, 0.0f);

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

    writeWav("fm_drums.wav", out.data(), totalSamples, sampleRate);
    printf("INFO  wrote fm_drums.wav     (kick snare hat openhat crash ride tom)\n");
}

static void write_wav_piano_sin_table() {

    PianoSinTable r;
    const int sampleRate   = kSampleRate;
    const int totalSamples = sampleRate * 2; // 2 seconds
    std::vector<float> out(totalSamples, 0.0f);


    r.initSinTable();
    // C major chord: C4=60, E4=64, G4=67
    r.noteOn(0, 60, 100);
    r.noteOn(0, 64, 100);
    r.noteOn(0, 67, 100);
    r.noteOn(0, 68, 100);

    const int releaseAt = sampleRate; // noteOff at 1 s

    for (int i = 0; i < totalSamples; i += 256) {
        if (i >= releaseAt && i < releaseAt + 256) {
            r.noteOff(0, 60);
            r.noteOff(0, 64);
            r.noteOff(0, 67);
        }
        int n = std::min(256, totalSamples - i);
        r.render(out.data() + i, n);
    }
    writeWav("piano_chord_2.wav", out.data(), totalSamples, sampleRate);
    printf("INFO  wrote piano_chord_2.wav  (C major chord, release at 1 s)\n");
}

int main() {
    printf("=== WaveTable ===\n");
    write_wav_wave_table();

    printf("\n=== NoiseDrum ===\n");
    test_drum_silence_before_noteOn();
    test_drum_kick_sound();
    test_drum_snare_sound();
    test_drum_closed_hat_sound();
    test_drum_open_hat_sound();
    test_drum_crash_sound();
    test_drum_ride_sound();
    test_drum_kick_decays();

    printf("\n=== FmDrum ===\n");
    test_fm_drum_silence_before_noteOn();
    test_fm_drum_kick_sound();
    test_fm_drum_snare_sound();
    test_fm_drum_closed_hat_sound();
    test_fm_drum_open_hat_sound();
    test_fm_drum_crash_sound();
    test_fm_drum_ride_sound();
    test_fm_drum_tom_sound();
    test_fm_drum_unknown_note_silence();
    test_fm_drum_kick_decays();

    printf("\n=== Piano ===\n");
    test_piano_silence_before_noteOn();
    test_piano_sound_after_noteOn();
    test_piano_sustain_audible();
    test_piano_silent_after_release();
    test_piano_voice_stealing();

    printf("\n=== WAV output ===\n");
    write_wav_noise_drums();
    write_wav_fm_drums();
    write_wav_piano_chord();
    write_wav_piano_sin_table();

    printf("\n%d passed, %d failed\n", gPass, gFail);
    return gFail > 0 ? 1 : 0;
}
