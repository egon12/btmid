package org.egon12.btmid.synth

import android.media.midi.MidiDevice

object NativeAudioEngine {
    init {
        System.loadLibrary("btmid")
    }

    private val ptr: Long = create()

    fun interface MidiEventListener {
        fun onMidiEvent(channel: Int, type: Int, data1: Int, data2: Int)
    }

    fun start() = start(ptr)
    fun stop() = stop(ptr)
    fun noteOn(channel: Int, note: Int, velocity: Int) = noteOn(ptr, channel, note, velocity)
    fun noteOff(channel: Int, note: Int) = noteOff(ptr, channel, note)
    fun loadSample(name: String, data: FloatArray) = loadSample(ptr, name, data)
    fun setDrumBackend(backendId: Int) = setDrumBackend(ptr, backendId)
    fun setOutputPort(device: MidiDevice, listener: MidiEventListener) =
        setOutputPort(ptr, device, listener)
    fun clearOutputPort() = clearOutputPort(ptr)

    private external fun create(): Long
    private external fun start(ptr: Long)
    private external fun stop(ptr: Long)
    private external fun destroy(ptr: Long)
    private external fun noteOn(ptr: Long, channel: Int, note: Int, velocity: Int)
    private external fun noteOff(ptr: Long, channel: Int, note: Int)
    private external fun loadSample(ptr: Long, name: String, data: FloatArray)
    private external fun setDrumBackend(ptr: Long, backendId: Int)
    private external fun setOutputPort(ptr: Long, device: MidiDevice, callback: MidiEventListener)
    private external fun clearOutputPort(ptr: Long)
}
