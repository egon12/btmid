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
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults.buttonColors
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import org.gilbertxenodike.btmid.LoopControlAction
import org.gilbertxenodike.btmid.LoopState
import org.gilbertxenodike.btmid.ui.modifier.blink
import org.gilbertxenodike.btmid.ui.theme.BtmidTheme
import org.gilbertxenodike.btmid.ui.theme.Red
import org.gilbertxenodike.btmid.ui.theme.SoftRed

private val TIME_SIGNATURES = listOf(2, 3, 4)

private fun currentBeat(progress: Int, timeSignature: Int): Int = when (timeSignature) {
    2 -> if (progress <= 1) 0 else 1
    3 -> if (progress == 0) 0 else if (progress == 1) 1 else 2
    else -> progress.coerceIn(0, 3)
}

@Composable
fun LoopControls(
    loopState: LoopState,
    loopProgress: Int,
    timeSignature: Int,
    onLoopControlAction: (LoopControlAction) -> Unit,
    onTimeSignatureChanged: (Int) -> Unit,
    modifier: Modifier = Modifier,
) {
    val stateLabel = when (loopState) {
        LoopState.Idle -> ""
        LoopState.Recording -> "recording…"
        LoopState.Playing -> "playing"
        LoopState.Armed -> "armed…"
        LoopState.Overdubbing -> "overdubbing…"
    }
    val showBeats = loopState == LoopState.Playing || loopState == LoopState.Overdubbing
    val beat = if (showBeats) currentBeat(loopProgress, timeSignature) else -1

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

            // Beat indicators
            if (showBeats) {
                BeatIndicators(beats = timeSignature, currentBeat = beat)
            }
        }

        // Time signature selector
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            TIME_SIGNATURES.forEach { sig ->
                FilterChip(
                    selected = timeSignature == sig,
                    onClick = { onTimeSignatureChanged(sig) },
                    label = { Text("${sig}/4") },
                )
            }
        }
    }
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
    BtmidTheme { LoopControls(LoopState.Idle, 0, 4, {}, {}, Modifier.padding(16.dp)) }
}

@Preview(showBackground = true)
@Composable
private fun LoopControlsRecordingPreview() {
    BtmidTheme { LoopControls(LoopState.Recording, 0, 4, {}, {}, Modifier.padding(16.dp)) }
}

@Preview(showBackground = true)
@Composable
private fun LoopControlsPlayingPreview() {
    BtmidTheme { LoopControls(LoopState.Playing, 2, 4, {}, {}, Modifier.padding(16.dp)) }
}

@Preview(showBackground = true)
@Composable
private fun LoopControlsArmedPreview() {
    BtmidTheme { LoopControls(LoopState.Armed, 0, 4, {}, {}, Modifier.padding(16.dp)) }
}

@Preview(showBackground = true)
@Composable
private fun LoopControlsPlaying34Preview() {
    BtmidTheme { LoopControls(LoopState.Playing, 1, 3, {}, {}, Modifier.padding(16.dp)) }
}
