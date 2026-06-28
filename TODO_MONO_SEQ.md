# Keyboard Synth Refactor: Piano / Poly / Mono + Waveform Row

## Goal

- **3 keyboard types** in a top row: `[Piano]  [Poly]  [Mono]`
- **3 waveforms** in a second row (only when Poly or Mono): `[Sine]  [Saw]  [Square]`
- Combination produces 6 instrument IDs in C++:
  `sine_polysynth`, `saw_polysynth`, `square_polysynth`,
  `sine_monosynth`, `saw_monosynth`, `square_monosynth`
- **Waveform normalization** so all three waveforms sound equally loud.

---

## Current State (what's already done)

- `PolyOscillator.h` already has the `Waveform` enum and waveform switch in `processSample()` — only normalization factors are missing (Step 3 is a small addition).
- Everything else is still the old shape: `KeyboardSound { Piano, Sine, Saw, Square, Mono }`, old IDs `"sine_oscillator"` / `"saw_oscillator"` / `"square_oscillator"` / `"mono_osc"`.

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
static constexpr float kNormSine   = 1.000f;   // reference — no change
static constexpr float kNormSaw    = 1.225f;   // sqrt(3/2): boost saw to match sine
static constexpr float kNormSquare = 0.707f;   // 1/sqrt(2): attenuate square to match
```

---

## Checklist

### C++

- [ ] Step 1 — `MonoOscillator.h`: add `Waveform` enum, update constructor signature, add `mWaveform` member
- [ ] Step 2 — `MonoOscillator.cpp`: update constructor + replace `sinf` line with waveform switch + normalization
- [ ] Step 3 — `PolyOscillator.cpp`: add normalization factors to existing `processSample()` switch (small change)
- [ ] Step 4 — `InstrumentRepository.cpp`: add 6 new IDs; remove 4 old IDs

### Kotlin

- [ ] Step 5 — `MainViewModel.kt`: replace `KeyboardSound` enum with `KeyboardType { Piano, Poly, Mono }`
- [ ] Step 6 — `MainViewModel.kt`: add `SynthWaveform { Sine, Saw, Square }` enum
- [ ] Step 7 — `MainViewModel.kt`: update `UiState` — replace `keyboardSound` with `keyboardType` + `synthWaveform`
- [ ] Step 8 — `MainViewModel.kt`: add `instrumentId()` helper; replace `setKeyboardSound()` with `setKeyboardType()` + `setWaveform()`; update `selectEngine()`
- [ ] Step 9 — Rename `KeyboardSoundStore.kt` → `KeyboardTypeStore.kt`; add `WaveformStore.kt`
- [ ] Step 10 — `MainViewModel.kt`: update store fields and `init` block

### UI

- [ ] Step 11 — `KeyboardSoundSelector.kt`: update imports + labels + parameter type for Piano/Poly/Mono
- [ ] Step 12 — Create `WaveformSelector.kt`
- [ ] Step 13 — `MainScreen.kt`: replace `KeyboardSoundSelector` call and add conditional `WaveformSelector`
- [ ] Step 14 — `MainScreen.kt`: update function signature, imports, and all three `@Preview` functions
- [ ] Step 15 — `MainActivity.kt`: replace `onSetKeyboardSound` with `onSetKeyboardType` + `onSetWaveform`

---

## Step 1 — `MonoOscillator.h`

**File:** `app/src/main/cpp/instruments/MonoOscillator.h`

### 1a. Add `Waveform` enum and update constructor (lines 9–11)

```cpp
// before
class MonoOscillator : public Instrument {
public:
    explicit MonoOscillator(float portamentoTau = 0.15f);

// after
class MonoOscillator : public Instrument {
public:
    enum class Waveform { Sine, Saw, Square };

    explicit MonoOscillator(float portamentoTau = 0.15f,
                            Waveform waveform   = Waveform::Sine);
```

### 1b. Add `mWaveform` member (after `mPortamentoCoeff` on line 46)

```cpp
// before
    float    mPortamentoCoeff{0.0f};

// after
    float    mPortamentoCoeff{0.0f};
    Waveform mWaveform{Waveform::Sine};
```

---

## Step 2 — `MonoOscillator.cpp`

**File:** `app/src/main/cpp/instruments/MonoOscillator.cpp`

### 2a. Update constructor (lines 6–8)

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

### 2b. Replace the `sinf` line in `render()` (lines 129–130)

```cpp
// before
        float sample = std::sinf(2.0f * static_cast<float>(M_PI) * mPhaseAccum);
        buffer[i] += mGain * sample;

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
        buffer[i] += mGain * raw * norm;
```

Note: move the `static constexpr` declarations **before** the `for` loop (line 88) in the final code to avoid re-declaring them on every iteration — put them at the top of `render()`, right after draining the queues:

```cpp
void MonoOscillator::render(float* buffer, int32_t frames) {
    // ... queue drain (lines 72–84) unchanged ...

    if (mPhase == EnvPhase::Idle) return;

    static constexpr float kNormSine   = 1.000f;
    static constexpr float kNormSaw    = 1.225f;
    static constexpr float kNormSquare = 0.707f;
    float norm = (mWaveform == Waveform::Saw)    ? kNormSaw
               : (mWaveform == Waveform::Square) ? kNormSquare
               : kNormSine;

    for (int32_t i = 0; i < frames; ++i) {
        // ... ADSR unchanged (lines 90–122) ...

        mPhaseAccum += mCurrentFreq / kSampleRate;
        if (mPhaseAccum >= 1.0f) mPhaseAccum -= 1.0f;

        mCurrentFreq = mTargetFreq + (mCurrentFreq - mTargetFreq) * mPortamentoCoeff;

        float raw;
        switch (mWaveform) {
            case Waveform::Saw:
                raw = 2.0f * mPhaseAccum - 1.0f;
                break;
            case Waveform::Square:
                raw = (mPhaseAccum < 0.5f) ? 1.0f : -1.0f;
                break;
            default:
                raw = std::sinf(2.0f * static_cast<float>(M_PI) * mPhaseAccum);
                break;
        }
        buffer[i] += mGain * raw * norm;
    }
}
```

---

## Step 3 — `PolyOscillator.cpp`: add normalization to `processSample()`

**File:** `app/src/main/cpp/instruments/PolyOscillator.cpp`

The `Waveform` enum and switch already exist (lines 79–92). Only normalization factors need adding:

```cpp
// before (lines 79–92)
float PolyOscillator::processSample(Voice& v) const {
    v.phase += v.freq / kSampleRate;
    if (v.phase >= 1.0f) v.phase -= 1.0f;

    switch (mWaveform) {
        case Waveform::Sine:
            return std::sinf(2.0f * static_cast<float>(M_PI) * v.phase);
        case Waveform::Saw:
            return 2.0f * v.phase - 1.0f;
        case Waveform::Square:
            return (v.phase < 0.5f) ? 1.0f : -1.0f;
    }
    return 0.0f;
}

// after
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
    }
    return 0.0f;
}
```

No other changes to `PolyOscillator.h` or `.cpp` needed.

---

## Step 4 — `InstrumentRepository.cpp`: replace old IDs with new ones

**File:** `app/src/main/cpp/InstrumentRepository.cpp`

Replace the four old `else if` blocks (lines 27–34):

```cpp
// before
    } else if (id == "sine_oscillator") {
        inst = std::make_unique<PolyOscillator>(PolyOscillator::Waveform::Sine);
    } else if (id == "saw_oscillator") {
        inst = std::make_unique<PolyOscillator>(PolyOscillator::Waveform::Saw);
    } else if (id == "square_oscillator") {
        inst = std::make_unique<PolyOscillator>(PolyOscillator::Waveform::Square);
    } else if (id == "mono_osc") {
        inst = std::make_unique<MonoOscillator>(0.15f);
    }

// after
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
```

Keep `"piano"`, `"noise_drum"`, `"fm_drum"`, `"sample_drum"` unchanged.

---

## Step 5 — `MainViewModel.kt`: replace `KeyboardSound` enum

**File:** `app/src/main/java/org/gilbertxenodike/btmid/MainViewModel.kt`

Line 29:

```kotlin
// before
enum class KeyboardSound { Piano, Sine, Saw, Square, Mono }

// after
enum class KeyboardType { Piano, Poly, Mono }
```

---

## Step 6 — `MainViewModel.kt`: add `SynthWaveform` enum

Insert immediately after `KeyboardType` (after line 29):

```kotlin
enum class SynthWaveform { Sine, Saw, Square }
```

---

## Step 7 — `MainViewModel.kt`: update `UiState`

Line 51:

```kotlin
// before
    val keyboardSound: KeyboardSound = KeyboardSound.Piano,

// after
    val keyboardType: KeyboardType = KeyboardType.Piano,
    val synthWaveform: SynthWaveform = SynthWaveform.Sine,
```

`loopState`, `loopLengthSec`, and all other fields are unchanged.

---

## Step 8 — `MainViewModel.kt`: actions + helper

### 8a. Add private `instrumentId()` helper (before `setDrumBackend`)

```kotlin
private fun instrumentId(type: KeyboardType, waveform: SynthWaveform): String {
    if (type == KeyboardType.Piano) return "piano"
    val wavePart = waveform.name.lowercase()
    val typePart = if (type == KeyboardType.Poly) "polysynth" else "monosynth"
    return "${wavePart}_${typePart}"
}
```

### 8b. Replace `setKeyboardSound()` (lines 174–185) with two functions

```kotlin
// before
fun setKeyboardSound(sound: KeyboardSound) {
    val ids = arrayOf(
        "piano",
        "sine_oscillator",
        "saw_oscillator",
        "square_oscillator",
        "mono_osc"
    )
    NativeAudioEngine.setInstrument(0, ids[sound.ordinal])
    _uiState.value = _uiState.value.copy(keyboardSound = sound)
    viewModelScope.launch { keyboardSoundStore.save(sound) }
}

// after
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

### 8c. Update `selectEngine()` (lines 215–224)

```kotlin
// before
fun selectEngine(engine: AudioEngine) {
    val current = _uiState.value
    val drumIds = arrayOf("noise_drum", "fm_drum", "sample_drum")
    val keyIds = arrayOf("piano", "sine_oscillator", "saw_oscillator", "square_oscillator", "mono_osc")
    NativeAudioEngine.setOutput(engine)
    NativeAudioEngine.setInstrument(0, keyIds[current.keyboardSound.ordinal])
    NativeAudioEngine.setInstrument(9, drumIds[current.drumBackend.ordinal])
    NativeAudioEngine.start()
    _uiState.value = current.copy(engine = engine)
}

// after
fun selectEngine(engine: AudioEngine) {
    val current = _uiState.value
    val drumIds = arrayOf("noise_drum", "fm_drum", "sample_drum")
    NativeAudioEngine.setOutput(engine)
    NativeAudioEngine.setInstrument(0, instrumentId(current.keyboardType, current.synthWaveform))
    NativeAudioEngine.setInstrument(9, drumIds[current.drumBackend.ordinal])
    NativeAudioEngine.start()
    _uiState.value = current.copy(engine = engine)
}
```

---

## Step 9 — DataStore: `KeyboardTypeStore` + `WaveformStore`

### 9a. Rename `KeyboardSoundStore.kt` → `KeyboardTypeStore.kt` and rewrite

**File:** `app/src/main/java/org/gilbertxenodike/btmid/KeyboardTypeStore.kt`

```kotlin
package org.gilbertxenodike.btmid

import android.content.Context
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

private val KEYBOARD_TYPE_KEY = stringPreferencesKey("keyboard_type")

class KeyboardTypeStore(private val context: Context) {
    val keyboardType: Flow<KeyboardType> = context.dataStore.data
        .map { prefs ->
            KeyboardType.entries.firstOrNull { it.name == prefs[KEYBOARD_TYPE_KEY] }
                ?: KeyboardType.Piano
        }

    suspend fun save(type: KeyboardType) {
        context.dataStore.edit { prefs ->
            prefs[KEYBOARD_TYPE_KEY] = type.name
        }
    }
}
```

Note: the DataStore key changes from `"keyboard_sound"` to `"keyboard_type"`. Any previously saved value is silently discarded (falls back to `KeyboardType.Piano`) — this is acceptable.

### 9b. Create `WaveformStore.kt`

**New file:** `app/src/main/java/org/gilbertxenodike/btmid/WaveformStore.kt`

```kotlin
package org.gilbertxenodike.btmid

import android.content.Context
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

private val WAVEFORM_KEY = stringPreferencesKey("synth_waveform")

class WaveformStore(private val context: Context) {
    val waveform: Flow<SynthWaveform> = context.dataStore.data
        .map { prefs ->
            SynthWaveform.entries.firstOrNull { it.name == prefs[WAVEFORM_KEY] }
                ?: SynthWaveform.Sine
        }

    suspend fun save(waveform: SynthWaveform) {
        context.dataStore.edit { prefs ->
            prefs[WAVEFORM_KEY] = waveform.name
        }
    }
}
```

---

## Step 10 — `MainViewModel.kt`: update store fields and `init`

### 10a. Update store field declarations (around line 69)

```kotlin
// before
private val keyboardSoundStore: KeyboardSoundStore = KeyboardSoundStore(application)

// after
private val keyboardTypeStore = KeyboardTypeStore(application)
private val waveformStore     = WaveformStore(application)
```

### 10b. Update the `init` restore block (lines 82–85)

```kotlin
// before
viewModelScope.launch {
    val saved = keyboardSoundStore.keyboardSound.first()
    setKeyboardSound(saved)
}

// after
viewModelScope.launch {
    val savedType = keyboardTypeStore.keyboardType.first()
    val savedWave = waveformStore.waveform.first()
    val id = instrumentId(savedType, savedWave)
    NativeAudioEngine.setInstrument(0, id)
    _uiState.value = _uiState.value.copy(keyboardType = savedType, synthWaveform = savedWave)
}
```

---

## Step 11 — `KeyboardSoundSelector.kt`: update to `KeyboardType`

**File:** `app/src/main/java/org/gilbertxenodike/btmid/ui/KeyboardSoundSelector.kt`

Full replacement (the file can keep its name or be renamed to `KeyboardTypeSelector.kt` — either is fine):

```kotlin
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
```

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

## Step 13 — `MainScreen.kt`: replace selector call + add WaveformSelector

**File:** `app/src/main/java/org/gilbertxenodike/btmid/ui/MainScreen.kt`

Lines 130–133:

```kotlin
// before
            KeyboardSoundSelector(
                selected = uiState.keyboardSound,
                onSelect = onSetKeyboardSound,
            )

// after
            KeyboardSoundSelector(
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

## Step 14 — `MainScreen.kt`: update signature, imports, previews

### 14a. Update function signature (lines 51–66)

```kotlin
// before
    onSetKeyboardSound: (KeyboardSound) -> Unit,

// after — replace that one line with:
    onSetKeyboardType: (KeyboardType) -> Unit,
    onSetWaveform: (SynthWaveform) -> Unit,
```

### 14b. Update imports (around lines 39–48)

```kotlin
// remove
import org.gilbertxenodike.btmid.KeyboardSound

// add
import org.gilbertxenodike.btmid.KeyboardType
import org.gilbertxenodike.btmid.SynthWaveform
```

### 14c. Update `MainScreenPermissionNeededPreview` (around line 326)

```kotlin
// before
            onSetKeyboardSound = {},

// after
            onSetKeyboardType = {},
            onSetWaveform = {},
```

Also update the `UiState()` inside if it uses `keyboardSound` — the default `UiState()` will be fine once the field is renamed.

### 14d. Update `MainScreenConnectedPreview` (around line 358)

Same replacement:
```kotlin
// before
            onSetKeyboardSound = {},

// after
            onSetKeyboardType = {},
            onSetWaveform = {},
```

### 14e. Update `MainScreenScanningPreview` (around line 391)

Same replacement:
```kotlin
// before
            onSetKeyboardSound = {},

// after
            onSetKeyboardType = {},
            onSetWaveform = {},
```

---

## Step 15 — `MainActivity.kt`: wire new callbacks

**File:** `app/src/main/java/org/gilbertxenodike/btmid/MainActivity.kt`

Line 51:

```kotlin
// before
                        onSetKeyboardSound = { vm.setKeyboardSound(it) },

// after
                        onSetKeyboardType = { vm.setKeyboardType(it) },
                        onSetWaveform     = { vm.setWaveform(it)     },
```

---

## Test Plan

1. `./gradlew assembleDebug` — should compile clean with no references to old IDs or `KeyboardSound`
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
