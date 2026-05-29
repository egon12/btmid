package org.egon12.btmid.midi

import android.media.midi.MidiReceiver
import android.util.Log

private const val TAG = "AppMidiReceiver"

class AppMidiReceiver(private val router: MidiRouter) : MidiReceiver() {
    override fun onSend(msg: ByteArray, offset: Int, count: Int, timestamp: Long) {
        val hex = msg.slice(offset until offset + count)
            .joinToString(" ") { "%02X".format(it) }
        Log.d(TAG, "MIDI bytes: $hex")

        MidiParser.parse(msg, offset, count).forEach { event ->
            Log.d(TAG, "MIDI event: $event")
            router.route(event)
        }
    }
}
