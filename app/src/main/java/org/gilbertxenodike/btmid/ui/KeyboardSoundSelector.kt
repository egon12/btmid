package org.gilbertxenodike.btmid.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import org.gilbertxenodike.btmid.KeyboardType

@Composable
fun KeyboardSoundSelector(
    selected: KeyboardType,
    onSelect: (KeyboardType) -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(modifier = modifier, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        KeyboardType.entries.forEach { type ->
            FilterChip(
                selected = type == selected,
                onClick = { onSelect(type) },
                label = { Text(type.label) },
            )
        }
    }
}

private val KeyboardType.label get() = when (this) {
    KeyboardType.Piano -> "Piano"
    KeyboardType.Poly  -> "Poly"
    KeyboardType.Mono  -> "Mono"
}
