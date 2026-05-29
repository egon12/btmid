package org.egon12.btmid.synth

import java.util.concurrent.ConcurrentLinkedQueue
import kotlin.math.PI
import kotlin.math.exp
import kotlin.math.sin
import kotlin.random.Random

private const val SAMPLE_RATE = 44100
private const val TWO_PI = 2.0 * PI

private data class DrumNoteOnEvent(val note: Int, val velocity: Int)

class DrumSynth {
    private val pendingOn = ConcurrentLinkedQueue<DrumNoteOnEvent>()

    // voices is only accessed from the render thread — no lock needed
    private val voices = mutableListOf<DrumVoice>()

    fun noteOn(note: Int, velocity: Int) {
        pendingOn.offer(DrumNoteOnEvent(note, velocity))
    }

    @Suppress("UNUSED_PARAMETER")
    fun noteOff(note: Int) {
        // Drum voices are one-shot; noteOff is ignored
    }

    fun render(buffer: FloatArray) {
        while (true) {
            val event = pendingOn.poll() ?: break
            voices.add(createVoice(event.note, event.velocity / 127f * 0.7f))
        }

        val done = mutableListOf<DrumVoice>()
        for (voice in voices) {
            for (i in buffer.indices) {
                buffer[i] += voice.nextSample()
            }
            if (voice.isDone) done.add(voice)
        }
        voices.removeAll(done)
    }

    private fun createVoice(note: Int, gain: Float): DrumVoice = when (note) {
        35, 36 -> BassDrumVoice(gain)
        38, 40 -> SnareVoice(gain)
        42     -> ClosedHatVoice(gain)
        46     -> NoiseVoice(gain, decayMs = 300.0)
        49, 57 -> NoiseVoice(gain, decayMs = 800.0)
        51     -> RideVoice(gain)
        else   -> NoiseVoice(gain, decayMs = 100.0)
    }
}

private abstract class DrumVoice {
    abstract var gain: Float
    abstract val decayCoeff: Float
    abstract fun nextSample(): Float
    val isDone get() = gain < 1e-4f
}

private class BassDrumVoice(initialGain: Float) : DrumVoice() {
    override var gain = initialGain
    override val decayCoeff = exp(-1.0 / (0.150 * SAMPLE_RATE)).toFloat()
    private var phase = 0.0
    private val phaseInc = TWO_PI * 60.0 / SAMPLE_RATE

    override fun nextSample(): Float {
        val s = gain * sin(phase).toFloat()
        phase += phaseInc
        gain *= decayCoeff
        return s
    }
}

private class SnareVoice(initialGain: Float) : DrumVoice() {
    override var gain = initialGain
    override val decayCoeff = exp(-1.0 / (0.100 * SAMPLE_RATE)).toFloat()
    private var tonePhase = 0.0
    private val tonePhaseInc = TWO_PI * 200.0 / SAMPLE_RATE

    override fun nextSample(): Float {
        val noise = Random.nextFloat() * 2f - 1f
        val tone = sin(tonePhase).toFloat()
        val s = gain * (0.7f * noise + 0.3f * tone)
        tonePhase += tonePhaseInc
        gain *= decayCoeff
        return s
    }
}

private class ClosedHatVoice(initialGain: Float) : DrumVoice() {
    override var gain = initialGain
    override val decayCoeff = exp(-1.0 / (0.050 * SAMPLE_RATE)).toFloat()
    private var prevNoise = 0f

    override fun nextSample(): Float {
        val x = Random.nextFloat() * 2f - 1f
        val y = x - prevNoise
        prevNoise = x
        val s = gain * y
        gain *= decayCoeff
        return s
    }
}

private class NoiseVoice(initialGain: Float, decayMs: Double) : DrumVoice() {
    override var gain = initialGain
    override val decayCoeff = exp(-1.0 / (decayMs / 1000.0 * SAMPLE_RATE)).toFloat()

    override fun nextSample(): Float {
        val s = gain * (Random.nextFloat() * 2f - 1f)
        gain *= decayCoeff
        return s
    }
}

private class RideVoice(initialGain: Float) : DrumVoice() {
    override var gain = initialGain
    override val decayCoeff = exp(-1.0 / (0.400 * SAMPLE_RATE)).toFloat()
    private var tonePhase = 0.0
    private val tonePhaseInc = TWO_PI * 600.0 / SAMPLE_RATE

    override fun nextSample(): Float {
        val noise = Random.nextFloat() * 2f - 1f
        val tone = sin(tonePhase).toFloat()
        val s = gain * (0.6f * noise + 0.4f * tone)
        tonePhase += tonePhaseInc
        gain *= decayCoeff
        return s
    }
}
