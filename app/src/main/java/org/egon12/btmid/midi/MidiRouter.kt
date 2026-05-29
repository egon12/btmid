package org.egon12.btmid.midi

import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.asSharedFlow
import org.egon12.btmid.synth.DrumSynth
import org.egon12.btmid.synth.PianoSynth

class MidiRouter(
    private val pianoSynth: PianoSynth,
    private val drumSynth: DrumSynth,
) {
    private val _events = MutableSharedFlow<MidiEvent>(extraBufferCapacity = 64)
    val events: SharedFlow<MidiEvent> = _events.asSharedFlow()

    fun route(event: MidiEvent) {
        _events.tryEmit(event)
        when (event) {
            is MidiEvent.NoteOn -> when (event.channel) {
                0 -> pianoSynth.noteOn(event.note, event.velocity)
                9 -> drumSynth.noteOn(event.note, event.velocity)
            }
            is MidiEvent.NoteOff -> when (event.channel) {
                0 -> pianoSynth.noteOff(event.note)
                9 -> drumSynth.noteOff(event.note)
            }
            is MidiEvent.ControlChange -> Unit
        }
    }
}
