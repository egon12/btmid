package org.gilbertxenodike.btmid.midi

import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.asSharedFlow
import org.gilbertxenodike.btmid.synth.NativeAudioEngine

class MidiRouter : NativeAudioEngine.MidiEventListener {
    private val _events = MutableSharedFlow<MidiEvent>(extraBufferCapacity = 64)
    val events: SharedFlow<MidiEvent> = _events.asSharedFlow()

    private val _loopState = MutableSharedFlow<Int>(extraBufferCapacity = 8)
    val loopStateEvents: SharedFlow<Int> = _loopState.asSharedFlow()

    // Called from the C++ dispatch thread after AMidi parses each event
    override fun onMidiEvent(channel: Int, type: Int, data1: Int, data2: Int) {
        val event: MidiEvent = when (type) {
            0x80 -> MidiEvent.NoteOff(channel, data1, data2)
            0x90 -> MidiEvent.NoteOn(channel, data1, data2)
            0xB0 -> MidiEvent.ControlChange(channel, data1, data2)
            else -> return
        }
        _events.tryEmit(event)
    }

    override fun onLoopState(state: Int) {
        _loopState.tryEmit(state)
    }
}
