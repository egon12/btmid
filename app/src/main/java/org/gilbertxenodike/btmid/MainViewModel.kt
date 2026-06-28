package org.gilbertxenodike.btmid

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
import org.gilbertxenodike.btmid.bluetooth.BleMidiConnection
import org.gilbertxenodike.btmid.bluetooth.BleScanner
import org.gilbertxenodike.btmid.midi.MidiEvent
import org.gilbertxenodike.btmid.midi.MidiRouter
import org.gilbertxenodike.btmid.synth.NativeAudioEngine
import org.gilbertxenodike.btmid.synth.SampleBank

enum class ConnectionStatus { Idle, Scanning, Connected }
enum class DrumBackend { Noise, Fm, Samples }

enum class KeyboardType { Piano, Poly, Mono }
enum class SynthWaveform { Sine, Saw, Square }
enum class LoopState { Idle, Recording, Playing, Armed, Overdubbing }

sealed class AudioOutput {
    object Oboe : AudioOutput()
    data class Wifi(val host: String, val port: Int) : AudioOutput()
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
    val engine: AudioOutput = AudioOutput.Oboe,
    val selectEngineDialogVisible: Boolean = false,
    val keyboardType: KeyboardType = KeyboardType.Piano,
    val synthWaveform: SynthWaveform = SynthWaveform.Sine,
    val loopState: LoopState = LoopState.Idle,
    val loopLengthSec: Float = 0f,
)

class MainViewModel(application: Application) : AndroidViewModel(application),
    NativeAudioEngine.LoopStateListener {
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
    private val keyboardTypeStore = KeyboardTypeStore(application)
    private val waveformStore = WaveformStore(application)


    init {
        NativeAudioEngine.start()
        NativeAudioEngine.setLoopStateListener(this)
        viewModelScope.launch {
            withContext(Dispatchers.IO) { sampleBank.load() }
            _uiState.value = _uiState.value.copy(samplesLoaded = true)
        }
        viewModelScope.launch {
            val saved = drumBackendStore.drumBackend.first()
            setDrumBackend(saved)
        }
        viewModelScope.launch {
            val savedType = keyboardTypeStore.keyboardType.first()
            val savedWave = waveformStore.waveform.first()
            val id = instrumentId(savedType, savedWave)
            NativeAudioEngine.setInstrument(0, id)
            _uiState.value =
                _uiState.value.copy(keyboardType = savedType, synthWaveform = savedWave)
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

    private fun instrumentId(type: KeyboardType, waveform: SynthWaveform): String {
        if (type == KeyboardType.Piano) return "piano"
        val wavePart = waveform.name.lowercase()
        val typePart = if (type == KeyboardType.Poly) "polysynth" else "monosynth"
        return "${wavePart}_${typePart}"
    }

    fun setDrumBackend(backend: DrumBackend) {
        NativeAudioEngine.setDrumBackend(backend.ordinal)
        _uiState.value = _uiState.value.copy(drumBackend = backend)
        viewModelScope.launch { drumBackendStore.save(backend) }
    }

    fun setKeyboardType(type: KeyboardType) {
        val id = instrumentId(type, _uiState.value.synthWaveform)
        NativeAudioEngine.setInstrument(0, id)
        _uiState.value = _uiState.value.copy(keyboardType = type)
        viewModelScope.launch { keyboardTypeStore.save(type) }
    }

    fun setWaveform(waveform: SynthWaveform) {
        val id = instrumentId(_uiState.value.keyboardType, waveform)
        NativeAudioEngine.setInstrument(0, id)
        _uiState.value = _uiState.value.copy(synthWaveform = waveform)
        viewModelScope.launch { waveformStore.save(waveform) }
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

    fun loopRecord() {
        NativeAudioEngine.loopRecord()
        _uiState.value = _uiState.value.copy(loopState = LoopState.Armed, loopLengthSec = 0f)
    }

    fun loopStop() {
        NativeAudioEngine.loopPlay()
        _uiState.value = _uiState.value.copy(loopState = LoopState.Playing)
    }

    fun loopClear() {
        NativeAudioEngine.loopClear()
        _uiState.value = _uiState.value.copy(loopState = LoopState.Idle, loopLengthSec = 0f)
    }

    fun selectOutput(engine: AudioOutput) {
        val current = _uiState.value
        NativeAudioEngine.setOutput(engine)
        _uiState.value = current.copy(engine = engine)
    }

    override fun onLoopState(state: Int) {
        val ls = when (state) {
            1 -> LoopState.Recording
            2 -> LoopState.Playing
            3 -> LoopState.Armed
            4 -> LoopState.Overdubbing
            else -> LoopState.Idle
        }
        _uiState.value = _uiState.value.copy(loopState = ls)
    }
}
