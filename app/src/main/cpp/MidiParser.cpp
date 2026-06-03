#include "MidiParser.h"

int parseMidi(const uint8_t* data, size_t length, MidiMsg* out, int maxOut, uint8_t& runningStatus) {
    int    count = 0;
    size_t i     = 0;

    while (i < length && count < maxOut) {
        uint8_t b = data[i];
        uint8_t status;
        size_t  d;  // index of first data byte

        if (b & 0x80) {
            status        = b;
            runningStatus = b;
            d             = i + 1;
        } else {
            status = runningStatus;
            d      = i;
        }

        uint8_t type    = status & 0xF0;
        uint8_t channel = status & 0x0F;

        bool stop = false;
        switch (type) {
            case 0x80:
                if (d + 2 > length) { stop = true; break; }
                out[count++] = { MidiMsgType::NoteOff, channel, data[d], data[d + 1] };
                i = d + 2;
                break;
            case 0x90: {
                if (d + 2 > length) { stop = true; break; }
                uint8_t vel = data[d + 1];
                out[count++] = { vel == 0 ? MidiMsgType::NoteOff : MidiMsgType::NoteOn,
                                 channel, data[d], vel };
                i = d + 2;
                break;
            }
            case 0xB0:
                if (d + 2 > length) { stop = true; break; }
                out[count++] = { MidiMsgType::CC, channel, data[d], data[d + 1] };
                i = d + 2;
                break;
            case 0xA0: case 0xE0: i = d + 2; break;
            case 0xC0: case 0xD0: i = d + 1; break;
            default:               i = d + 1; break;
        }
        if (stop) break;
    }
    return count;
}
