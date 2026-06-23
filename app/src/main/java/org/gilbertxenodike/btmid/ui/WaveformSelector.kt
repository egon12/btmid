package org.gilbertxenodike.btmid.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import org.gilbertxenodike.btmid.SynthWaveform
import org.gilbertxenodike.btmid.ui.theme.BtmidTheme

@Composable
fun WaveformSelector(
    selected: SynthWaveform,
    onSelect: (SynthWaveform) -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(modifier = modifier, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        SynthWaveform.entries.forEach { waveform ->
            FilterChip(
                selected = waveform == selected,
                onClick  = { onSelect(waveform) },
                label    = { Text(waveform.label) },
            )
        }
    }
}

private val SynthWaveform.label get() = when (this) {
    SynthWaveform.Sine   -> "Sine"
    SynthWaveform.Saw    -> "Saw"
    SynthWaveform.Square -> "Square"
}

@Preview(showBackground = true)
@Composable
private fun WaveformSelectorPreview() {
    BtmidTheme { WaveformSelector(selected = SynthWaveform.Saw, onSelect = {}) }
}
