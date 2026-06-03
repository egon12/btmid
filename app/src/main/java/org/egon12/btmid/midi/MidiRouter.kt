package org.egon12.btmid.midi

import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.asSharedFlow
import org.egon12.btmid.synth.NativeAudioEngine

class MidiRouter {
    private val _events = MutableSharedFlow<MidiEvent>(extraBufferCapacity = 64)
    val events: SharedFlow<MidiEvent> = _events.asSharedFlow()

    fun route(event: MidiEvent) {
        _events.tryEmit(event)
        when (event) {
            is MidiEvent.NoteOn  -> NativeAudioEngine.noteOn(event.channel, event.note, event.velocity)
            is MidiEvent.NoteOff -> NativeAudioEngine.noteOff(event.channel, event.note)
            is MidiEvent.ControlChange -> Unit
        }
    }
}
