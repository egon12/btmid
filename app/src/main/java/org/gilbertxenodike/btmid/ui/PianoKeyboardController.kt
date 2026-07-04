package org.gilbertxenodike.btmid.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.material3.FilterChip
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
import androidx.compose.ui.unit.dp
import org.gilbertxenodike.btmid.synth.NativeAudioEngine

@Composable
fun PianoKeyboardController(modifier: Modifier = Modifier) {
    var octave by remember { mutableIntStateOf(4) }
    var sustained by remember { mutableStateOf(false) }

    Column(modifier = modifier, verticalArrangement = Arrangement.spacedBy(4.dp)) {
        PianoKeyboard(
            octave = octave,
            modifier = Modifier.fillMaxWidth().height(120.dp),
        )

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            FilterChip(
                selected = sustained,
                onClick = {
                    sustained = !sustained
                    NativeAudioEngine.controlChange(0, 64, if (sustained) 127 else 0)
                },
                label = { Text("Sustain") },
            )

            Row(verticalAlignment = Alignment.CenterVertically) {
                TextButton(
                    onClick = { if (octave > 0) octave-- },
                    enabled = octave > 0,
                ) { Text("◂") }
                Text("Oct $octave")
                TextButton(
                    onClick = { if (octave < 8) octave++ },
                    enabled = octave < 8,
                ) { Text("▸") }
            }
        }
    }
}
