package org.egon12.btmid.synth

class AudioEngine {
    fun start() = NativeAudioEngine.start()
    fun stop()  = NativeAudioEngine.stop()
}
