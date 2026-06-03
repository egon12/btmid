package org.egon12.btmid.bluetooth

import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothProfile
import android.content.Context
import android.media.midi.MidiDevice
import android.media.midi.MidiManager
import android.media.midi.MidiOutputPort
import android.os.Handler
import android.os.Looper
import android.util.Log
import org.egon12.btmid.midi.AppMidiReceiver
import org.egon12.btmid.midi.MidiRouter

private const val TAG = "BleMidiConnection"

class BleMidiConnection(private val context: Context) {

    private val midiManager = context.getSystemService(MidiManager::class.java)
    private var gatt: BluetoothGatt? = null
    private var midiDevice: MidiDevice? = null
    private var outputPort: MidiOutputPort? = null

    fun connect(
        bluetoothDevice: BluetoothDevice,
        router: MidiRouter,
        onConnected: () -> Unit,
        onError: (String) -> Unit,
    ) {
        val gattCallback = object : BluetoothGattCallback() {
            override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    gatt.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH)
                    Log.d(TAG, "GATT connected, requested high connection priority")
                }
            }
        }
        gatt = bluetoothDevice.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)

        midiManager.openBluetoothDevice(
            bluetoothDevice,
            { device ->
                if (device == null) {
                    Log.e(TAG, "Failed to open MIDI device")
                    onError("Failed to open MIDI device")
                    return@openBluetoothDevice
                }
                midiDevice = device
                val port = device.openOutputPort(0)
                if (port == null) {
                    Log.e(TAG, "Failed to open output port")
                    onError("Failed to open output port")
                    return@openBluetoothDevice
                }
                outputPort = port
                port.connect(AppMidiReceiver(router))
                Log.d(TAG, "Connected to MIDI device: ${bluetoothDevice.address}")
                onConnected()
            },
            Handler(Looper.getMainLooper())
        )
    }

    fun disconnect() {
        outputPort?.close()
        outputPort = null
        midiDevice?.close()
        midiDevice = null
        gatt?.close()
        gatt = null
        Log.d(TAG, "Disconnected")
    }
}
