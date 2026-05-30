package org.egon12.btmid.synth

interface DrumSynth {
    fun noteOn(note: Int, velocity: Int)
    fun noteOff(note: Int)
    fun render(buffer: FloatArray)
}

// Thin delegating wrapper — backend can be swapped at runtime with a single @Volatile write.
// Both MidiRouter and AudioEngine hold the same proxy instance, so event routing and rendering
// always target the same backend without any locking.
class DrumSynthProxy(initial: DrumSynth) : DrumSynth {
    @Volatile var backend: DrumSynth = initial

    override fun noteOn(note: Int, velocity: Int) = backend.noteOn(note, velocity)
    override fun noteOff(note: Int) = backend.noteOff(note)
    override fun render(buffer: FloatArray) = backend.render(buffer)
}
