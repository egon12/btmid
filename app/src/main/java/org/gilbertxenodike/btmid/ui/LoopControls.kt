package org.gilbertxenodike.btmid.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import org.gilbertxenodike.btmid.LoopState
import org.gilbertxenodike.btmid.ui.theme.BtmidTheme

@Composable
fun LoopControls(
    loopState: LoopState,
    onRecord: () -> Unit,
    onStop: () -> Unit,
    onClear: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(
        modifier = modifier,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Button(
            onClick = onRecord,
            colors = ButtonDefaults.buttonColors(
                containerColor = if (loopState == LoopState.Recording)
                    MaterialTheme.colorScheme.error
                else
                    MaterialTheme.colorScheme.primary
            ),
            enabled = loopState == LoopState.Idle || loopState == LoopState.Armed,
        ) {
            Text(
                when (loopState) {
                    LoopState.Recording -> "\u25CF REC"
                    LoopState.Armed     -> "\u25CF ARMED"
                    else                -> "REC"
                }
            )
        }

        Button(
            onClick = onStop,
            enabled = loopState == LoopState.Recording || loopState == LoopState.Armed,
        ) {
            Text("STOP")
        }

        OutlinedButton(
            onClick = onClear,
            enabled = loopState == LoopState.Playing || loopState == LoopState.Idle,
        ) {
            Text("CLEAR")
        }

        val label = when (loopState) {
            LoopState.Idle      -> ""
            LoopState.Recording -> "recording\u2026"
            LoopState.Playing   -> "looping"
            LoopState.Armed     -> "armed\u2026"
        }
        if (label.isNotEmpty()) {
            Text(
                text = label,
                style = MaterialTheme.typography.labelSmall,
                modifier = Modifier.padding(start = 4.dp),
            )
        }
    }
}

@Preview(showBackground = true)
@Composable
private fun LoopControlsIdlePreview() {
    BtmidTheme { LoopControls(LoopState.Idle, {}, {}, {}) }
}

@Preview(showBackground = true)
@Composable
private fun LoopControlsRecordingPreview() {
    BtmidTheme { LoopControls(LoopState.Recording, {}, {}, {}) }
}

@Preview(showBackground = true)
@Composable
private fun LoopControlsPlayingPreview() {
    BtmidTheme { LoopControls(LoopState.Playing, {}, {}, {}) }
}

@Preview(showBackground = true)
@Composable
private fun LoopControlsArmedPreview() {
    BtmidTheme { LoopControls(LoopState.Armed, {}, {}, {}) }
}
