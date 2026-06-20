package org.gilbertxenodike.btmid.bluetooth

import android.Manifest
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.ParcelUuid
import android.util.Log
import androidx.annotation.RequiresPermission
import java.util.UUID

private const val TAG = "BleScanner"
private val MIDI_SERVICE_UUID = UUID.fromString("03B80E5A-EDE8-4B33-A751-6CE34EC4C700")

class BleScanner(context: Context) {

    private val bluetoothLeScanner = context
        .getSystemService(BluetoothManager::class.java)
        .adapter
        .bluetoothLeScanner

    private val scanFilter = ScanFilter.Builder()
        .setServiceUuid(ParcelUuid(MIDI_SERVICE_UUID))
        .build()

    private val scanSettings = ScanSettings.Builder()
        .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
        .build()

    private var onDeviceFound: ((address: String, name: String) -> Unit)? = null

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device
            val name = result.scanRecord?.deviceName ?: device.address
            Log.d(TAG, "Found device: $name (${device.address})")
            onDeviceFound?.invoke(device.address, name)
        }

        override fun onScanFailed(errorCode: Int) {
            Log.e(TAG, "Scan failed with error: $errorCode")
        }
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_SCAN)
    fun startScan(onDeviceFound: (address: String, name: String) -> Unit) {
        this.onDeviceFound = onDeviceFound
        bluetoothLeScanner.startScan(listOf(scanFilter), scanSettings, scanCallback)
        Log.d(TAG, "Scan started")
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_SCAN)
    fun stopScan() {
        bluetoothLeScanner.stopScan(scanCallback)
        onDeviceFound = null
        Log.d(TAG, "Scan stopped")
    }
}
