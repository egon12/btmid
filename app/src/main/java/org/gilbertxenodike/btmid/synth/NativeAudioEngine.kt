package org.gilbertxenodike.btmid.synth

import android.media.midi.MidiDevice
import org.gilbertxenodike.btmid.AudioOutput

object NativeAudioEngine {
    init {
        System.loadLibrary("btmid")
    }

    private val ptr: Long = create()

    interface MidiEventListener {
        fun onMidiEvent(channel: Int, type: Int, data1: Int, data2: Int)
    }

    interface LoopStateListener {
        fun onLoopStateChange(state: Int)

        // run from 0 until 100 maybe per 25
        fun onLoopProgress(progress: Int)
    }


    fun start() = start(ptr)
    fun stop() = stop(ptr)
    fun noteOn(channel: Int, note: Int, velocity: Int) = noteOn(ptr, channel, note, velocity)
    fun noteOff(channel: Int, note: Int) = noteOff(ptr, channel, note)
    fun controlChange(channel: Int, cc: Int, value: Int) = controlChange(ptr, channel, cc, value)
    fun loadSample(name: String, data: FloatArray) = loadSample(ptr, name, data)
    fun setInstrument(channel: Int, name: String) = setInstrument(ptr, channel, name)
    fun setDrumBackend(backendId: Int) = setDrumBackend(ptr, backendId)

    fun setOutput(audioOutput: AudioOutput) {
        when (audioOutput) {
            AudioOutput.Oboe -> setOutput(ptr, 1, "")
            is AudioOutput.Wifi -> setOutput(ptr, 2, audioOutput.host)
        }
    }

    fun openMidiDevice(device: MidiDevice, listener: MidiEventListener) =
        openMidiDevice(ptr, device, listener)

    fun closeMidiDevice() = closeMidiDevice(ptr)

    fun setLoopStateListener(callback: LoopStateListener) = setLoopStateListener(ptr, callback)
    fun loopRecord() = loopRecord(ptr)
    fun loopPlay() = loopPlay(ptr)
    fun loopStop() = loopStop(ptr)
    fun loopClear() = loopClear(ptr)
    fun loopState(): Int = loopState(ptr)

    external fun benchmarkPianos(): String

    private external fun create(): Long
    private external fun start(ptr: Long)
    private external fun stop(ptr: Long)
    private external fun destroy(ptr: Long)
    private external fun noteOn(ptr: Long, channel: Int, note: Int, velocity: Int)
    private external fun noteOff(ptr: Long, channel: Int, note: Int)
    private external fun controlChange(ptr: Long, channel: Int, cc: Int, value: Int)
    private external fun loadSample(ptr: Long, name: String, data: FloatArray)
    private external fun setInstrument(ptr: Long, channel: Int, name: String)
    private external fun setDrumBackend(ptr: Long, backendId: Int)

    private external fun setOutput(ptr: Long, engineId: Int, ip: String)
    private external fun openMidiDevice(ptr: Long, device: MidiDevice, listener: MidiEventListener)
    private external fun closeMidiDevice(ptr: Long)

    private external fun setLoopStateListener(ptr: Long, callback: LoopStateListener)
    private external fun loopRecord(ptr: Long)
    private external fun loopPlay(ptr: Long)
    private external fun loopStop(ptr: Long)
    private external fun loopClear(ptr: Long)
    private external fun loopState(ptr: Long): Int
}
