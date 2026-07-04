package org.gilbertxenodike.btmid.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Card
import androidx.compose.material3.FilterChip
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import org.gilbertxenodike.btmid.ChannelStrip
import org.gilbertxenodike.btmid.KeyboardType
import org.gilbertxenodike.btmid.SynthWaveform
import org.gilbertxenodike.btmid.UiState

@Composable
fun MixerScreen(
    uiState: UiState,
    onBack: () -> Unit,
    onSetChannelInstrument: (channel: Int, id: String) -> Unit,
    onSetChannelVolume: (channel: Int, volume: Float) -> Unit,
    onAddChannel: () -> Unit,
    onRemoveChannel: (channel: Int) -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            TextButton(onClick = onBack) { Text("← Back") }
            Text(
                text = "Mixer",
                style = MaterialTheme.typography.titleMedium,
                modifier = Modifier.padding(start = 8.dp),
            )
        }

        HorizontalDivider()

        uiState.channels.forEach { strip ->
            ChannelStripCard(
                strip = strip,
                samplesLoaded = uiState.samplesLoaded,
                canRemove = strip.channel != 0 && strip.channel != 9,
                onSetInstrument = { id -> onSetChannelInstrument(strip.channel, id) },
                onSetVolume = { vol -> onSetChannelVolume(strip.channel, vol) },
                onRemove = { onRemoveChannel(strip.channel) },
            )
        }

        OutlinedButton(
            onClick = onAddChannel,
            modifier = Modifier.align(Alignment.CenterHorizontally),
        ) {
            Text("+ Add Channel")
        }
    }
}

@Composable
private fun ChannelStripCard(
    strip: ChannelStrip,
    samplesLoaded: Boolean,
    canRemove: Boolean,
    onSetInstrument: (String) -> Unit,
    onSetVolume: (Float) -> Unit,
    onRemove: () -> Unit,
) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(
            modifier = Modifier.padding(12.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    text = "Ch ${strip.channel + 1} · ${strip.label}",
                    style = MaterialTheme.typography.titleSmall,
                )
                if (canRemove) {
                    TextButton(onClick = onRemove) { Text("Remove") }
                }
            }

            if (strip.channel == 9) {
                DrumInstrumentPicker(
                    instrumentId = strip.instrumentId,
                    samplesLoaded = samplesLoaded,
                    onSelect = onSetInstrument,
                )
            } else {
                KeyboardInstrumentPicker(
                    instrumentId = strip.instrumentId,
                    onSelect = onSetInstrument,
                )
            }

            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                Text("Vol", style = MaterialTheme.typography.labelSmall)
                Slider(
                    value = strip.volume,
                    onValueChange = onSetVolume,
                    valueRange = 0f..1.5f,
                    modifier = Modifier.weight(1f),
                )
                Text(
                    text = "%.1f".format(strip.volume),
                    style = MaterialTheme.typography.labelSmall,
                    modifier = Modifier.width(28.dp),
                )
            }
        }
    }
}

@Composable
private fun DrumInstrumentPicker(
    instrumentId: String,
    samplesLoaded: Boolean,
    onSelect: (String) -> Unit,
) {
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        FilterChip(
            selected = instrumentId == "noise_drum",
            onClick = { onSelect("noise_drum") },
            label = { Text("Noise") },
        )
        FilterChip(
            selected = instrumentId == "fm_drum",
            onClick = { onSelect("fm_drum") },
            label = { Text("FM") },
        )
        FilterChip(
            selected = instrumentId == "sample_drum",
            onClick = { if (samplesLoaded) onSelect("sample_drum") },
            enabled = samplesLoaded,
            label = { Text("Samples") },
        )
    }
}

@Composable
private fun KeyboardInstrumentPicker(
    instrumentId: String,
    onSelect: (String) -> Unit,
) {
    val type = when {
        instrumentId.endsWith("_polysynth") -> KeyboardType.Poly
        instrumentId.endsWith("_monosynth") -> KeyboardType.Mono
        else -> KeyboardType.Piano
    }
    val waveform = when {
        instrumentId.startsWith("saw") -> SynthWaveform.Saw
        instrumentId.startsWith("square") -> SynthWaveform.Square
        else -> SynthWaveform.Sine
    }

    fun buildId(t: KeyboardType, w: SynthWaveform): String {
        if (t == KeyboardType.Piano) return "piano"
        return "${w.name.lowercase()}_${if (t == KeyboardType.Poly) "polysynth" else "monosynth"}"
    }

    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            FilterChip(
                selected = type == KeyboardType.Piano,
                onClick = { onSelect("piano") },
                label = { Text("Piano") },
            )
            FilterChip(
                selected = type == KeyboardType.Poly,
                onClick = { onSelect(buildId(KeyboardType.Poly, waveform)) },
                label = { Text("Poly") },
            )
            FilterChip(
                selected = type == KeyboardType.Mono,
                onClick = { onSelect(buildId(KeyboardType.Mono, waveform)) },
                label = { Text("Mono") },
            )
        }
        if (type != KeyboardType.Piano) {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FilterChip(
                    selected = waveform == SynthWaveform.Sine,
                    onClick = { onSelect(buildId(type, SynthWaveform.Sine)) },
                    label = { Text("Sine") },
                )
                FilterChip(
                    selected = waveform == SynthWaveform.Saw,
                    onClick = { onSelect(buildId(type, SynthWaveform.Saw)) },
                    label = { Text("Saw") },
                )
                FilterChip(
                    selected = waveform == SynthWaveform.Square,
                    onClick = { onSelect(buildId(type, SynthWaveform.Square)) },
                    label = { Text("Square") },
                )
            }
        }
    }
}
