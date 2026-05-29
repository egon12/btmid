package org.egon12.btmid.synth

import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

private const val SAMPLE_RATE = 44100

class AudioEngine(
    private val pianoSynth: PianoSynth,
    private val drumSynth: DrumSynth,
) {
    private val scope = CoroutineScope(Dispatchers.Default + SupervisorJob())
    private var renderJob: Job? = null

    private val bufferSizeBytes: Int
    private val bufferSizeSamples: Int
    private val audioTrack: AudioTrack
    private val renderBuffer: FloatArray

    init {
        val minBytes = AudioTrack.getMinBufferSize(
            SAMPLE_RATE,
            AudioFormat.CHANNEL_OUT_MONO,
            AudioFormat.ENCODING_PCM_FLOAT
        )
        bufferSizeBytes = maxOf(minBytes, SAMPLE_RATE / 200 * 4)  // 5ms floor
        bufferSizeSamples = bufferSizeBytes / 4
        renderBuffer = FloatArray(bufferSizeSamples)

        audioTrack = AudioTrack.Builder()
            .setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_GAME)
                    .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                    .build()
            )
            .setAudioFormat(
                AudioFormat.Builder()
                    .setEncoding(AudioFormat.ENCODING_PCM_FLOAT)
                    .setSampleRate(SAMPLE_RATE)
                    .setChannelMask(AudioFormat.CHANNEL_OUT_MONO)
                    .build()
            )
            .setBufferSizeInBytes(bufferSizeBytes)
            .setTransferMode(AudioTrack.MODE_STREAM)
            .setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY)
            .build()
    }

    fun start() {
        audioTrack.play()
        renderJob = scope.launch {
            while (isActive) {
                renderBuffer.fill(0f)
                pianoSynth.render(renderBuffer)
                drumSynth.render(renderBuffer)
                audioTrack.write(renderBuffer, 0, bufferSizeSamples, AudioTrack.WRITE_BLOCKING)
            }
        }
    }

    fun stop() {
        renderJob?.cancel()
        renderJob = null
        audioTrack.stop()
        audioTrack.release()
    }
}
