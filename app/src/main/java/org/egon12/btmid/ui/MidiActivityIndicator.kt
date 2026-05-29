package org.egon12.btmid.ui

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import org.egon12.btmid.ui.theme.BtmidTheme

@Composable
fun MidiActivityIndicator(pulse: Boolean, modifier: Modifier = Modifier) {
    val alpha = remember { Animatable(0f) }
    LaunchedEffect(pulse) {
        alpha.snapTo(1f)
        alpha.animateTo(0f, animationSpec = tween(durationMillis = 400))
    }
    Box(
        modifier = modifier
            .size(10.dp)
            .background(
                color = MaterialTheme.colorScheme.primary.copy(alpha = alpha.value),
                shape = CircleShape,
            )
    )
}

@Preview(showBackground = true)
@Composable
private fun MidiActivityIndicatorPreview() {
    BtmidTheme {
        MidiActivityIndicator(pulse = true)
    }
}
