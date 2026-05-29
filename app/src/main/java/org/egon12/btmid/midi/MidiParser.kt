package org.egon12.btmid.midi

object MidiParser {

    fun parse(msg: ByteArray, offset: Int, count: Int): List<MidiEvent> {
        val events = mutableListOf<MidiEvent>()
        var i = offset
        val end = offset + count
        var runningStatus = 0

        while (i < end) {
            val b = msg[i].u()
            val status: Int
            val d: Int  // index of first data byte

            if (b and 0x80 != 0) {
                status = b
                runningStatus = b
                d = i + 1
            } else {
                status = runningStatus
                d = i
            }

            val type = status and 0xF0
            val ch = status and 0x0F

            when (type) {
                0x80 -> {
                    if (d + 1 >= end) break
                    events += MidiEvent.NoteOff(ch, msg[d].u(), msg[d + 1].u())
                    i = d + 2
                }
                0x90 -> {
                    if (d + 1 >= end) break
                    val note = msg[d].u()
                    val vel = msg[d + 1].u()
                    events += if (vel == 0) MidiEvent.NoteOff(ch, note, 0)
                              else MidiEvent.NoteOn(ch, note, vel)
                    i = d + 2
                }
                0xB0 -> {
                    if (d + 1 >= end) break
                    events += MidiEvent.ControlChange(ch, msg[d].u(), msg[d + 1].u())
                    i = d + 2
                }
                0xA0, 0xE0 -> i = d + 2  // skip other 2-data-byte messages
                0xC0, 0xD0 -> i = d + 1  // skip 1-data-byte messages
                else        -> i = d + 1  // system/unknown — advance one byte
            }
        }
        return events
    }

    private fun Byte.u() = toInt() and 0xFF
}
