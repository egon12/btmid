package org.gilbertxenodike.btmid.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.waitForUpOrCancellation
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import org.gilbertxenodike.btmid.synth.NativeAudioEngine
import org.gilbertxenodike.btmid.ui.theme.BtmidTheme

private data class DrumPadDef(val note: Int, val label: String)

private val DRUM_PAD_ROWS = listOf(
    listOf(
        DrumPadDef(42, "Closed Hat"),
        DrumPadDef(46, "Open Hat"),
        DrumPadDef(51, "Ride"),
        DrumPadDef(49, "Crash"),
    ),
    listOf(
        DrumPadDef(36, "Kick"),
        DrumPadDef(38, "Snare"),
        DrumPadDef(45, "Lo Tom"),
        DrumPadDef(50, "Hi Tom"),
    ),
)

@Composable
fun DrumTrigger(modifier: Modifier = Modifier) {
    Column(
        modifier = modifier,
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        for (row in DRUM_PAD_ROWS) {
            Row(
                modifier = Modifier.weight(1f).fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(6.dp),
            ) {
                for (pad in row) {
                    DrumPad(
                        note = pad.note,
                        label = pad.label,
                        modifier = Modifier.weight(1f).fillMaxSize(),
                    )
                }
            }
        }
    }
}

@Composable
private fun DrumPad(note: Int, label: String, modifier: Modifier = Modifier) {
    var pressed by remember { mutableStateOf(false) }
    val bgColor = if (pressed) MaterialTheme.colorScheme.primary
                  else MaterialTheme.colorScheme.surfaceVariant
    val textColor = if (pressed) MaterialTheme.colorScheme.onPrimary
                    else MaterialTheme.colorScheme.onSurfaceVariant

    Box(
        contentAlignment = Alignment.Center,
        modifier = modifier
            .clip(RoundedCornerShape(8.dp))
            .background(bgColor)
            .pointerInput(note) {
                awaitEachGesture {
                    awaitFirstDown(requireUnconsumed = false)
                    pressed = true
                    NativeAudioEngine.noteOn(9, note, 100)
                    waitForUpOrCancellation()
                    pressed = false
                    NativeAudioEngine.noteOff(9, note)
                }
            }
    ) {
        Text(
            text = label,
            style = MaterialTheme.typography.labelSmall,
            color = textColor,
            textAlign = TextAlign.Center,
            modifier = Modifier.padding(4.dp),
        )
    }
}

@Preview(showBackground = true)
@Composable
private fun DrumTriggerPreview() {
    BtmidTheme {
        DrumTrigger(
            modifier = Modifier
                .fillMaxWidth()
                .height(160.dp)
                .padding(8.dp)
        )
    }
}
