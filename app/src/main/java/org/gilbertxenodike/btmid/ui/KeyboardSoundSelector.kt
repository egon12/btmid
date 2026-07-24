package org.gilbertxenodike.btmid.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
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

// Poly and Mono drill down into a waveform row; Piano and SoundFont don't.
private val KeyboardType.hasWaveforms
    get() = this == KeyboardType.Poly || this == KeyboardType.Mono

@Composable
fun KeyboardSoundSelector(
    selected: KeyboardType,
    waveform: SynthWaveform,
    soundFontLoaded: Boolean,
    onSelectType: (KeyboardType) -> Unit,
    onSelectWaveform: (SynthWaveform) -> Unit,
    modifier: Modifier = Modifier,
) {
    var showWaveforms by remember { mutableStateOf(selected.hasWaveforms) }

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
                val notReady = type == KeyboardType.SoundFont && !soundFontLoaded
                FilterChip(
                    selected = type == selected,
                    enabled = !notReady,
                    onClick = {
                        if (notReady) return@FilterChip
                        onSelectType(type)
                        if (type.hasWaveforms) showWaveforms = true
                    },
                    label = {
                        if (notReady) {
                            Row(
                                verticalAlignment = Alignment.CenterVertically,
                                horizontalArrangement = Arrangement.spacedBy(4.dp),
                            ) {
                                CircularProgressIndicator(
                                    modifier = Modifier.size(12.dp),
                                    strokeWidth = 2.dp,
                                    color = MaterialTheme.colorScheme.outline,
                                )
                                Spacer(Modifier.width(2.dp))
                                Text(type.typeLabel)
                            }
                        } else {
                            Text(type.typeLabel)
                        }
                    },
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
        KeyboardType.SoundFont -> "SF"
    }

private val SynthWaveform.waveformLabel
    get() = when (this) {
        SynthWaveform.Sine -> "Sine"
        SynthWaveform.Saw -> "Saw"
        SynthWaveform.Square -> "Square"
    }
