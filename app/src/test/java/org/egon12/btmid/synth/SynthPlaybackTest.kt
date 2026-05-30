package org.egon12.btmid.synth

import org.junit.Assume.assumeTrue
import org.junit.Test
import javax.sound.sampled.AudioFormat
import javax.sound.sampled.AudioSystem
import javax.sound.sampled.DataLine
import javax.sound.sampled.SourceDataLine

private const val SAMPLE_RATE = 44100
private const val CHUNK = 512

/**
 * Run individually from IDE or:
 *   ./gradlew :app:testDebugUnitTest --tests "org.egon12.btmid.synth.SynthPlaybackTest"
 *
 * Tests skip automatically when no audio output is available (e.g. headless CI).
 */
class SynthPlaybackTest {

    @Test
    fun `piano - C major scale`() = withAudio { line ->
        val piano = PianoSynth()
        val scale = intArrayOf(60, 62, 64, 65, 67, 69, 71, 72) // C D E F G A B C
        for (note in scale) {
            piano.noteOn(note, 100)
            line.renderMs(350) { piano.render(it) }
            piano.noteOff(note)
            line.renderMs(150) { piano.render(it) }
        }
    }

    @Test
    fun `piano - C major chord`() = withAudio { line ->
        val piano = PianoSynth()
        for (note in intArrayOf(60, 64, 67)) piano.noteOn(note, 90)  // C E G
        line.renderMs(800) { piano.render(it) }
        for (note in intArrayOf(60, 64, 67)) piano.noteOff(note)
        line.renderMs(400) { piano.render(it) }
    }

    @Test
    fun `drums - basic 4-4 pattern`() = withAudio { line ->
        val drums = NoiseDrumSynth()
        repeat(4) { // 4 bars at ~120 BPM
            // Beat 1
            drums.noteOn(36, 100); drums.noteOn(42, 80)
            line.renderMs(250) { drums.render(it) }
            // Beat 1.5
            drums.noteOn(42, 60)
            line.renderMs(250) { drums.render(it) }
            // Beat 2
            drums.noteOn(38, 100); drums.noteOn(42, 80)
            line.renderMs(250) { drums.render(it) }
            // Beat 2.5
            drums.noteOn(42, 60)
            line.renderMs(250) { drums.render(it) }
            // Beat 3
            drums.noteOn(36, 100); drums.noteOn(42, 80)
            line.renderMs(250) { drums.render(it) }
            // Beat 3.5
            drums.noteOn(42, 60)
            line.renderMs(250) { drums.render(it) }
            // Beat 4
            drums.noteOn(38, 100); drums.noteOn(42, 80)
            line.renderMs(250) { drums.render(it) }
            // Beat 4.5
            drums.noteOn(42, 60)
            line.renderMs(250) { drums.render(it) }
        }
    }

    @Test
    fun `fm drums - kick and snare`() = withAudio { line ->
        val drums = FmDrumSynth()
        repeat(4) {
            drums.noteOn(36, 100); drums.noteOn(42, 80)
            line.renderMs(250) { drums.render(it) }
            drums.noteOn(42, 60)
            line.renderMs(250) { drums.render(it) }
            drums.noteOn(38, 100); drums.noteOn(42, 80)
            line.renderMs(250) { drums.render(it) }
            drums.noteOn(42, 60)
            line.renderMs(250) { drums.render(it) }
        }
    }

    @Test
    fun `fm drums - all voices`() = withAudio { line ->
        val drums = FmDrumSynth()
        val voices = mapOf(
            36 to "Kick", 38 to "Snare",
            42 to "Closed Hat", 46 to "Open Hat",
            49 to "Crash", 51 to "Ride",
            41 to "Low Floor Tom", 43 to "High Floor Tom",
            45 to "Low Tom", 47 to "Low-Mid Tom",
            48 to "Hi-Mid Tom", 50 to "High Tom",
        )
        for ((note, _) in voices) {
            drums.noteOn(note, 100)
            line.renderMs(700) { drums.render(it) }
        }
    }

    @Test
    fun `drums - all voices`() = withAudio { line ->
        val drums = NoiseDrumSynth()
        val voices = mapOf(
            36 to "Bass Drum",
            38 to "Snare",
            42 to "Closed Hat",
            46 to "Open Hat",
            49 to "Crash",
            51 to "Ride",
        )
        for ((note, _) in voices) {
            drums.noteOn(note, 100)
            line.renderMs(600) { drums.render(it) }
        }
    }
}

private fun withAudio(block: (SourceDataLine) -> Unit) {
    val format = AudioFormat(SAMPLE_RATE.toFloat(), 16, 1, true, false)
    assumeTrue(AudioSystem.isLineSupported(DataLine.Info(SourceDataLine::class.java, format)))
    val line = AudioSystem.getSourceDataLine(format)
    line.open(format, CHUNK * 2)
    line.start()
    block(line)
    line.drain()
    line.close()
}

private fun SourceDataLine.renderMs(ms: Int, render: (FloatArray) -> Unit) {
    val totalSamples = SAMPLE_RATE.toLong() * ms / 1000
    val floatBuf = FloatArray(CHUNK)
    val pcmBuf = ByteArray(CHUNK * 2)
    var rendered = 0L
    while (rendered < totalSamples) {
        floatBuf.fill(0f)
        render(floatBuf)
        for (i in floatBuf.indices) {
            val s = (floatBuf[i].coerceIn(-1f, 1f) * 32767f).toInt()
            pcmBuf[i * 2]     = (s and 0xFF).toByte()
            pcmBuf[i * 2 + 1] = ((s ushr 8) and 0xFF).toByte()
        }
        write(pcmBuf, 0, pcmBuf.size)
        rendered += CHUNK
    }
}
