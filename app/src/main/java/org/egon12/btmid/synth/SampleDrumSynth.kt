package org.egon12.btmid.synth

import java.util.concurrent.ConcurrentLinkedQueue

private data class SampleNoteOnEvent(val note: Int, val velocity: Int)

class SampleDrumSynth(private val bank: SampleBank) : DrumSynth {

    private val pendingOn = ConcurrentLinkedQueue<SampleNoteOnEvent>()

    // voices is only accessed from the render thread — no lock needed
    private val voices = mutableListOf<SampleVoice>()

    override fun noteOn(note: Int, velocity: Int) {
        pendingOn.offer(SampleNoteOnEvent(note, velocity))
    }

    override fun noteOff(note: Int) {
        // one-shot; noteOff ignored
    }

    override fun render(buffer: FloatArray) {
        while (true) {
            val event = pendingOn.poll() ?: break
            val pcm = bank[sampleName(event.note)] ?: continue
            voices.add(SampleVoice(pcm, event.velocity / 127f * 0.7f))
        }

        val iter = voices.iterator()
        while (iter.hasNext()) {
            val voice = iter.next()
            val pcm = voice.pcm
            val gain = voice.gain
            var pos = voice.pos
            val count = minOf(buffer.size, pcm.size - pos)
            for (i in 0 until count) {
                buffer[i] += gain * pcm[pos++]
            }
            voice.pos = pos
            if (voice.isDone) iter.remove()
        }
    }

    private fun sampleName(note: Int): String = when (note) {
        35, 36       -> "kick"
        38, 40       -> "snare"
        42           -> "closed_hat"
        46           -> "open_hat"
        49, 57       -> "crash"
        51           -> "ride"
        41, 43,
        45, 47,
        48, 50       -> "tom"
        else         -> ""
    }
}

private class SampleVoice(val pcm: FloatArray, val gain: Float) {
    var pos = 0
    val isDone get() = pos >= pcm.size
}
