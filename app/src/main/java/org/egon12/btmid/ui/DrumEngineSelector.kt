package org.egon12.btmid.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import org.egon12.btmid.DrumBackend
import org.egon12.btmid.ui.theme.BtmidTheme

@Composable
fun DrumEngineSelector(
    selected: DrumBackend,
    samplesLoaded: Boolean,
    onSelect: (DrumBackend) -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(modifier = modifier, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        DrumBackend.entries.forEach { backend ->
            val isSamplesNotReady = backend == DrumBackend.Samples && !samplesLoaded
            FilterChip(
                selected = backend == selected,
                onClick = { if (!isSamplesNotReady) onSelect(backend) },
                enabled = !isSamplesNotReady,
                label = {
                    if (isSamplesNotReady) {
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
                            Text("Samples")
                        }
                    } else {
                        Text(backend.label)
                    }
                },
            )
        }
    }
}

private val DrumBackend.label get() = when (this) {
    DrumBackend.Noise   -> "Noise"
    DrumBackend.Fm      -> "FM Synth"
    DrumBackend.Samples -> "Samples"
}

@Preview(showBackground = true)
@Composable
private fun DrumEngineSelectorLoadingPreview() {
    BtmidTheme {
        DrumEngineSelector(
            selected = DrumBackend.Noise,
            samplesLoaded = false,
            onSelect = {},
        )
    }
}

@Preview(showBackground = true)
@Composable
private fun DrumEngineSelectorReadyPreview() {
    BtmidTheme {
        DrumEngineSelector(
            selected = DrumBackend.Samples,
            samplesLoaded = true,
            onSelect = {},
        )
    }
}
