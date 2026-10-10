package org.gilbertxenodike.btmid.synth

import android.content.Context
import android.net.Uri
import java.io.File

// Lets the user supply their own GM SoundFont from device storage instead of
// bundling one in the APK (a usable GM .sf2 is commonly 50-150+ MB).
// TinySoundFont parses SF2 directly from a file path, so this just keeps one
// copy in app-private storage and hands native code the path.
class SoundFontBank(private val context: Context) {

    var isLoaded = false
        private set

    private val file: File
        get() = File(File(context.filesDir, "soundfonts"), "gm.sf2")

    // Call once at startup: loads the previously-imported font, if any.
    // Returns false (without error) if the user has never imported one.
    fun loadPersisted(): Boolean {
        val f = file
        if (!f.exists()) return false
        NativeAudioEngine.loadSoundFontFile(f.absolutePath)
        isLoaded = true
        return true
    }

    // Call when the user picks a .sf2 file via the system file picker.
    // Copies it into app-private storage (via a temp file, so a cancelled or
    // failed copy never leaves a half-written font behind), then loads it.
    fun import(uri: Uri): Boolean {
        val target = file
        target.parentFile?.mkdirs()
        val tmp = File(target.parentFile, "${target.name}.tmp")
        val copied = context.contentResolver.openInputStream(uri)?.use { input ->
            tmp.outputStream().use { output -> input.copyTo(output) }
            true
        } ?: false
        if (!copied) {
            tmp.delete()
            return false
        }
        tmp.renameTo(target)
        NativeAudioEngine.loadSoundFontFile(target.absolutePath)
        isLoaded = true
        return true
    }
}
