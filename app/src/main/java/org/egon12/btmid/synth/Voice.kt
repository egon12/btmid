package org.egon12.btmid.synth

class Voice(
    val note: Int,
    val velocity: Int,
) {
    enum class EnvelopePhase { Attack, Decay, Sustain, Release, Done }

    val phaseAccumulators = DoubleArray(5)
    var envelopeGain: Float = 0f
    var envelopePhase: EnvelopePhase = EnvelopePhase.Attack
    var envelopeSamples: Int = 0
}
