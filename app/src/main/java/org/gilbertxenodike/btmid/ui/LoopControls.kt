package org.gilbertxenodike.btmid.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults.buttonColors
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import org.gilbertxenodike.btmid.LoopControlAction
import org.gilbertxenodike.btmid.LoopState
import org.gilbertxenodike.btmid.TimeSignature
import org.gilbertxenodike.btmid.ui.modifier.blink
import org.gilbertxenodike.btmid.ui.theme.BtmidTheme
import org.gilbertxenodike.btmid.ui.theme.Red
import org.gilbertxenodike.btmid.ui.theme.SoftRed

private fun currentBeat(progress: Int, sig: TimeSignature): Int {
    val totalBeats = sig.beatsPerBar * sig.bars
    val beatInLoop = progress * totalBeats / 4
    return beatInLoop % sig.beatsPerBar
}

@Composable
fun LoopControls(
    loopState: LoopState,
    loopProgress: Int,
    timeSignature: TimeSignature,
    onLoopControlAction: (LoopControlAction) -> Unit,
    onTimeSignatureChanged: (TimeSignature) -> Unit,
    modifier: Modifier = Modifier,
) {
    var dialogOpen by remember { mutableStateOf(false) }

    val stateLabel = when (loopState) {
        LoopState.Idle -> ""
        LoopState.Recording -> "recording…"
        LoopState.Playing -> "playing"
        LoopState.Armed -> "armed…"
        LoopState.Overdubbing -> "overdubbing…"
    }
    val showBeats = loopState == LoopState.Playing || loopState == LoopState.Overdubbing
    val beat = if (showBeats) currentBeat(loopProgress, timeSignature) else -1

    if (dialogOpen) {
        TimeSignatureDialog(
            current = timeSignature,
            onConfirm = { sig ->
                onTimeSignatureChanged(sig)
                dialogOpen = false
            },
            onDismiss = { dialogOpen = false },
        )
    }

    Column(modifier = modifier, verticalArrangement = Arrangement.spacedBy(6.dp)) {
        // Title row
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text("Loop", style = MaterialTheme.typography.titleSmall)
            Spacer(Modifier.weight(1f))
            if (stateLabel.isNotEmpty()) {
                Text(
                    text = stateLabel,
                    style = MaterialTheme.typography.labelSmall,
                    color = when (loopState) {
                        LoopState.Recording, LoopState.Armed, LoopState.Overdubbing ->
                            MaterialTheme.colorScheme.error
                        else -> MaterialTheme.colorScheme.onSurfaceVariant
                    },
                    modifier = if (loopState == LoopState.Armed) Modifier.blink() else Modifier,
                )
                Spacer(Modifier.size(8.dp))
            }
            TextButton(
                onClick = { dialogOpen = true },
                contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 8.dp, vertical = 0.dp),
            ) {
                Text("${timeSignature.label} ▾", style = MaterialTheme.typography.labelMedium)
            }
        }

        // Buttons + beat indicators on same row
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Button(
                onClick = { onLoopControlAction(LoopControlAction.Rec) },
                colors = buttonColors(
                    containerColor = when (loopState) {
                        LoopState.Idle -> SoftRed
                        LoopState.Recording -> Red
                        LoopState.Playing -> SoftRed
                        LoopState.Armed -> Red
                        LoopState.Overdubbing -> Red
                    }
                ),
            ) {
                Text(
                    "●",
                    modifier = if (loopState == LoopState.Armed) Modifier.blink() else Modifier,
                )
            }

            Button(onClick = { onLoopControlAction(LoopControlAction.Stop) }) {
                Text("■")
            }

            OutlinedButton(
                onClick = { onLoopControlAction(LoopControlAction.Clear) },
                enabled = loopState == LoopState.Playing || loopState == LoopState.Idle,
            ) {
                Text("✕")
            }

            Spacer(Modifier.weight(1f))

            if (showBeats) {
                BeatIndicators(beats = timeSignature.beatsPerBar, currentBeat = beat)
            }
        }
    }
}

@Composable
private fun TimeSignatureDialog(
    current: TimeSignature,
    onConfirm: (TimeSignature) -> Unit,
    onDismiss: () -> Unit,
) {
    var beatsPerBar by remember { mutableIntStateOf(current.beatsPerBar) }
    var noteValue by remember { mutableIntStateOf(current.noteValue) }
    var bars by remember { mutableIntStateOf(current.bars) }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Time Signature") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Text("Beats per bar", style = MaterialTheme.typography.labelMedium)
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    listOf(2, 3, 4, 5, 6, 7).forEach { b ->
                        FilterChip(
                            selected = beatsPerBar == b,
                            onClick = { beatsPerBar = b },
                            label = { Text("$b") },
                        )
                    }
                }

                Text("Note value", style = MaterialTheme.typography.labelMedium)
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    listOf(4, 8).forEach { n ->
                        FilterChip(
                            selected = noteValue == n,
                            onClick = { noteValue = n },
                            label = { Text("1/$n") },
                        )
                    }
                }

                Text("Bars", style = MaterialTheme.typography.labelMedium)
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    listOf(1, 2, 4, 8, 16).forEach { bar ->
                        FilterChip(
                            selected = bars == bar,
                            onClick = { bars = bar },
                            label = { Text("$bar") },
                        )
                    }
                }

                Text(
                    text = TimeSignature(beatsPerBar, noteValue, bars).label,
                    style = MaterialTheme.typography.headlineSmall,
                    modifier = Modifier.padding(top = 4.dp),
                )
            }
        },
        confirmButton = {
            TextButton(onClick = { onConfirm(TimeSignature(beatsPerBar, noteValue, bars)) }) {
                Text("OK")
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        },
    )
}

@Composable
private fun BeatIndicators(beats: Int, currentBeat: Int) {
    val activeColor = MaterialTheme.colorScheme.primary
    val inactiveColor = MaterialTheme.colorScheme.outlineVariant
    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        repeat(beats) { i ->
            Box(
                modifier = Modifier
                    .size(14.dp)
                    .then(
                        if (i == currentBeat)
                            Modifier.background(activeColor, CircleShape)
                        else
                            Modifier.border(1.5.dp, inactiveColor, CircleShape)
                    )
            )
        }
    }
}

@Preview(showBackground = true)
@Composable
private fun LoopControlsIdlePreview() {
    BtmidTheme { LoopControls(LoopState.Idle, 0, TimeSignature(), {}, {}, Modifier.padding(16.dp)) }
}

@Preview(showBackground = true)
@Composable
private fun LoopControlsPlayingPreview() {
    BtmidTheme { LoopControls(LoopState.Playing, 2, TimeSignature(), {}, {}, Modifier.padding(16.dp)) }
}

@Preview(showBackground = true)
@Composable
private fun LoopControlsPlayingCustomPreview() {
    BtmidTheme {
        LoopControls(
            LoopState.Playing, 1, TimeSignature(beatsPerBar = 3, bars = 4),
            {}, {}, Modifier.padding(16.dp)
        )
    }
}
