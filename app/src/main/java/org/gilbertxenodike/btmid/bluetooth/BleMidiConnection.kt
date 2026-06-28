package org.gilbertxenodike.btmid.bluetooth

import android.Manifest
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothProfile
import android.content.Context
import android.media.midi.MidiDevice
import android.media.midi.MidiManager
import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.annotation.RequiresPermission
import org.gilbertxenodike.btmid.midi.MidiRouter
import org.gilbertxenodike.btmid.synth.NativeAudioEngine

private const val TAG = "BleMidiConnection"

class BleMidiConnection(private val context: Context) {

    private val midiManager = context.getSystemService(MidiManager::class.java)
    private var gatt: BluetoothGatt? = null
    private var midiDevice: MidiDevice? = null

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    fun connect(
        bluetoothDevice: BluetoothDevice,
        router: MidiRouter,
        onConnected: () -> Unit,
        onError: (String) -> Unit,
    ) {
        val gattCallback = object : BluetoothGattCallback() {
            @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
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
                NativeAudioEngine.openMidiDevice(device, router)
                Log.d(TAG, "Connected to MIDI device via AMidi: ${bluetoothDevice.address}")
                onConnected()
            },
            Handler(Looper.getMainLooper())
        )
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    fun disconnect() {
        NativeAudioEngine.closeMidiDevice()
        midiDevice?.close()
        midiDevice = null
        gatt?.close()
        gatt = null
        Log.d(TAG, "Disconnected")
    }
}
