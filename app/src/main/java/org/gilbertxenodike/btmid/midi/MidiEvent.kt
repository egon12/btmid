package org.gilbertxenodike.btmid.midi

sealed class MidiEvent {
    data class NoteOn(val channel: Int, val note: Int, val velocity: Int) : MidiEvent()
    data class NoteOff(val channel: Int, val note: Int, val velocity: Int) : MidiEvent()
    data class ControlChange(val channel: Int, val controller: Int, val value: Int) : MidiEvent()
}
