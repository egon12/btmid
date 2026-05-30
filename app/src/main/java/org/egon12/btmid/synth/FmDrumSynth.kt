package org.egon12.btmid.synth

import java.util.concurrent.ConcurrentLinkedQueue
import kotlin.math.PI
import kotlin.math.exp
import kotlin.math.sin
import kotlin.random.Random

private const val SAMPLE_RATE = 44100
private const val TWO_PI = 2.0 * PI

private data class FmNoteOnEvent(val note: Int, val velocity: Int)

class FmDrumSynth : DrumSynth {
    private val pendingOn = ConcurrentLinkedQueue<FmNoteOnEvent>()

    // voices is only accessed from the render thread — no lock needed
    private val voices = mutableListOf<FmDrumVoice>()

    override fun noteOn(note: Int, velocity: Int) {
        pendingOn.offer(FmNoteOnEvent(note, velocity))
    }

    override fun noteOff(note: Int) {}

    override fun render(buffer: FloatArray) {
        while (true) {
            val event = pendingOn.poll() ?: break
            createVoice(event.note, event.velocity / 127f * 0.7f)?.let { voices.add(it) }
        }

        val done = mutableListOf<FmDrumVoice>()
        for (voice in voices) {
            for (i in buffer.indices) {
                buffer[i] += voice.nextSample()
            }
            if (voice.isDone) done.add(voice)
        }
        voices.removeAll(done)
    }

    private fun createVoice(note: Int, gain: Float): FmDrumVoice? = when (note) {
        35, 36          -> FmKickVoice(gain)
        38, 40          -> FmSnareVoice(gain)
        42              -> FmHatVoice(gain, decayMs = 45.0)
        46              -> FmHatVoice(gain, decayMs = 350.0)
        49, 57          -> FmCrashVoice(gain)
        51              -> FmRideVoice(gain)
        41, 43, 45,
        47, 48, 50      -> FmTomVoice(gain, tomStartFreq(note))
        else            -> null
    }

    private fun tomStartFreq(note: Int) = when (note) {
        41 -> 60.0   // Low Floor Tom
        43 -> 75.0   // High Floor Tom
        45 -> 90.0   // Low Tom
        47 -> 110.0  // Low-Mid Tom
        48 -> 130.0  // Hi-Mid Tom
        else -> 155.0 // High Tom (50)
    }
}

private abstract class FmDrumVoice {
    abstract var gain: Float
    abstract fun nextSample(): Float
    val isDone get() = gain < 1e-4f
}

// Bass drum: self-FM sine with frequency sweep 50→30 Hz and modulation index sweep 4→0
private class FmKickVoice(initialGain: Float) : FmDrumVoice() {
    override var gain = initialGain
    private val envCoeff = exp(-1.0 / (0.180 * SAMPLE_RATE)).toFloat()

    private var phase = 0.0
    private var freq = 50.0
    private val freqEnd = 30.0
    private val freqDecay = exp(-1.0 / (0.050 * SAMPLE_RATE))  // 50ms sweep tau

    private var modIndex = 4.0
    private val modIndexDecay = exp(-1.0 / (0.080 * SAMPLE_RATE))

    override fun nextSample(): Float {
        val s = (gain * sin(phase + modIndex * sin(phase))).toFloat()
        phase += TWO_PI * freq / SAMPLE_RATE
        gain *= envCoeff
        freq = freqEnd + (freq - freqEnd) * freqDecay
        modIndex *= modIndexDecay
        return s
    }
}

// Snare: 2-operator FM tone (200 Hz carrier, 180 Hz modulator, index 2) mixed with white noise
private class FmSnareVoice(initialGain: Float) : FmDrumVoice() {
    override var gain = initialGain
    private val envCoeff = exp(-1.0 / (0.120 * SAMPLE_RATE)).toFloat()

    private var carrierPhase = 0.0
    private val carrierPhaseInc = TWO_PI * 200.0 / SAMPLE_RATE
    private var modPhase = 0.0
    private val modPhaseInc = TWO_PI * 180.0 / SAMPLE_RATE
    private val modIndex = 2.0

    override fun nextSample(): Float {
        val tone = sin(carrierPhase + modIndex * sin(modPhase)).toFloat()
        val noise = Random.nextFloat() * 2f - 1f
        val s = gain * (0.5f * tone + 0.5f * noise)
        carrierPhase += carrierPhaseInc
        modPhase += modPhaseInc
        gain *= envCoeff
        return s
    }
}

// Hi-hat: 2-op FM with √2 carrier/modulator ratio for inharmonic metallic texture.
// decayMs controls open (350ms) vs closed (45ms).
private class FmHatVoice(initialGain: Float, decayMs: Double) : FmDrumVoice() {
    override var gain = initialGain
    private val envCoeff = exp(-1.0 / (decayMs / 1000.0 * SAMPLE_RATE)).toFloat()

    private var carrierPhase = 0.0
    private val carrierPhaseInc = TWO_PI * 8000.0 / SAMPLE_RATE
    private var modPhase = 0.0
    // √2 ratio → inharmonic partials give metallic "ping"
    private val modPhaseInc = TWO_PI * 8000.0 * 1.4142 / SAMPLE_RATE
    private val modIndex = 8.0

    override fun nextSample(): Float {
        val s = (gain * sin(carrierPhase + modIndex * sin(modPhase))).toFloat()
        carrierPhase += carrierPhaseInc
        modPhase += modPhaseInc
        gain *= envCoeff
        return s
    }
}

// Crash: 4-op FM chain at inharmonic frequencies with random initial phases + noise layer.
// Random phases on each hit ensure no two crashes sound identical.
private class FmCrashVoice(initialGain: Float) : FmDrumVoice() {
    override var gain = initialGain
    private val envCoeff = exp(-1.0 / (0.900 * SAMPLE_RATE)).toFloat()

    private val phases = DoubleArray(4) { Random.nextDouble() * TWO_PI }
    private val phaseIncs = doubleArrayOf(6000.0, 8485.0, 10392.0, 12728.0)
        .map { TWO_PI * it / SAMPLE_RATE }.toDoubleArray()
    private val modIndex = 5.0

    override fun nextSample(): Float {
        // chain: op3 → op2 → op1 → op0 (carrier)
        var mod = sin(phases[3])
        for (i in 2 downTo 0) mod = sin(phases[i] + modIndex * mod)
        val noise = Random.nextFloat() * 2f - 1f
        val s = gain * (0.7f * mod.toFloat() + 0.3f * noise)
        for (i in phaseIncs.indices) phases[i] += phaseIncs[i]
        gain *= envCoeff
        return s
    }
}

// Ride: 3-op FM at 600 Hz with inharmonic upper operators (index 1.5) for bell-like shimmer.
private class FmRideVoice(initialGain: Float) : FmDrumVoice() {
    override var gain = initialGain
    private val envCoeff = exp(-1.0 / (0.500 * SAMPLE_RATE)).toFloat()

    private var p0 = 0.0; private val i0 = TWO_PI * 600.0 / SAMPLE_RATE
    private var p1 = 0.0; private val i1 = TWO_PI * 2100.0 / SAMPLE_RATE  // 600 × 3.5
    private var p2 = 0.0; private val i2 = TWO_PI * 3060.0 / SAMPLE_RATE  // 600 × 5.1
    private val modIndex = 1.5

    override fun nextSample(): Float {
        val m2 = sin(p2)
        val m1 = sin(p1 + modIndex * m2)
        val s = (gain * sin(p0 + modIndex * m1)).toFloat()
        p0 += i0; p1 += i1; p2 += i2
        gain *= envCoeff
        return s
    }
}

// Tom: self-FM sine with frequency sweep (startFreq → startFreq/2) and mod index sweep 3→0.
private class FmTomVoice(initialGain: Float, startFreq: Double) : FmDrumVoice() {
    override var gain = initialGain
    private val envCoeff = exp(-1.0 / (0.150 * SAMPLE_RATE)).toFloat()

    private var phase = 0.0
    private var freq = startFreq
    private val freqEnd = startFreq * 0.5
    private val freqDecay = exp(-1.0 / (0.060 * SAMPLE_RATE))

    private var modIndex = 3.0
    private val modIndexDecay = exp(-1.0 / (0.080 * SAMPLE_RATE))

    override fun nextSample(): Float {
        val s = (gain * sin(phase + modIndex * sin(phase))).toFloat()
        phase += TWO_PI * freq / SAMPLE_RATE
        gain *= envCoeff
        freq = freqEnd + (freq - freqEnd) * freqDecay
        modIndex *= modIndexDecay
        return s
    }
}
