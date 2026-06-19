package org.egon12.btmid.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import org.egon12.btmid.KeyboardSound

@Composable
fun KeyboardSoundSelector(
	selected: KeyboardSound,
	onSelect: (KeyboardSound) -> Unit,
	modifier: Modifier = Modifier,
) {
	Row(modifier = modifier, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
		KeyboardSound.entries.forEach { sound ->
			FilterChip(
				selected = sound == selected,
				onClick = { onSelect(sound) },
				label = { Text(sound.label) },
			)
		}
	}
}

private val KeyboardSound.label get() = when (this) {
	KeyboardSound.Piano     -> "Piano"
	KeyboardSound.Sine      -> "Sine"
	KeyboardSound.Saw       -> "Saw"
	KeyboardSound.Square    -> "Square"
	KeyboardSound.Mono      -> "Mono"
}
