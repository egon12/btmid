package org.egon12.btmid.synth

import android.content.Context
import android.media.AudioFormat
import android.media.MediaCodec
import android.media.MediaExtractor
import android.media.MediaFormat
import java.nio.ByteOrder

private val DRUM_ASSETS = listOf(
    "kick", "snare", "closed_hat", "open_hat", "crash", "ride", "tom"
)

class SampleBank(private val context: Context) {

    private val samples = HashMap<String, FloatArray>(DRUM_ASSETS.size * 2)

    val isLoaded get() = samples.isNotEmpty()

    fun load() {
        for (name in DRUM_ASSETS) {
            val pcm = decodeAsset("samples/drums/$name.ogg")
            samples[name] = pcm
            NativeAudioEngine.loadSample(name, pcm)
        }
    }

    operator fun get(name: String): FloatArray? = samples[name]

    private fun decodeAsset(path: String): FloatArray {
        val afd = context.assets.openFd(path)
        val extractor = MediaExtractor()
        extractor.setDataSource(afd)
        afd.close()

        val trackIndex = (0 until extractor.trackCount).first { i ->
            extractor.getTrackFormat(i).getString(MediaFormat.KEY_MIME)
                ?.startsWith("audio/") == true
        }
        extractor.selectTrack(trackIndex)

        val srcFormat = extractor.getTrackFormat(trackIndex)
        val channelCount = srcFormat.getInteger(MediaFormat.KEY_CHANNEL_COUNT)
        val mime = srcFormat.getString(MediaFormat.KEY_MIME)!!

        val codec = MediaCodec.createDecoderByType(mime)
        codec.configure(srcFormat, null, null, 0)
        codec.start()

        val pcm = mutableListOf<Float>()
        val info = MediaCodec.BufferInfo()
        var inputDone = false
        var outputDone = false

        while (!outputDone) {
            if (!inputDone) {
                val idx = codec.dequeueInputBuffer(0)
                if (idx >= 0) {
                    val buf = codec.getInputBuffer(idx)!!
                    val n = extractor.readSampleData(buf, 0)
                    if (n < 0) {
                        codec.queueInputBuffer(idx, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM)
                        inputDone = true
                    } else {
                        codec.queueInputBuffer(idx, 0, n, extractor.sampleTime, 0)
                        extractor.advance()
                    }
                }
            }

            val outIdx = codec.dequeueOutputBuffer(info, 10_000L)
            if (outIdx == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) continue
            if (outIdx < 0) continue

            val outBuf = codec.getOutputBuffer(outIdx)!!.also {
                it.order(ByteOrder.nativeOrder())
            }
            val outFormat = codec.getOutputFormat()
            val encoding = if (outFormat.containsKey(MediaFormat.KEY_PCM_ENCODING))
                outFormat.getInteger(MediaFormat.KEY_PCM_ENCODING)
            else AudioFormat.ENCODING_PCM_16BIT

            if (encoding == AudioFormat.ENCODING_PCM_FLOAT) {
                val fb = outBuf.asFloatBuffer()
                while (fb.hasRemaining()) pcm.add(fb.get())
            } else {
                val sb = outBuf.asShortBuffer()
                while (sb.hasRemaining()) pcm.add(sb.get() / 32768f)
            }

            codec.releaseOutputBuffer(outIdx, false)
            if (info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM != 0) outputDone = true
        }

        codec.stop()
        codec.release()
        extractor.release()

        val srcRate = srcFormat.getInteger(MediaFormat.KEY_SAMPLE_RATE)
        val mono = downmixToMono(pcm.toFloatArray(), channelCount)
        return if (srcRate == TARGET_SAMPLE_RATE) mono else resample(mono, srcRate, TARGET_SAMPLE_RATE)
    }

    private fun downmixToMono(interleaved: FloatArray, channels: Int): FloatArray {
        if (channels == 1) return interleaved
        return FloatArray(interleaved.size / channels) { i ->
            var sum = 0f
            for (ch in 0 until channels) sum += interleaved[i * channels + ch]
            sum / channels
        }
    }

    private fun resample(src: FloatArray, srcRate: Int, dstRate: Int): FloatArray {
        val ratio = srcRate.toDouble() / dstRate
        val dstLen = (src.size / ratio).toInt()
        return FloatArray(dstLen) { i ->
            val pos = i * ratio
            val lo = pos.toInt().coerceAtMost(src.size - 1)
            val hi = (lo + 1).coerceAtMost(src.size - 1)
            val frac = (pos - lo).toFloat()
            src[lo] * (1f - frac) + src[hi] * frac
        }
    }

    companion object {
        // Must match kSampleRate in AudioConfig.h
        private const val TARGET_SAMPLE_RATE = 48000
    }
}
