# Plan: Touchable Piano Keyboard UI (One Octave, Multi-Touch)

## Context

The app currently receives MIDI from a BLE device. The user wants a virtual piano keyboard on-screen that can send noteOn/noteOff events directly to the NativeAudioEngine — enabling the app to make sound without any physical MIDI device connected. The keyboard must handle multiple simultaneous touches so chords are playable.

## Key Findings

- **NativeAudioEngine** (singleton at `synth/NativeAudioEngine.kt:18`) exposes `noteOn(channel, note, velocity)` and `noteOff(channel, note)` — callable from any thread, no locking needed.
- **Piano = channel 0** (as documented in CLAUDE.md and confirmed in MidiRouter routing logic).
- **UI pattern**: All composables take state + lambda callbacks; `MainScreen.kt` hosts all sections in a scrollable `Column`.
- **No existing touch/gesture handling** beyond standard `onClick` — this is the first use of `pointerInput`.
- The NativeAudioEngine is started in `MainViewModel.init` unconditionally, so it's ready even before a BLE device connects.

## Approach

### Visual Layout

Draw one octave C4–B4 (MIDI notes 60–71) using Compose `Canvas`:
- 7 white keys across the full width, equal width
- 5 black keys overlaid at 60% white-key width, 62% total height
- Pressed keys render in a highlight color (Material primaryContainer for white, primary for black)

```
  [C#][D#]   [F#][G#][A#]
[C ][D ][E ][F ][G ][A ][B ]
```

Key MIDI notes:
| Key | Note | Position |
|-----|------|----------|
| C   | 60   | white 0  |
| C#  | 61   | black, centered at x = 1 × whiteW |
| D   | 62   | white 1  |
| D#  | 63   | black, centered at x = 2 × whiteW |
| E   | 64   | white 2  |
| F   | 65   | white 3  |
| F#  | 66   | black, centered at x = 4 × whiteW |
| G   | 67   | white 4  |
| G#  | 68   | black, centered at x = 5 × whiteW |
| A   | 69   | white 5  |
| A#  | 70   | black, centered at x = 6 × whiteW |
| B   | 71   | white 6  |

### Multi-Touch Handling

Use a single `Modifier.pointerInput(Unit)` with `awaitPointerEventScope` on the whole keyboard Canvas. Track a `mutableStateMapOf<PointerId, Int>()` (pointer → active note):

```
PointerEventType.Press   → hitTest(pos) → noteOn(0, note, 100); add to map
PointerEventType.Release → map[id]?.let { noteOff(0, it) }; remove from map
PointerEventType.Move    → re-hitTest; if note changed, noteOff(old) + noteOn(new)
```

Hit-test order: check black keys first (they sit on top), then white keys.

### Direct NativeAudioEngine Calls

No ViewModel changes needed — call `NativeAudioEngine.noteOn/Off` directly from the composable's `pointerInput` lambda. This keeps latency minimal (no coroutine dispatch, no state update cycle). The `MidiActivityIndicator` and event log will not reflect these taps (they only show BLE events), which is acceptable for now.

## Files to Create / Modify

### 1. New: `app/src/main/java/org/egon12/btmid/ui/PianoKeyboard.kt`

```kotlin
@Composable
fun PianoKeyboard(modifier: Modifier = Modifier)
```

Responsibilities:
- `remember { mutableStateMapOf<PointerId, Int>() }` for pressed-key visual state
- `Canvas(modifier.pointerInput(Unit) { ... })` — draw + handle touch in one composable
- `drawPianoKeys(scope, pressedNotes)` — internal draw helper
  - Draw 7 white key rects, outline borders, fill highlight if in pressedNotes
  - Draw 5 black key rects on top, fill highlight if pressed
- `hitTest(offset, size): Int` — returns MIDI note or -1 if no key hit
  - Compute `whiteKeyWidth = size.width / 7f`
  - Check black key rects first (narrower, higher priority)
  - Fallback to white key index
- `awaitPointerEventScope` loop:
  - Press: `change.pressed && !change.previousPressed` → `hitTest` → `noteOn`, add to map
  - Release: `!change.pressed && change.previousPressed` → noteOff old note, remove from map
  - Move: `change.pressed && change.position != change.previousPosition` → re-hitTest; swap note if changed
- Include `@Preview` with a static keyboard

### 2. Modify: `app/src/main/java/org/egon12/btmid/ui/MainScreen.kt`

Add `PianoKeyboard` inside the `permissionsGranted` branch, after `DrumEngineSelector` and before the discovered devices section:

```kotlin
DrumEngineSelector(...)

PianoKeyboard(
    modifier = Modifier
        .fillMaxWidth()
        .height(120.dp)
)

if (uiState.discoveredDevices.isNotEmpty()) { ... }
```

No new parameters or callbacks needed in `MainScreen` or `MainActivity`.

## Verification

1. Build: `./gradlew assembleDebug` — confirm no compile errors
2. Deploy to device/emulator
3. Open app, grant permissions
4. Without connecting any BLE device: tap individual white and black keys → audio should play
5. Touch two keys simultaneously → both notes play (polyphonic)
6. Slide finger from one key to another → seamless legato (old note off, new note on)
7. Hold 3+ fingers → all notes sustain until each finger lifts
8. Confirm no audio glitches or ANR from the UI thread
