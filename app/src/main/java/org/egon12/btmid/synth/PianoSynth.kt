package org.egon12.btmid.synth

import java.util.concurrent.ConcurrentLinkedQueue
import kotlin.math.PI
import kotlin.math.exp
import kotlin.math.pow
import kotlin.math.sin

private const val SAMPLE_RATE = 44100
private const val MAX_VOICES = 8
private const val TWO_PI = 2.0 * PI

private val HARMONIC_AMPLITUDES = floatArrayOf(1.00f, 0.50f, 0.25f, 0.12f, 0.06f)

private const val ATTACK_SAMPLES  = (0.005 * SAMPLE_RATE).toInt()   // 5ms
private const val DECAY_SAMPLES   = (0.080 * SAMPLE_RATE).toInt()   // 80ms
private const val SUSTAIN_LEVEL   = 0.60f
private val RELEASE_COEFF = exp(-1.0 / (0.300 * SAMPLE_RATE)).toFloat()  // 300ms

private data class NoteOnEvent(val note: Int, val velocity: Int)

class PianoSynth {
    private val pendingOn  = ConcurrentLinkedQueue<NoteOnEvent>()
    private val pendingOff = ConcurrentLinkedQueue<Int>()

    // voices is only accessed from the render thread — no lock needed
    private val voices = mutableListOf<PianoVoice>()

    fun noteOn(note: Int, velocity: Int) {
        pendingOn.offer(NoteOnEvent(note, velocity))
    }

    fun noteOff(note: Int) {
        pendingOff.offer(note)
    }

    fun render(buffer: FloatArray) {
        drainPending()

        val done = mutableListOf<PianoVoice>()
        for (voice in voices) {
            voice.render(buffer)
            if (voice.voicePhase == VoicePhase.Done) done.add(voice)
        }
        voices.removeAll(done)
    }

    private fun drainPending() {
        while (true) {
            val event = pendingOn.poll() ?: break
            addVoice(event.note, event.velocity)
        }
        while (true) {
            val note = pendingOff.poll() ?: break
            releaseVoice(note)
        }
    }

    private fun addVoice(note: Int, velocity: Int) {
        val peak = velocity / 127f * 0.7f
        val freq = 440.0 * 2.0.pow((note - 69) / 12.0)
        voices.removeAll { it.note == note }
        if (voices.size >= MAX_VOICES) voices.removeAt(0)
        voices.add(PianoVoice(note, freq, peak))
    }

    private fun releaseVoice(note: Int) {
        voices.filter { it.note == note && it.voicePhase != VoicePhase.Release }
              .forEach { it.voicePhase = VoicePhase.Release }
    }
}

private enum class VoicePhase { Attack, Decay, Sustain, Release, Done }

private class PianoVoice(
    val note: Int,
    freq: Double,
    val peak: Float,
) {
    val phases = DoubleArray(5)
    val phaseIncrements = DoubleArray(5) { h -> TWO_PI * freq * (h + 1) / SAMPLE_RATE }
    var gain = 0f
    var voicePhase = VoicePhase.Attack
    var envelopeSamples = 0

    fun render(buffer: FloatArray) {
        for (i in buffer.indices) {
            when (voicePhase) {
                VoicePhase.Attack -> {
                    gain = (envelopeSamples.toFloat() / ATTACK_SAMPLES) * peak
                    envelopeSamples++
                    if (envelopeSamples >= ATTACK_SAMPLES) {
                        gain = peak
                        voicePhase = VoicePhase.Decay
                        envelopeSamples = 0
                    }
                }
                VoicePhase.Decay -> {
                    val t = envelopeSamples.toFloat() / DECAY_SAMPLES
                    gain = peak * (1f - t * (1f - SUSTAIN_LEVEL))
                    envelopeSamples++
                    if (envelopeSamples >= DECAY_SAMPLES) {
                        gain = peak * SUSTAIN_LEVEL
                        voicePhase = VoicePhase.Sustain
                    }
                }
                VoicePhase.Sustain -> Unit
                VoicePhase.Release -> {
                    gain *= RELEASE_COEFF
                    if (gain < 1e-4f) {
                        voicePhase = VoicePhase.Done
                        break
                    }
                }
                VoicePhase.Done -> break
            }

            var sample = 0f
            for (h in 0 until 5) {
                sample += (HARMONIC_AMPLITUDES[h] * sin(phases[h])).toFloat()
                phases[h] += phaseIncrements[h]
            }
            buffer[i] += gain * sample
        }
    }
}
