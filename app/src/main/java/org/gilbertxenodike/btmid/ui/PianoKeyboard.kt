package org.gilbertxenodike.btmid.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.PointerId
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import org.gilbertxenodike.btmid.synth.NativeAudioEngine
import org.gilbertxenodike.btmid.ui.theme.BtmidTheme

private val WHITE_NOTES = intArrayOf(60, 62, 64, 65, 67, 69, 71)

private data class BlackKeyDef(val note: Int, val centerIndex: Int)

private val BLACK_KEY_DEFS = listOf(
    BlackKeyDef(61, 1),
    BlackKeyDef(63, 2),
    BlackKeyDef(66, 4),
    BlackKeyDef(68, 5),
    BlackKeyDef(70, 6),
)

@Composable
fun PianoKeyboard(octave: Int = 4, modifier: Modifier = Modifier) {
    val noteOffset = (octave - 4) * 12
    val pressedNotes = remember { mutableStateMapOf<PointerId, Int>() }
    val activeNoteSet: Set<Int> = pressedNotes.values.toSet()
    val whitePressedColor = MaterialTheme.colorScheme.primaryContainer
    val blackPressedColor = MaterialTheme.colorScheme.primary

    LaunchedEffect(octave) {
        pressedNotes.forEach { (_, note) -> NativeAudioEngine.noteOff(0, note) }
        pressedNotes.clear()
    }

    Canvas(
        modifier = modifier.pointerInput(noteOffset) {
            awaitPointerEventScope {
                while (true) {
                    val event = awaitPointerEvent()
                    val w = size.width.toFloat()
                    val h = size.height.toFloat()
                    event.changes.forEach { change ->
                        val id = change.id
                        when {
                            change.pressed && !change.previousPressed -> {
                                val raw = hitTest(change.position, w, h)
                                if (raw >= 0) {
                                    val note = raw + noteOffset
                                    pressedNotes[id] = note
                                    NativeAudioEngine.noteOn(0, note, 100)
                                }
                                change.consume()
                            }
                            !change.pressed && change.previousPressed -> {
                                pressedNotes.remove(id)?.let { note ->
                                    NativeAudioEngine.noteOff(0, note)
                                }
                                change.consume()
                            }
                            change.pressed -> {
                                val raw = hitTest(change.position, w, h)
                                val newNote = if (raw >= 0) raw + noteOffset else -1
                                val oldNote = pressedNotes[id]
                                if (newNote != oldNote) {
                                    if (oldNote != null) NativeAudioEngine.noteOff(0, oldNote)
                                    if (newNote >= 0) {
                                        pressedNotes[id] = newNote
                                        NativeAudioEngine.noteOn(0, newNote, 100)
                                    } else {
                                        pressedNotes.remove(id)
                                    }
                                }
                                change.consume()
                            }
                        }
                    }
                }
            }
        }
    ) {
        drawPianoKeys(activeNoteSet, noteOffset, whitePressedColor, blackPressedColor)
    }
}

private fun hitTest(pos: Offset, w: Float, h: Float): Int {
    val whiteW = w / 7f
    val blackW = whiteW * 0.6f
    val blackH = h * 0.62f

    for (def in BLACK_KEY_DEFS) {
        val cx = def.centerIndex * whiteW
        val left = cx - blackW / 2f
        val right = cx + blackW / 2f
        if (pos.x in left..right && pos.y <= blackH) return def.note
    }

    val whiteIndex = (pos.x / whiteW).toInt().coerceIn(0, 6)
    return WHITE_NOTES[whiteIndex]
}

private fun DrawScope.drawPianoKeys(
    pressedNotes: Set<Int>,
    noteOffset: Int,
    whitePressedColor: Color,
    blackPressedColor: Color,
) {
    val whiteW = size.width / 7f
    val blackW = whiteW * 0.6f
    val blackH = size.height * 0.62f

    WHITE_NOTES.forEachIndexed { index, note ->
        val x = index * whiteW
        drawRect(
            color = if ((note + noteOffset) in pressedNotes) whitePressedColor else Color.White,
            topLeft = Offset(x + 1f, 1f),
            size = Size(whiteW - 2f, size.height - 2f),
        )
        drawRect(
            color = Color.Black,
            topLeft = Offset(x, 0f),
            size = Size(whiteW, size.height),
            style = Stroke(width = 1f),
        )
    }

    for (def in BLACK_KEY_DEFS) {
        val cx = def.centerIndex * whiteW
        val left = cx - blackW / 2f
        drawRect(
            color = if ((def.note + noteOffset) in pressedNotes) blackPressedColor else Color.Black,
            topLeft = Offset(left, 0f),
            size = Size(blackW, blackH),
        )
    }
}

@Preview(showBackground = true)
@Composable
private fun PianoKeyboardPreview() {
    BtmidTheme {
        PianoKeyboard(
            modifier = Modifier
                .fillMaxWidth()
                .height(120.dp)
        )
    }
}
