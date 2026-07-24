package org.gilbertxenodike.btmid.synth

import android.content.Context

// Loads an SF2 SoundFont asset and hands the raw bytes to the native engine.
// TinySoundFont parses SF2 itself, so no decode/resample is needed here.
class SoundFontBank(private val context: Context) {

    var isLoaded = false
        private set

    fun load() {
        val bytes = context.assets.open(ASSET_PATH).use { it.readBytes() }
        NativeAudioEngine.loadSoundFont(bytes)
        isLoaded = true
    }

    companion object {
        private const val ASSET_PATH = "soundfonts/gm.sf2"
    }
}
