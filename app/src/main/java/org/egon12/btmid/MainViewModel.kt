package org.egon12.btmid

import android.Manifest.permission.BLUETOOTH_CONNECT
import android.Manifest.permission.BLUETOOTH_SCAN
import android.app.Application
import android.bluetooth.BluetoothManager
import android.content.pm.PackageManager.PERMISSION_GRANTED
import androidx.core.content.ContextCompat.checkSelfPermission
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.application
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.egon12.btmid.bluetooth.BleMidiConnection
import org.egon12.btmid.bluetooth.BleScanner
import org.egon12.btmid.midi.MidiEvent
import org.egon12.btmid.midi.MidiRouter
import org.egon12.btmid.synth.NativeAudioEngine
import org.egon12.btmid.synth.SampleBank

enum class ConnectionStatus { Idle, Scanning, Connected }
enum class DrumBackend { Noise, Fm, Samples }

enum class KeyboardSound { Piano, Sine, Saw, Square }

sealed class AudioEngine {
    object Oboe : AudioEngine()
    data class Wifi(val host: String, val port: Int) : AudioEngine()
}

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
    val engine: AudioEngine = AudioEngine.Oboe,
    val selectEngineDialogVisible: Boolean = false,
    val keyboardSound: KeyboardSound = KeyboardSound.Piano,
)

class MainViewModel(application: Application) : AndroidViewModel(application) {
    private val _uiState = MutableStateFlow(UiState())
    val uiState: StateFlow<UiState> = _uiState.asStateFlow()

    private val bluetoothAdapter = application
        .getSystemService(BluetoothManager::class.java)
        .adapter

    private val sampleBank = SampleBank(application)
    private val midiRouter = MidiRouter()
    private val bleScanner = BleScanner(application)
    private val bleMidiConnection = BleMidiConnection(application)
    private val drumBackendStore = DrumBackendStore(application)
    private val keyboardSoundStore: KeyboardSoundStore = KeyboardSoundStore(application)


    init {
        NativeAudioEngine.start()
        viewModelScope.launch {
            withContext(Dispatchers.IO) { sampleBank.load() }
            _uiState.value = _uiState.value.copy(samplesLoaded = true)
        }
        viewModelScope.launch {
            val saved = drumBackendStore.drumBackend.first()
            setDrumBackend(saved)
        }
        viewModelScope.launch {
            val saved = keyboardSoundStore.keyboardSound.first()
            setKeyboardSound(saved)
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
                    recentEvents = (current.recentEvents + MidiEventUiModel(description)).takeLast(
                        10
                    ),
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
        if (checkSelfPermission(application, BLUETOOTH_SCAN) != PERMISSION_GRANTED) {
            // TODO show error dialog
            return
        }
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
        if (checkSelfPermission(application, BLUETOOTH_SCAN) != PERMISSION_GRANTED) {
            // TODO show error dialog
            return
        }
        bleScanner.stopScan()
        _uiState.value = _uiState.value.copy(connectionStatus = ConnectionStatus.Idle)
    }

    fun connect(device: DeviceUiState) {
        val bluetoothDevice = bluetoothAdapter.getRemoteDevice(device.address)
        if (checkSelfPermission(application, BLUETOOTH_SCAN) != PERMISSION_GRANTED) {
            // TODO error dialog
            return
        }
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
        if (checkSelfPermission(application, BLUETOOTH_CONNECT) == PERMISSION_GRANTED) {
            bleMidiConnection.disconnect()
        }
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

    fun setKeyboardSound(sound: KeyboardSound) {
        val ids = arrayOf(
            "piano",
            "sine_oscillator",
            "saw_oscillator",
            "square_oscillator"
        )
        NativeAudioEngine.setInstrument(0, ids[sound.ordinal])
        _uiState.value = _uiState.value.copy(keyboardSound = sound)
        viewModelScope.launch { keyboardSoundStore.save(sound) }
    }

    override fun onCleared() {
        super.onCleared()
        if (checkSelfPermission(application, BLUETOOTH_SCAN) == PERMISSION_GRANTED) {
            bleScanner.stopScan()
            bleMidiConnection.disconnect()
        }
        NativeAudioEngine.stop()
    }

    fun showSelectEngineDialog(it: Boolean) {
        _uiState.value = _uiState.value.copy(selectEngineDialogVisible = it)
    }

    fun selectEngine(it: AudioEngine) {
        NativeAudioEngine.setEngine(it)
        NativeAudioEngine.setInstrument(0, "piano")
        NativeAudioEngine.setInstrument(9, "sample_drum")
        NativeAudioEngine.start()
    }
}
