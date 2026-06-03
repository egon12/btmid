package org.egon12.btmid

import android.app.Application
import android.bluetooth.BluetoothManager
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import org.egon12.btmid.bluetooth.BleMidiConnection
import org.egon12.btmid.bluetooth.BleScanner
import org.egon12.btmid.midi.MidiEvent
import org.egon12.btmid.midi.MidiRouter
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.egon12.btmid.synth.AudioEngine
import org.egon12.btmid.synth.NativeAudioEngine
import org.egon12.btmid.synth.SampleBank

enum class ConnectionStatus { Idle, Scanning, Connected }
enum class DrumBackend { Noise, Fm, Samples }

data class DeviceUiState(val address: String, val name: String)
data class MidiEventUiModel(val description: String)

data class UiState(
    val permissionsGranted: Boolean = false,
    val connectionStatus: ConnectionStatus = ConnectionStatus.Idle,
    val discoveredDevices: List<DeviceUiState> = emptyList(),
    val connectedDeviceAddress: String? = null,
    val recentEvents: List<MidiEventUiModel> = emptyList(),
    val midiActivityPulse: Boolean = false,
    val drumBackend: DrumBackend = DrumBackend.Noise,
    val samplesLoaded: Boolean = false,
)

class MainViewModel(application: Application) : AndroidViewModel(application) {
    private val _uiState = MutableStateFlow(UiState())
    val uiState: StateFlow<UiState> = _uiState.asStateFlow()

    private val bluetoothAdapter = application
        .getSystemService(BluetoothManager::class.java)
        .adapter

    private val sampleBank = SampleBank(application)
    private val midiRouter = MidiRouter()
    private val audioEngine = AudioEngine()
    private val bleScanner = BleScanner(application)
    private val bleMidiConnection = BleMidiConnection(application)
    private val drumBackendStore = DrumBackendStore(application)

    init {
        audioEngine.start()
        viewModelScope.launch {
            withContext(Dispatchers.IO) { sampleBank.load() }
            _uiState.value = _uiState.value.copy(samplesLoaded = true)
        }
        viewModelScope.launch {
            val saved = drumBackendStore.drumBackend.first()
            setDrumBackend(saved)
        }
        viewModelScope.launch {
            midiRouter.events.collect { event ->
                val description = when (event) {
                    is MidiEvent.NoteOn -> "NoteOn  ch${event.channel + 1} note=${event.note} vel=${event.velocity}"
                    is MidiEvent.NoteOff -> "NoteOff ch${event.channel + 1} note=${event.note}"
                    is MidiEvent.ControlChange -> "CC      ch${event.channel + 1} ctrl=${event.controller} val=${event.value}"
                }
                val current = _uiState.value
                _uiState.value = current.copy(
                    recentEvents = (current.recentEvents + MidiEventUiModel(description)).takeLast(10),
                    midiActivityPulse = !current.midiActivityPulse,
                )
            }
        }
    }

    fun onPermissionsResult(granted: Boolean) {
        _uiState.value = _uiState.value.copy(permissionsGranted = granted)
    }

    fun startScan() {
        _uiState.value = _uiState.value.copy(
            connectionStatus = ConnectionStatus.Scanning,
            discoveredDevices = emptyList()
        )
        bleScanner.startScan { address, name ->
            val current = _uiState.value.discoveredDevices
            if (current.none { it.address == address }) {
                _uiState.value = _uiState.value.copy(
                    discoveredDevices = current + DeviceUiState(address, name)
                )
            }
        }
    }

    fun stopScan() {
        bleScanner.stopScan()
        _uiState.value = _uiState.value.copy(connectionStatus = ConnectionStatus.Idle)
    }

    fun connect(device: DeviceUiState) {
        val bluetoothDevice = bluetoothAdapter.getRemoteDevice(device.address)
        bleScanner.stopScan()
        bleMidiConnection.connect(
            bluetoothDevice = bluetoothDevice,
            router = midiRouter,
            onConnected = {
                _uiState.value = _uiState.value.copy(
                    connectionStatus = ConnectionStatus.Connected,
                    connectedDeviceAddress = device.address,
                )
            },
            onError = {
                _uiState.value = _uiState.value.copy(connectionStatus = ConnectionStatus.Idle)
            }
        )
    }

    fun disconnect() {
        bleMidiConnection.disconnect()
        _uiState.value = _uiState.value.copy(
            connectionStatus = ConnectionStatus.Idle,
            connectedDeviceAddress = null,
        )
    }

    fun setDrumBackend(backend: DrumBackend) {
        NativeAudioEngine.setDrumBackend(backend.ordinal)
        _uiState.value = _uiState.value.copy(drumBackend = backend)
        viewModelScope.launch { drumBackendStore.save(backend) }
    }

    override fun onCleared() {
        super.onCleared()
        bleScanner.stopScan()
        bleMidiConnection.disconnect()
        audioEngine.stop()
    }
}
