#include "SfInstrument.h"
#include "../AudioConfig.h"

#define TSF_IMPLEMENTATION
#include "tsf.h"

// TSF renders on a single (audio) channel here; we route everything to TSF
// channel 0, bank 0, preset 0.
static constexpr int kTsfChannel = 0;

SfInstrument::~SfInstrument() {
    if (tsf* f = mSf.load(std::memory_order_acquire)) {
        tsf_close(f);
    }
}

void SfInstrument::loadSoundFont(const uint8_t* data, int len) {
    if (!data || len <= 0) return;

    tsf* f = tsf_load_memory(data, len);
    if (!f) return;

    tsf_set_output(f, TSF_MONO, kSampleRate, 0.0f);
    tsf_channel_set_bank_preset(f, kTsfChannel, 0, 0);

    // Load-once: publish for the audio thread. We never replace an in-use
    // pointer, so no deferred free is needed.
    mSf.store(f, std::memory_order_release);
}

void SfInstrument::noteOn(int, int note, int velocity) {
    mQueue.push({NoteOn, (uint8_t)note, (uint8_t)velocity});
}

void SfInstrument::noteOff(int, int note) {
    mQueue.push({NoteOff, (uint8_t)note, 0});
}

void SfInstrument::controlChange(int, int cc, int value) {
    mQueue.push({ControlChange, (uint8_t)cc, (uint8_t)value});
}

void SfInstrument::render(float* buffer, int32_t frames) {
    // Acquire-load: pairs with release-store in loadSoundFont.
    tsf* f = mSf.load(std::memory_order_acquire);

    Event e;
    while (mQueue.pop(e)) {
        if (!f) continue; // drain queue even if not loaded yet
        switch (e.type) {
            case NoteOn:
                tsf_channel_note_on(f, kTsfChannel, e.a, e.b / 127.0f);
                break;
            case NoteOff:
                tsf_channel_note_off(f, kTsfChannel, e.a);
                break;
            case ControlChange:
                // CC#64 sustain handled natively; unsupported CCs are no-ops.
                tsf_channel_midi_control(f, kTsfChannel, e.a, e.b);
                break;
        }
    }

    if (!f) return;

    // flag_mixing=1 adds into the shared mono mix buffer (matches renderAudio).
    tsf_render_float(f, buffer, frames, /*flag_mixing=*/1);
}
