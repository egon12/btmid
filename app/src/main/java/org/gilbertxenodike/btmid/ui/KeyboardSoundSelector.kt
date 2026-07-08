package org.gilbertxenodike.btmid.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import org.gilbertxenodike.btmid.KeyboardType
import org.gilbertxenodike.btmid.SynthWaveform

@Composable
fun KeyboardSoundSelector(
    selected: KeyboardType,
    waveform: SynthWaveform,
    onSelectType: (KeyboardType) -> Unit,
    onSelectWaveform: (SynthWaveform) -> Unit,
    modifier: Modifier = Modifier,
) {
    var showWaveforms by remember { mutableStateOf(selected != KeyboardType.Piano) }

    Row(
        modifier = modifier,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (showWaveforms) {
            TextButton(onClick = { showWaveforms = false }) { Text("←") }
            SynthWaveform.entries.forEach { w ->
                FilterChip(
                    selected = w == waveform,
                    onClick = { onSelectWaveform(w) },
                    label = { Text(w.waveformLabel) },
                )
            }
        } else {
            KeyboardType.entries.forEach { type ->
                FilterChip(
                    selected = type == selected,
                    onClick = {
                        onSelectType(type)
                        if (type != KeyboardType.Piano) showWaveforms = true
                    },
                    label = { Text(type.typeLabel) },
                )
            }
        }
    }
}

private val KeyboardType.typeLabel
    get() = when (this) {
        KeyboardType.Piano -> "Piano"
        KeyboardType.Poly -> "Poly"
        KeyboardType.Mono -> "Mono"
    }

private val SynthWaveform.waveformLabel
    get() = when (this) {
        SynthWaveform.Sine -> "Sine"
        SynthWaveform.Saw -> "Saw"
        SynthWaveform.Square -> "Square"
    }
