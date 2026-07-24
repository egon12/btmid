#pragma once
#include "../Instrument.h"
#include "../SpscRing.h"
#include <atomic>
#include <cstdint>

// Forward declaration so callers don't need tsf.h (single translation unit
// defines TSF_IMPLEMENTATION in SfInstrument.cpp).
struct tsf;

// SoundFont (SF2) instrument backed by TinySoundFont.
//   - loadSoundFont() parses SF2 bytes off the UI/IO thread; the resulting tsf*
//     is published via an atomic (load-once, no live reload).
//   - noteOn/noteOff/controlChange are queued (SpscRing) and drained on the
//     audio thread inside render(), because TSF is not thread-safe.
class SfInstrument : public Instrument {
public:
    SfInstrument() = default;
    ~SfInstrument() override;

    void noteOn(int channel, int note, int velocity) override;
    void noteOff(int channel, int note) override;
    void controlChange(int channel, int cc, int value) override;
    void render(float* buffer, int32_t frames) override;

    // Called from the JNI/Kotlin thread after reading the .sf2 asset bytes.
    void loadSoundFont(const uint8_t* data, int len);

private:
    static constexpr int kQueueCap = 64; // must be power-of-2

    enum EventType : uint8_t { NoteOn, NoteOff, ControlChange };
    struct Event {
        uint8_t type;
        uint8_t a; // note or cc
        uint8_t b; // velocity or cc value
    };

    SpscRing<Event, kQueueCap> mQueue;

    // Published by loadSoundFont (release), read by render (acquire).
    std::atomic<tsf*> mSf{nullptr};
};
