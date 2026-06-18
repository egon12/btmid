# TODO: Add Keyboard Sound Selection

Add a `FilterChip` row (like `DrumEngineSelector`) above the piano keyboard so the user can switch between available keyboard/channel-0 sounds at runtime, with the selection persisted across app restarts.

## Available Sounds

| Enum value   | Display label | C++ string ID         | Class                          |
|--------------|---------------|-----------------------|--------------------------------|
| `Piano`      | Piano         | `"piano"`             | `Piano` (additive sine, 5 harmonics) |
| `SineTable`  | Sine Table    | `"piano_sin_table"`   | `PianoSinTable` (LUT oscillator) |
| `Sine`       | Sine          | `"sine_oscillator"`   | `PolyOscillator(Sine)`         |
| `Saw`        | Saw           | `"saw_oscillator"`    | `PolyOscillator(Saw)`          |
| `Square`     | Square        | `"square_oscillator"` | `PolyOscillator(Square)`       |

## Steps

### 1. C++ — Register `PianoSinTable` in `InstrumentRepository`

- [ ] Add `#include "instruments/PianoSinTable.h"` to `InstrumentRepository.cpp`
- [ ] In `getOrCreate()`, add case for `"piano_sin_table"`:
  ```cpp
  if (id == "piano_sin_table") {
      auto p = std::make_unique<PianoSinTable>();
      p->initSinTable();
      return mInstruments.emplace(id, std::move(p)).first->second.get();
  }
  ```

### 2. Kotlin — Add `KeyboardSound` enum

- [ ] In `MainViewModel.kt`, add:
  ```kotlin
  enum class KeyboardSound { Piano, SineTable, Sine, Saw, Square }
  ```

### 3. Kotlin — Add `keyboardSound` to `UiState`

- [ ] Add field to `UiState`:
  ```kotlin
  val keyboardSound: KeyboardSound = KeyboardSound.Piano,
  ```

### 4. Kotlin — Add `setKeyboardSound()` action in `MainViewModel`

- [ ] Add method:
  ```kotlin
  fun setKeyboardSound(sound: KeyboardSound) {
      val ids = arrayOf("piano", "piano_sin_table", "sine_oscillator", "saw_oscillator", "square_oscillator")
      NativeAudioEngine.setInstrument(0, ids[sound.ordinal])
      _uiState.value = _uiState.value.copy(keyboardSound = sound)
      viewModelScope.launch { keyboardSoundStore.save(sound) }
  }
  ```

### 5. Kotlin — Create `KeyboardSoundStore` (DataStore persistence)

- [ ] Create `app/src/main/java/org/egon12/btmid/KeyboardSoundStore.kt`, mirroring `DrumBackendStore.kt`:
  ```kotlin
  private val KEYBOARD_SOUND_KEY = stringPreferencesKey("keyboard_sound")

  class KeyboardSoundStore(private val context: Context) {
      val keyboardSound: Flow<KeyboardSound> = context.dataStore.data
          .map { prefs ->
              KeyboardSound.entries.firstOrNull { it.name == prefs[KEYBOARD_SOUND_KEY] }
                  ?: KeyboardSound.Piano
          }

      suspend fun save(sound: KeyboardSound) {
          context.dataStore.edit { prefs ->
              prefs[KEYBOARD_SOUND_KEY] = sound.name
          }
      }
  }
  ```

### 6. Kotlin — Wire up persistence in `MainViewModel`

- [ ] Add `private val keyboardSoundStore = KeyboardSoundStore(getApplication())`
- [ ] In `init`, restore saved selection:
  ```kotlin
  viewModelScope.launch {
      val saved = keyboardSoundStore.keyboardSound.first()
      setKeyboardSound(saved)
  }
  ```

### 7. UI — Create `KeyboardSoundSelector` composable

- [ ] Create `app/src/main/java/org/egon12/btmid/ui/KeyboardSoundSelector.kt`, mirroring `DrumEngineSelector.kt`:
  ```kotlin
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
      KeyboardSound.SineTable -> "Sine Table"
      KeyboardSound.Sine      -> "Sine"
      KeyboardSound.Saw       -> "Saw"
      KeyboardSound.Square    -> "Square"
  }
  ```

### 8. UI — Add `KeyboardSoundSelector` to `MainScreen`

- [ ] Add parameter to `MainScreen`:
  ```kotlin
  onSetKeyboardSound: (KeyboardSound) -> Unit,
  ```
- [ ] Place `KeyboardSoundSelector` above `PianoKeyboard`:
  ```kotlin
  KeyboardSoundSelector(
      selected = uiState.keyboardSound,
      onSelect = onSetKeyboardSound,
  )

  PianoKeyboard(...)
  ```
- [ ] Update all `@Preview` composables to pass `onSetKeyboardSound = {}`

### 9. Wire up in `MainActivity`

- [ ] Add `onSetKeyboardSound = { vm.setKeyboardSound(it) }` to the `MainScreen` call

### 10. Verify

- [ ] `./gradlew assembleDebug` — compiles
- [ ] `./gradlew test` — unit tests pass
- [ ] `./gradlew lint` — no new warnings
- [ ] Manual test: tap each chip, confirm piano sound changes on channel 0
- [ ] Manual test: kill and restart app, confirm last selection is restored
