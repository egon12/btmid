# Keyboard Synth Refactor: Piano / Poly / Mono + Waveform Row

## Goal

- **3 keyboard types** in a top row: `[Piano]  [Poly]  [Mono]`
- **3 waveforms** in a second row (only when Poly or Mono): `[Sine]  [Saw]  [Square]`
- Combination produces 6 instrument IDs in C++:
  `sine_polysynth`, `saw_polysynth`, `square_polysynth`,
  `sine_monosynth`, `saw_monosynth`, `square_monosynth`
- **Waveform normalization** so all three waveforms sound equally loud.

---

## Waveform Normalization

Raw waveforms at peak=1 have different RMS (perceived loudness):

| Waveform | Formula | RMS |
|----------|---------|-----|
| Sine | `sin(2π·t)` | 1/√2 ≈ 0.707 |
| Bipolar Saw | `2t − 1` | 1/√3 ≈ 0.577 |
| Square | `±1` | 1.0 |

To normalize all to sine's RMS level (the natural reference), multiply the raw sample:

```cpp
// In processSample() / render loop, before multiplying by gain:
static constexpr float kNormSine   = 1.000f;          // reference — no change
static constexpr float kNormSaw    = 1.225f;           // sqrt(3/2): boost saw to match sine
static constexpr float kNormSquare = 0.707f;           // 1/sqrt(2): attenuate square to match
```

The saw boost raises its per-voice peak by ×1.225; since `gain` is already capped at
`peak × 0.7f`, the actual sample peak per voice stays ≤ 0.86 — well within range.

Apply the constant as the last step before writing to the buffer:

```cpp
// PolyOscillator::processSample
float raw = ...; // sine / saw / square formula
return raw * normFactorForWaveform(); // multiply by kNorm* constant
```

```cpp
// MonoOscillator::render loop (line 129 area)
float raw = ...; // sine / saw / square formula
float sample = raw * kNorm* * mGain;
```

---

## Checklist

### C++

- [ ] Step 1 — `MonoOscillator.h`: add `Waveform` enum, constructor param, `mWaveform` member
- [ ] Step 2 — `MonoOscillator.cpp`: add waveform branch in render loop + normalization constant
- [ ] Step 3 — `PolyOscillator.h/.cpp`: add normalization constants; apply in `processSample()`
- [ ] Step 4 — `InstrumentRepository.cpp`: add 6 new IDs; keep `piano`; remove old IDs

### Kotlin

- [ ] Step 5 — `MainViewModel.kt`: replace `KeyboardSound` enum with `KeyboardType { Piano, Poly, Mono }`
- [ ] Step 6 — `MainViewModel.kt`: add `SynthWaveform { Sine, Saw, Square }` enum
- [ ] Step 7 — `MainViewModel.kt`: update `UiState` — replace `keyboardSound` with `keyboardType` + `synthWaveform`
- [ ] Step 8 — `MainViewModel.kt`: add helper `instrumentId()`, update `setKeyboardType()`, add `setWaveform()`
- [ ] Step 9 — `KeyboardSoundStore.kt` → rename to `KeyboardTypeStore.kt`; add `WaveformStore.kt`
- [ ] Step 10 — `MainViewModel.kt`: restore both from DataStore in `init`; update `selectEngine()`

### UI

- [ ] Step 11 — `KeyboardSoundSelector.kt`: update labels + entries for Piano/Poly/Mono
- [ ] Step 12 — Create `WaveformSelector.kt` (Sine / Saw / Square chips)
- [ ] Step 13 — `MainScreen.kt`: show `WaveformSelector` row when `keyboardType != Piano`
- [ ] Step 14 — `MainScreen.kt`: add `onSetWaveform` parameter; update previews
- [ ] Step 15 — `MainActivity.kt`: wire `onSetWaveform = { viewModel.setWaveform(it) }`

---

## Step 1 — `MonoOscillator.h`: add `Waveform` enum

**File:** `app/src/main/cpp/instruments/MonoOscillator.h`

After the opening `class MonoOscillator : public Instrument {`, before `public:`, add:

```cpp
public:
    enum class Waveform { Sine, Saw, Square };

    explicit MonoOscillator(float portamentoTau = 0.15f,
                            Waveform waveform   = Waveform::Sine);
```

Replace the existing constructor declaration (currently `explicit MonoOscillator(float portamentoTau = 0.15f)`).

Add private member after `mPortamentoCoeff`:

```cpp
    float    mPortamentoCoeff{0.0f};
    Waveform mWaveform{Waveform::Sine};   // ← add
```

---

## Step 2 — `MonoOscillator.cpp`: waveform branch + normalization

**File:** `app/src/main/cpp/instruments/MonoOscillator.cpp`

### 2a. Update constructor

```cpp
// before
MonoOscillator::MonoOscillator(float portamentoTau) {
    recalcPortamentoCoeff(portamentoTau);
}

// after
MonoOscillator::MonoOscillator(float portamentoTau, Waveform waveform)
    : mWaveform(waveform) {
    recalcPortamentoCoeff(portamentoTau);
}
```

### 2b. Replace the hardcoded `sinf` line in `render()` (currently around line 129)

```cpp
// before
float sample = std::sinf(2.0f * static_cast<float>(M_PI) * mPhaseAccum);

// after
static constexpr float kNormSine   = 1.000f;
static constexpr float kNormSaw    = 1.225f;
static constexpr float kNormSquare = 0.707f;

float raw;
switch (mWaveform) {
    case Waveform::Saw:
        raw = 2.0f * mPhaseAccum - 1.0f;
        break;
    case Waveform::Square:
        raw = (mPhaseAccum < 0.5f) ? 1.0f : -1.0f;
        break;
    default: // Sine
        raw = std::sinf(2.0f * static_cast<float>(M_PI) * mPhaseAccum);
        break;
}
float norm = (mWaveform == Waveform::Saw)    ? kNormSaw
           : (mWaveform == Waveform::Square) ? kNormSquare
           : kNormSine;
float sample = raw * norm;
```

---

## Step 3 — `PolyOscillator`: add normalization in `processSample()`

**File:** `app/src/main/cpp/instruments/PolyOscillator.cpp`

`processSample()` currently returns raw waveform values. Add normalization:

```cpp
float PolyOscillator::processSample(Voice& v) const {
    v.phase += v.freq / kSampleRate;
    if (v.phase >= 1.0f) v.phase -= 1.0f;

    static constexpr float kNormSine   = 1.000f;
    static constexpr float kNormSaw    = 1.225f;
    static constexpr float kNormSquare = 0.707f;

    switch (mWaveform) {
        case Waveform::Sine:
            return std::sinf(2.0f * static_cast<float>(M_PI) * v.phase) * kNormSine;
        case Waveform::Saw:
            return (2.0f * v.phase - 1.0f) * kNormSaw;
        case Waveform::Square:
            return ((v.phase < 0.5f) ? 1.0f : -1.0f) * kNormSquare;
        default:
            return 0.0f;
    }
}
```

No other changes to `PolyOscillator` needed.

---

## Step 4 — `InstrumentRepository.cpp`: new instrument IDs

**File:** `app/src/main/cpp/InstrumentRepository.cpp`

Replace the current poly/mono blocks:

```cpp
// Remove these IDs (replaced below):
//   "sine_oscillator", "saw_oscillator", "square_oscillator", "mono_osc"

// Add:
} else if (id == "sine_polysynth") {
    inst = std::make_unique<PolyOscillator>(PolyOscillator::Waveform::Sine);
} else if (id == "saw_polysynth") {
    inst = std::make_unique<PolyOscillator>(PolyOscillator::Waveform::Saw);
} else if (id == "square_polysynth") {
    inst = std::make_unique<PolyOscillator>(PolyOscillator::Waveform::Square);
} else if (id == "sine_monosynth") {
    inst = std::make_unique<MonoOscillator>(0.15f, MonoOscillator::Waveform::Sine);
} else if (id == "saw_monosynth") {
    inst = std::make_unique<MonoOscillator>(0.15f, MonoOscillator::Waveform::Saw);
} else if (id == "square_monosynth") {
    inst = std::make_unique<MonoOscillator>(0.15f, MonoOscillator::Waveform::Square);
}
// Keep "piano" unchanged.
```

---

## Step 5 — `MainViewModel.kt`: replace `KeyboardSound` with `KeyboardType`

**File:** `app/src/main/java/org/gilbertxenodike/btmid/MainViewModel.kt`

```kotlin
// Remove:
enum class KeyboardSound { Piano, Sine, Saw, Square, Mono }

// Add:
enum class KeyboardType { Piano, Poly, Mono }
```

---

## Step 6 — `MainViewModel.kt`: add `SynthWaveform` enum

```kotlin
enum class SynthWaveform { Sine, Saw, Square }
```

---

## Step 7 — `MainViewModel.kt`: update `UiState`

```kotlin
data class UiState(
    // ... existing fields ...
    val keyboardType: KeyboardType = KeyboardType.Piano,   // replaces keyboardSound
    val synthWaveform: SynthWaveform = SynthWaveform.Sine, // new
    // loopState, loopLengthSec, etc. unchanged
)
```

Remove `val keyboardSound: KeyboardSound` entirely.

---

## Step 8 — `MainViewModel.kt`: instrument ID helper + actions

### 8a. Private helper

```kotlin
private fun instrumentId(type: KeyboardType, waveform: SynthWaveform): String {
    if (type == KeyboardType.Piano) return "piano"
    val wavePart = waveform.name.lowercase()         // "sine" / "saw" / "square"
    val typePart = if (type == KeyboardType.Poly) "polysynth" else "monosynth"
    return "${wavePart}_${typePart}"                 // e.g. "saw_monosynth"
}
```

### 8b. Replace `setKeyboardSound()` with two separate functions

```kotlin
fun setKeyboardType(type: KeyboardType) {
    val id = instrumentId(type, _uiState.value.synthWaveform)
    NativeAudioEngine.setInstrument(0, id)
    _uiState.value = _uiState.value.copy(keyboardType = type)
    viewModelScope.launch { keyboardTypeStore.save(type) }
}

fun setWaveform(waveform: SynthWaveform) {
    val id = instrumentId(_uiState.value.keyboardType, waveform)
    NativeAudioEngine.setInstrument(0, id)
    _uiState.value = _uiState.value.copy(synthWaveform = waveform)
    viewModelScope.launch { waveformStore.save(waveform) }
}
```

### 8c. Update `selectEngine()` to use the helper

```kotlin
fun selectEngine(engine: AudioEngine) {
    val current = _uiState.value
    val keyId = instrumentId(current.keyboardType, current.synthWaveform)
    NativeAudioEngine.setEngine(engine)
    NativeAudioEngine.setInstrument(0, keyId)
    // ... rest unchanged
}
```

---

## Step 9 — DataStore: rename `KeyboardSoundStore` → `KeyboardTypeStore`; add `WaveformStore`

**Rename file:** `KeyboardSoundStore.kt` → `KeyboardTypeStore.kt`

Update internals:
```kotlin
private val KEY = stringPreferencesKey("keyboard_type")   // was "keyboard_sound"

class KeyboardTypeStore(private val context: Context) {
    val keyboardType: Flow<KeyboardType> = context.dataStore.data.map { prefs ->
        KeyboardType.entries.firstOrNull { it.name == prefs[KEY] } ?: KeyboardType.Piano
    }
    suspend fun save(type: KeyboardType) {
        context.dataStore.edit { it[KEY] = type.name }
    }
}
```

**New file:** `WaveformStore.kt` — same pattern:

```kotlin
private val KEY = stringPreferencesKey("synth_waveform")

class WaveformStore(private val context: Context) {
    val waveform: Flow<SynthWaveform> = context.dataStore.data.map { prefs ->
        SynthWaveform.entries.firstOrNull { it.name == prefs[KEY] } ?: SynthWaveform.Sine
    }
    suspend fun save(waveform: SynthWaveform) {
        context.dataStore.edit { it[KEY] = waveform.name }
    }
}
```

---

## Step 10 — `MainViewModel.kt`: restore from DataStore in `init`

```kotlin
private val keyboardTypeStore = KeyboardTypeStore(application)
private val waveformStore     = WaveformStore(application)

init {
    // ... existing drum + sample init ...
    viewModelScope.launch {
        val savedType = keyboardTypeStore.keyboardType.first()
        val savedWave = waveformStore.waveform.first()
        // Apply both before emitting any state update
        val id = instrumentId(savedType, savedWave)
        NativeAudioEngine.setInstrument(0, id)
        _uiState.value = _uiState.value.copy(keyboardType = savedType, synthWaveform = savedWave)
    }
}
```

---

## Step 11 — `KeyboardSoundSelector.kt`: update for Piano / Poly / Mono

**File:** `app/src/main/java/org/gilbertxenodike/btmid/ui/KeyboardSoundSelector.kt`

Change the import and enum reference from `KeyboardSound` → `KeyboardType`. Update labels:

```kotlin
private val KeyboardType.label get() = when (this) {
    KeyboardType.Piano -> "Piano"
    KeyboardType.Poly  -> "Poly"
    KeyboardType.Mono  -> "Mono"
}
```

Rename the composable parameter type and the `@Preview` accordingly.

---

## Step 12 — Create `WaveformSelector.kt`

**New file:** `app/src/main/java/org/gilbertxenodike/btmid/ui/WaveformSelector.kt`

```kotlin
package org.gilbertxenodike.btmid.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import org.gilbertxenodike.btmid.SynthWaveform
import org.gilbertxenodike.btmid.ui.theme.BtmidTheme

@Composable
fun WaveformSelector(
    selected: SynthWaveform,
    onSelect: (SynthWaveform) -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(modifier = modifier, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        SynthWaveform.entries.forEach { waveform ->
            FilterChip(
                selected = waveform == selected,
                onClick  = { onSelect(waveform) },
                label    = { Text(waveform.label) },
            )
        }
    }
}

private val SynthWaveform.label get() = when (this) {
    SynthWaveform.Sine   -> "Sine"
    SynthWaveform.Saw    -> "Saw"
    SynthWaveform.Square -> "Square"
}

@Preview(showBackground = true)
@Composable
private fun WaveformSelectorPreview() {
    BtmidTheme { WaveformSelector(selected = SynthWaveform.Saw, onSelect = {}) }
}
```

---

## Step 13 — `MainScreen.kt`: show `WaveformSelector` below keyboard type row

**File:** `app/src/main/java/org/gilbertxenodike/btmid/ui/MainScreen.kt`

Find the `KeyboardSoundSelector` call and replace the block:

```kotlin
// before
KeyboardSoundSelector(
    selected = uiState.keyboardSound,
    onSelect = onSetKeyboardSound,
)

// after
KeyboardSoundSelector(          // now shows Piano / Poly / Mono
    selected = uiState.keyboardType,
    onSelect = onSetKeyboardType,
)
if (uiState.keyboardType != KeyboardType.Piano) {
    WaveformSelector(
        selected = uiState.synthWaveform,
        onSelect = onSetWaveform,
    )
}
```

---

## Step 14 — `MainScreen.kt`: update parameters + previews

Replace the `onSetKeyboardSound: (KeyboardSound)` parameter with:

```kotlin
onSetKeyboardType: (KeyboardType) -> Unit,
onSetWaveform:     (SynthWaveform) -> Unit,
```

Update every `@Preview` call to pass:
```kotlin
onSetKeyboardType = {},
onSetWaveform     = {},
```

And update the `UiState` inside each preview to use `keyboardType` / `synthWaveform` instead of `keyboardSound`.

---

## Step 15 — `MainActivity.kt`: wire new callbacks

In the `MainScreen(...)` call inside `setContent { ... }`, replace:

```kotlin
// Remove:
onSetKeyboardSound = { viewModel.setKeyboardSound(it) },

// Add:
onSetKeyboardType = { viewModel.setKeyboardType(it) },
onSetWaveform     = { viewModel.setWaveform(it)     },
```

---

## Test Plan

1. `./gradlew assembleDebug` — clean build
2. Open app → confirm top row shows `[Piano] [Poly] [Mono]`; no second row visible
3. Tap `[Poly]` → confirm `[Sine] [Saw] [Square]` row appears
4. Play keyboard → confirm polyphonic Sine sound
5. Tap `[Saw]` → confirm buzzier timbre at **same perceived volume** as Sine
6. Tap `[Square]` → confirm hollow timbre at **same perceived volume** (not louder)
7. Tap `[Mono]` → confirm second row stays; sound switches to monophonic with portamento glide
8. Try each waveform with Mono → confirm Saw and Square glide correctly
9. Tap `[Piano]` → confirm second row disappears; piano sounds
10. Force-close and reopen → confirm keyboard type and waveform are restored
11. Switch engine (Oboe ↔ Wifi) with `Mono + Saw` active → confirm correct instrument after swap
