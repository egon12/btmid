#pragma once
#include <cstddef>
#include <cstdint>

enum class MidiMsgType : uint8_t {
    NoteOff = 0x80,
    NoteOn  = 0x90,
    CC      = 0xB0,
};

struct MidiMsg {
    MidiMsgType type;
    uint8_t     channel;
    uint8_t     data1;   // note / controller#
    uint8_t     data2;   // velocity / CC value
};

// Parse raw MIDI 1.0 bytes (with running-status) into 'out'.
// runningStatus is read and updated across calls for the same port.
// Returns number of events written. 'out' must have capacity >= maxOut.
int parseMidi(const uint8_t* data, size_t length, MidiMsg* out, int maxOut, uint8_t& runningStatus);
