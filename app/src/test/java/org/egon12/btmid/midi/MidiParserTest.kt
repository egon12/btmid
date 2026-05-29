package org.egon12.btmid.midi

import org.junit.Assert.assertEquals
import org.junit.Test

class MidiParserTest {

    @Test
    fun `parses NoteOn`() {
        val bytes = byteArrayOf(0x90.b(), 60, 100)
        assertEquals(
            listOf(MidiEvent.NoteOn(channel = 0, note = 60, velocity = 100)),
            MidiParser.parse(bytes, 0, bytes.size)
        )
    }

    @Test
    fun `parses NoteOff`() {
        val bytes = byteArrayOf(0x80.b(), 60, 64)
        assertEquals(
            listOf(MidiEvent.NoteOff(channel = 0, note = 60, velocity = 64)),
            MidiParser.parse(bytes, 0, bytes.size)
        )
    }

    @Test
    fun `NoteOn with velocity 0 becomes NoteOff`() {
        val bytes = byteArrayOf(0x90.b(), 60, 0)
        assertEquals(
            listOf(MidiEvent.NoteOff(channel = 0, note = 60, velocity = 0)),
            MidiParser.parse(bytes, 0, bytes.size)
        )
    }

    @Test
    fun `parses ControlChange`() {
        val bytes = byteArrayOf(0xB0.b(), 64, 127)
        assertEquals(
            listOf(MidiEvent.ControlChange(channel = 0, controller = 64, value = 127)),
            MidiParser.parse(bytes, 0, bytes.size)
        )
    }

    @Test
    fun `parses channel from status byte`() {
        val bytes = byteArrayOf(0x99.b(), 36, 80)
        assertEquals(
            listOf(MidiEvent.NoteOn(channel = 9, note = 36, velocity = 80)),
            MidiParser.parse(bytes, 0, bytes.size)
        )
    }

    @Test
    fun `handles running status`() {
        val bytes = byteArrayOf(0x90.b(), 60, 100, 62, 90)
        assertEquals(
            listOf(
                MidiEvent.NoteOn(channel = 0, note = 60, velocity = 100),
                MidiEvent.NoteOn(channel = 0, note = 62, velocity = 90),
            ),
            MidiParser.parse(bytes, 0, bytes.size)
        )
    }

    @Test
    fun `running status with velocity 0 becomes NoteOff`() {
        val bytes = byteArrayOf(0x90.b(), 60, 100, 60, 0)
        assertEquals(
            listOf(
                MidiEvent.NoteOn(channel = 0, note = 60, velocity = 100),
                MidiEvent.NoteOff(channel = 0, note = 60, velocity = 0),
            ),
            MidiParser.parse(bytes, 0, bytes.size)
        )
    }

    @Test
    fun `parses multiple events`() {
        val bytes = byteArrayOf(0x90.b(), 60, 100, 0x80.b(), 60, 0)
        assertEquals(
            listOf(
                MidiEvent.NoteOn(channel = 0, note = 60, velocity = 100),
                MidiEvent.NoteOff(channel = 0, note = 60, velocity = 0),
            ),
            MidiParser.parse(bytes, 0, bytes.size)
        )
    }

    @Test
    fun `respects offset and count`() {
        val bytes = byteArrayOf(0xFF.b(), 0x90.b(), 60, 100, 0xFF.b())
        assertEquals(
            listOf(MidiEvent.NoteOn(channel = 0, note = 60, velocity = 100)),
            MidiParser.parse(bytes, 1, 3)
        )
    }

    @Test
    fun `skips program change and continues`() {
        val bytes = byteArrayOf(0xC0.b(), 10, 0x90.b(), 60, 100)
        assertEquals(
            listOf(MidiEvent.NoteOn(channel = 0, note = 60, velocity = 100)),
            MidiParser.parse(bytes, 0, bytes.size)
        )
    }

    @Test
    fun `returns empty list for empty input`() {
        assertEquals(emptyList<MidiEvent>(), MidiParser.parse(byteArrayOf(), 0, 0))
    }

    private fun Int.b() = toByte()
}
