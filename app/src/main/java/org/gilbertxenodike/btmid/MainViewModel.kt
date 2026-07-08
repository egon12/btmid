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
enum class LoopControlAction { Rec, Stop, Clear }

data class TimeSignature(
    val beatsPerBar: Int = 4,
    val noteValue: Int = 4,
    val bars: Int = 1,
) {
    val label: String
        get() = if (bars == 1) "$beatsPerBar/$noteValue" else "${bars}×$beatsPerBar/$noteValue"
}

sealed class AudioOutput {
    object Oboe : AudioOutput()
    data class Wifi(val host: String, val port: Int) : AudioOutput()
}

data class DeviceUiState(val address: String, val name: String)
data class MidiEventUiModel(val description: String)

data class ChannelStrip(
    val channel: Int,
    val label: String,
    val instrumentId: String,
    val volume: Float = 1f,
)

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
    val loopLengthSec: Int = 0,
    val timeSignature: TimeSignature = TimeSignature(),
    val channels: List<ChannelStrip> = listOf(
        ChannelStrip(0, "Keyboard", "piano"),
        ChannelStrip(9, "Drums", "noise_drum"),
    ),
    val selectedChannel: Int = 0,
    val mixerVisible: Boolean = false,
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
        val alreadyGranted = checkSelfPermission(application, BLUETOOTH_SCAN) == PERMISSION_GRANTED &&
                checkSelfPermission(application, BLUETOOTH_CONNECT) == PERMISSION_GRANTED
        if (alreadyGranted) _uiState.value = _uiState.value.copy(permissionsGranted = true)
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
            val updatedChannels = _uiState.value.channels.map {
                if (it.channel == 0) it.copy(instrumentId = id) else it
            }
            _uiState.value = _uiState.value.copy(
                keyboardType = savedType,
                synthWaveform = savedWave,
                channels = updatedChannels,
            )
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
        val drumId = drumBackendId(backend)
        val updatedChannels = _uiState.value.channels.map {
            if (it.channel == 9) it.copy(instrumentId = drumId) else it
        }
        _uiState.value = _uiState.value.copy(drumBackend = backend, channels = updatedChannels)
        viewModelScope.launch { drumBackendStore.save(backend) }
    }

    fun setKeyboardType(type: KeyboardType) {
        val channel = _uiState.value.selectedChannel
        val id = instrumentId(type, _uiState.value.synthWaveform)
        NativeAudioEngine.setInstrument(channel, id)
        val updatedChannels = _uiState.value.channels.map {
            if (it.channel == channel) it.copy(instrumentId = id) else it
        }
        _uiState.value = _uiState.value.copy(keyboardType = type, channels = updatedChannels)
        if (channel == 0) viewModelScope.launch { keyboardTypeStore.save(type) }
    }

    fun setWaveform(waveform: SynthWaveform) {
        val channel = _uiState.value.selectedChannel
        val id = instrumentId(_uiState.value.keyboardType, waveform)
        NativeAudioEngine.setInstrument(channel, id)
        val updatedChannels = _uiState.value.channels.map {
            if (it.channel == channel) it.copy(instrumentId = id) else it
        }
        _uiState.value = _uiState.value.copy(synthWaveform = waveform, channels = updatedChannels)
        if (channel == 0) viewModelScope.launch { waveformStore.save(waveform) }
    }

    fun setChannelVolume(channel: Int, volume: Float) {
        NativeAudioEngine.setChannelVolume(channel, volume)
        val updatedChannels = _uiState.value.channels.map {
            if (it.channel == channel) it.copy(volume = volume) else it
        }
        _uiState.value = _uiState.value.copy(channels = updatedChannels)
    }

    fun setChannelInstrument(channel: Int, id: String) {
        NativeAudioEngine.setInstrument(channel, id)
        val updatedChannels = _uiState.value.channels.map {
            if (it.channel == channel) it.copy(instrumentId = id) else it
        }
        _uiState.value = _uiState.value.copy(channels = updatedChannels)
        if (channel == _uiState.value.selectedChannel && !id.contains("drum")) {
            val type = keyboardTypeFromId(id)
            val waveform = waveformFromId(id)
            _uiState.value = _uiState.value.copy(keyboardType = type, synthWaveform = waveform)
        }
        if (channel == 9) {
            val backend = drumBackendFromId(id)
            _uiState.value = _uiState.value.copy(drumBackend = backend)
        }
    }

    fun addChannel() {
        val used = _uiState.value.channels.map { it.channel }.toSet()
        val next = (0..15).firstOrNull { it != 9 && it !in used } ?: return
        val strip = ChannelStrip(next, "Ch ${next + 1}", "piano")
        NativeAudioEngine.setInstrument(next, "piano")
        NativeAudioEngine.setChannelVolume(next, 1f)
        _uiState.value = _uiState.value.copy(
            channels = _uiState.value.channels + strip
        )
    }

    fun removeChannel(channel: Int) {
        if (channel == 0 || channel == 9) return
        NativeAudioEngine.setChannelVolume(channel, 0f)
        val updatedChannels = _uiState.value.channels.filter { it.channel != channel }
        val newSelected = if (_uiState.value.selectedChannel == channel) 0
                          else _uiState.value.selectedChannel
        _uiState.value = _uiState.value.copy(channels = updatedChannels, selectedChannel = newSelected)
    }

    fun selectChannel(channel: Int) {
        val strip = _uiState.value.channels.find { it.channel == channel }
        val type = strip?.let { keyboardTypeFromId(it.instrumentId) } ?: KeyboardType.Piano
        val waveform = strip?.let { waveformFromId(it.instrumentId) } ?: SynthWaveform.Sine
        _uiState.value = _uiState.value.copy(
            selectedChannel = channel,
            keyboardType = type,
            synthWaveform = waveform,
        )
    }

    fun showMixer(visible: Boolean) {
        _uiState.value = _uiState.value.copy(mixerVisible = visible)
    }

    private fun keyboardTypeFromId(id: String): KeyboardType = when {
        id.endsWith("_polysynth") -> KeyboardType.Poly
        id.endsWith("_monosynth") -> KeyboardType.Mono
        else -> KeyboardType.Piano
    }

    private fun waveformFromId(id: String): SynthWaveform = when {
        id.startsWith("saw") -> SynthWaveform.Saw
        id.startsWith("square") -> SynthWaveform.Square
        else -> SynthWaveform.Sine
    }

    private fun drumBackendFromId(id: String): DrumBackend = when (id) {
        "fm_drum" -> DrumBackend.Fm
        "sample_drum" -> DrumBackend.Samples
        else -> DrumBackend.Noise
    }

    private fun drumBackendId(backend: DrumBackend): String = when (backend) {
        DrumBackend.Noise -> "noise_drum"
        DrumBackend.Fm -> "fm_drum"
        DrumBackend.Samples -> "sample_drum"
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

    fun setTimeSignature(sig: TimeSignature) {
        _uiState.value = _uiState.value.copy(timeSignature = sig)
    }

    fun dispatchLoopControlAction(action: LoopControlAction) {
        when (action) {
            LoopControlAction.Rec -> NativeAudioEngine.loopRecord()
            LoopControlAction.Stop -> NativeAudioEngine.loopStop()
            LoopControlAction.Clear -> NativeAudioEngine.loopClear()
        }
    }

    override fun onLoopStateChange(state: Int) {
        val ls = when (state) {
            1 -> LoopState.Recording
            2 -> LoopState.Playing
            3 -> LoopState.Armed
            4 -> LoopState.Overdubbing
            else -> LoopState.Idle
        }
        _uiState.value = _uiState.value.copy(loopState = ls)
    }

    override fun onLoopProgress(progress: Int) {
        _uiState.value = _uiState.value.copy(loopLengthSec = progress)
    }

    fun selectOutput(engine: AudioOutput) {
        val current = _uiState.value
        NativeAudioEngine.setOutput(engine)
        _uiState.value = current.copy(engine = engine)
    }
}
