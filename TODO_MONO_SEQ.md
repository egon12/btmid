# MonoOscillator Waveform Selection — Detailed Plan

## UI Layout (two-row FilterChip)

When `Mono` is the selected keyboard sound, a second chip row appears directly below
the existing `KeyboardSoundSelector`:

```
Row 1 — always visible:
[ Piano ]  [ Poly* ]  [ Mono* ]

Row 2 — only when Poly or Mono* is selected:
[ Sine ]  [ Saw ]  [ Square ]
```

This mirrors the exact same pattern as `DrumEngineSelector` beneath the drum trigger —
inline, no dialog, immediate feedback while playing.

---

## Checklist

- [ ] Step 1 — C++: Add `Waveform` enum + `setWaveform()` to `MonoOscillator.h`
- [ ] Step 2 — C++: Implement saw/square + drain waveform queue in `MonoOscillator.cpp`
- [ ] Step 3 — C++: Register three new IDs in `InstrumentRepository.cpp`
- [ ] Step 4 — Kotlin: Add `MonoWaveform` enum to `MainViewModel.kt`
- [ ] Step 5 — Kotlin: Add `monoWaveform` field to `UiState`
- [ ] Step 6 — Kotlin: Create `MonoWaveformStore.kt`
- [ ] Step 7 — Kotlin: Wire `setMonoWaveform()` in `MainViewModel.kt`
- [ ] Step 8 — Kotlin: Fix `setKeyboardSound()` and `selectEngine()` in `MainViewModel.kt`
- [ ] Step 9 — UI: Create `MonoWaveformSelector.kt`
- [ ] Step 10 — UI: Update `MainScreen.kt` to show second row and pass new callbacks
- [ ] Step 11 — UI: Update `MainScreen.kt` previews

---

## Step 1 — `MonoOscillator.h`: add `Waveform` enum, member, queue, setter

**File:** `app/src/main/cpp/instruments/MonoOscillator.h`

### 1a. Add `Waveform` enum inside the class (after the opening `{`)

```cpp
public:
    enum class Waveform { Sine, Saw, Square };

    explicit MonoOscillator(float portamentoTau = 0.15f,
                            Waveform waveform = Waveform::Sine);
```

Replace the existing constructor declaration:
```cpp
// before
explicit MonoOscillator(float portamentoTau = 0.15f);
// after
explicit MonoOscillator(float portamentoTau = 0.15f,
                        Waveform waveform = Waveform::Sine);
```

### 1b. Add `setWaveform()` public method (after `setPortamentoTime`)

```cpp
    void setPortamentoTime(float tau);
    void setWaveform(Waveform wf);      // ← add this line
```

### 1c. Add `WaveformEvent` struct alongside the other event structs (in the private section)

```cpp
    struct NoteOnEvent  { int note; int velocity; };
    struct NoteOffEvent { int note; };
    struct CcEvent      { int cc; int value; };
    struct WaveformEvent { Waveform wf; };   // ← add this line
```

### 1d. Add the queue and member after the existing `SpscRing` declarations

```cpp
    SpscRing<NoteOnEvent,    kQueueCap> mOnQueue;
    SpscRing<NoteOffEvent,   kQueueCap> mOffQueue;
    SpscRing<CcEvent,        kQueueCap> mCcQueue;
    SpscRing<WaveformEvent,  8>         mWfQueue;   // ← add (capacity 8 is enough)
```

And add the member after `mPortamentoCoeff`:

```cpp
    float mPortamentoCoeff{0.0f};
    Waveform mWaveform{Waveform::Sine};   // ← add this line
```

---

## Step 2 — `MonoOscillator.cpp`: implement saw/square + drain waveform queue

**File:** `app/src/main/cpp/instruments/MonoOscillator.cpp`

### 2a. Update the constructor to accept and store the waveform

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

### 2b. Add `setWaveform()` implementation (after `recalcPortamentoCoeff`)

```cpp
void MonoOscillator::setWaveform(Waveform wf) {
    mWfQueue.push({wf});
}
```

### 2c. Drain `mWfQueue` at the top of `render()` (alongside the other queues)

```cpp
void MonoOscillator::render(float* buffer, int32_t frames) {
    NoteOnEvent onEv{};
    while (mOnQueue.pop(onEv)) handleNoteOn(onEv.note, onEv.velocity);

    NoteOffEvent offEv{};
    while (mOffQueue.pop(offEv)) handleNoteOff(offEv.note);

    CcEvent ccEv{};
    while (mCcQueue.pop(ccEv)) {
        if (ccEv.cc == 74) {
            float tau = 0.005f + (static_cast<float>(ccEv.value) / 127.0f) * 0.395f;
            setPortamentoTime(tau);
        }
    }

    WaveformEvent wfEv{};                          // ← add these two lines
    while (mWfQueue.pop(wfEv)) mWaveform = wfEv.wf;
    // ...
```

### 2d. Replace the hardcoded `sinf` sample line with a waveform branch

```cpp
// before (line 129)
float sample = std::sinf(2.0f * static_cast<float>(M_PI) * mPhaseAccum);

// after
float sample;
switch (mWaveform) {
    case Waveform::Sine:
        sample = std::sinf(2.0f * static_cast<float>(M_PI) * mPhaseAccum);
        break;
    case Waveform::Saw:
        sample = 2.0f * mPhaseAccum - 1.0f;
        break;
    case Waveform::Square:
        sample = mPhaseAccum < 0.5f ? 1.0f : -1.0f;
        break;
    default:
        sample = 0.0f;
        break;
}
```

---

## Step 3 — `InstrumentRepository.cpp`: register three waveform IDs

**File:** `app/src/main/cpp/InstrumentRepository.cpp`

In `getOrCreate()`, after the existing `} else if (id == "mono_osc") {` block add:

```cpp
    } else if (id == "mono_osc") {
        inst = std::make_unique<MonoOscillator>(0.15f, MonoOscillator::Waveform::Sine);
    } else if (id == "mono_osc_sine") {
        inst = std::make_unique<MonoOscillator>(0.15f, MonoOscillator::Waveform::Sine);
    } else if (id == "mono_osc_saw") {
        inst = std::make_unique<MonoOscillator>(0.15f, MonoOscillator::Waveform::Saw);
    } else if (id == "mono_osc_square") {
        inst = std::make_unique<MonoOscillator>(0.15f, MonoOscillator::Waveform::Square);
    }
```

Note: keep `"mono_osc"` as an alias for Sine (used by `KeyboardSoundStore` values saved
before this change — avoids crashes on upgrade).

---

## Step 4 — `MainViewModel.kt`: add `MonoWaveform` enum

**File:** `app/src/main/java/org/egon12/btmid/MainViewModel.kt`

After the existing `DrumBackend` enum, add:

```kotlin
enum class DrumBackend { Noise, Fm, Samples }

enum class MonoWaveform { Sine, Saw, Square }   // ← add this
```

---

## Step 5 — `UiState`: add `monoWaveform` field

**File:** `app/src/main/java/org/egon12/btmid/MainViewModel.kt`

In `data class UiState(...)`, add the new field after `keyboardSound`:

```kotlin
data class UiState(
    val permissionsGranted: Boolean = false,
    val connectionStatus: ConnectionStatus = ConnectionStatus.Idle,
    val discoveredDevices: List<DeviceUiState> = emptyList(),
    val connectedDeviceAddress: String? = null,
    val recentEvents: List<MidiEventUiModel> = emptyList(),
    val midiActivityPulse: Boolean = false,
    val drumBackend: DrumBackend = DrumBackend.Noise,
    val samplesLoaded: Boolean = false,
    val engine: AudioEngine = AudioEngine.Oboe,
    val selectEngineDialogVisible: Boolean = false,
    val keyboardSound: KeyboardSound = KeyboardSound.Piano,
    val monoWaveform: MonoWaveform = MonoWaveform.Sine,   // ← add this
)
```

---

## Step 6 — Create `MonoWaveformStore.kt`

**New file:** `app/src/main/java/org/egon12/btmid/MonoWaveformStore.kt`

Exact same pattern as `KeyboardSoundStore.kt`:

```kotlin
package org.gilbertxenodike.btmid

import android.content.Context
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

private val MONO_WAVEFORM_KEY = stringPreferencesKey("mono_waveform")

class MonoWaveformStore(private val context: Context) {
    val monoWaveform: Flow<MonoWaveform> = context.dataStore.data
        .map { prefs ->
            MonoWaveform.entries.firstOrNull { it.name == prefs[MONO_WAVEFORM_KEY] }
                ?: MonoWaveform.Sine
        }

    suspend fun save(waveform: MonoWaveform) {
        context.dataStore.edit { prefs ->
            prefs[MONO_WAVEFORM_KEY] = waveform.name
        }
    }
}
```

`context.dataStore` is the same extension already used by `KeyboardSoundStore` —
no new DataStore instance needed.

---

## Step 7 — `MainViewModel.kt`: wire `setMonoWaveform()`

**File:** `app/src/main/java/org/egon12/btmid/MainViewModel.kt`

### 7a. Declare the store (after `keyboardSoundStore`)

```kotlin
private val keyboardSoundStore: KeyboardSoundStore = KeyboardSoundStore(application)
private val monoWaveformStore: MonoWaveformStore = MonoWaveformStore(application)   // ← add
```

### 7b. Restore from DataStore in `init` (after the `keyboardSound` restore block)

```kotlin
viewModelScope.launch {
    val saved = keyboardSoundStore.keyboardSound.first()
    setKeyboardSound(saved)
}
viewModelScope.launch {                                        // ← add this block
    val saved = monoWaveformStore.monoWaveform.first()
    setMonoWaveform(saved)
}
```

### 7c. Add `setMonoWaveform()` function (after `setKeyboardSound`)

```kotlin
fun setMonoWaveform(waveform: MonoWaveform) {
    val id = when (waveform) {
        MonoWaveform.Sine   -> "mono_osc_sine"
        MonoWaveform.Saw    -> "mono_osc_saw"
        MonoWaveform.Square -> "mono_osc_square"
    }
    if (_uiState.value.keyboardSound == KeyboardSound.Mono) {
        NativeAudioEngine.setInstrument(0, id)
    }
    _uiState.value = _uiState.value.copy(monoWaveform = waveform)
    viewModelScope.launch { monoWaveformStore.save(waveform) }
}
```

The `if keyboardSound == Mono` guard means switching waveform while Piano/Sine/etc. is
active doesn't disrupt the active instrument; the correct ID is picked when the user
switches back to Mono (see Step 8).

---

## Step 8 — `MainViewModel.kt`: fix `setKeyboardSound()` and `selectEngine()`

**File:** `app/src/main/java/org/egon12/btmid/MainViewModel.kt`

### 8a. Helper to map current waveform to mono instrument ID

Add a private helper near the top of the class body:

```kotlin
private fun monoInstrumentId(): String = when (_uiState.value.monoWaveform) {
    MonoWaveform.Sine   -> "mono_osc_sine"
    MonoWaveform.Saw    -> "mono_osc_saw"
    MonoWaveform.Square -> "mono_osc_square"
}
```

### 8b. Update `setKeyboardSound()` to use the helper for Mono

```kotlin
fun setKeyboardSound(sound: KeyboardSound) {
    val id = when (sound) {
        KeyboardSound.Piano  -> "piano"
        KeyboardSound.Sine   -> "sine_oscillator"
        KeyboardSound.Saw    -> "saw_oscillator"
        KeyboardSound.Square -> "square_oscillator"
        KeyboardSound.Mono   -> monoInstrumentId()   // ← was always "mono_osc"
    }
    NativeAudioEngine.setInstrument(0, id)
    _uiState.value = _uiState.value.copy(keyboardSound = sound)
    viewModelScope.launch { keyboardSoundStore.save(sound) }
}
```

Delete the old `val ids = arrayOf(...)` array approach — it is replaced by the `when`
expression above which is explicit and safe to extend.

### 8c. Update `selectEngine()` the same way

```kotlin
fun selectEngine(engine: AudioEngine) {
    val current = _uiState.value
    val drumIds = arrayOf("noise_drum", "fm_drum", "sample_drum")
    val keyId = when (current.keyboardSound) {
        KeyboardSound.Piano  -> "piano"
        KeyboardSound.Sine   -> "sine_oscillator"
        KeyboardSound.Saw    -> "saw_oscillator"
        KeyboardSound.Square -> "square_oscillator"
        KeyboardSound.Mono   -> monoInstrumentId()
    }
    NativeAudioEngine.setEngine(engine)
    NativeAudioEngine.setInstrument(0, keyId)
    NativeAudioEngine.setInstrument(9, drumIds[current.drumBackend.ordinal])
    NativeAudioEngine.start()
    _uiState.value = current.copy(engine = engine)
}
```

---

## Step 9 — Create `MonoWaveformSelector.kt`

**New file:** `app/src/main/java/org/egon12/btmid/ui/MonoWaveformSelector.kt`

Same structure as `DrumEngineSelector.kt` and `KeyboardSoundSelector.kt`:

```kotlin
package org.gilbertxnodike.btmid.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import org.egon12.btmid.MonoWaveform
import org.egon12.btmid.ui.theme.BtmidTheme

@Composable
fun MonoWaveformSelector(
    selected: MonoWaveform,
    onSelect: (MonoWaveform) -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(modifier = modifier, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        MonoWaveform.entries.forEach { waveform ->
            FilterChip(
                selected = waveform == selected,
                onClick = { onSelect(waveform) },
                label = { Text(waveform.label) },
            )
        }
    }
}

private val MonoWaveform.label get() = when (this) {
    MonoWaveform.Sine   -> "Sine"
    MonoWaveform.Saw    -> "Saw"
    MonoWaveform.Square -> "Square"
}

@Preview(showBackground = true)
@Composable
private fun MonoWaveformSelectorPreview() {
    BtmidTheme {
        MonoWaveformSelector(selected = MonoWaveform.Saw, onSelect = {})
    }
}
```

---

## Step 10 — `MainScreen.kt`: add second row and new callback

**File:** `app/src/main/java/org/egon12/btmid/ui/MainScreen.kt`

### 10a. Add `onSetMonoWaveform` parameter to `MainScreen`

```kotlin
@Composable
fun MainScreen(
    uiState: UiState,
    onGrantPermissions: () -> Unit,
    onStartScan: () -> Unit,
    onStopScan: () -> Unit,
    onConnect: (DeviceUiState) -> Unit,
    onDisconnect: () -> Unit,
    onSetDrumBackend: (DrumBackend) -> Unit,
    onSetKeyboardSound: (KeyboardSound) -> Unit,
    onSetMonoWaveform: (MonoWaveform) -> Unit,   // ← add this
    showSelectEngineDialog: (Boolean) -> Unit,
    onSelectEngine: (AudioEngine) -> Unit,
    modifier: Modifier = Modifier,
)
```

Also add the missing import at the top of the file:
```kotlin
import org.gilbertxenodike.btmid.MonoWaveform
```

### 10b. Add the second chip row after `KeyboardSoundSelector`

Replace the current block (lines 119–122):

```kotlin
// before
KeyboardSoundSelector(
    selected = uiState.keyboardSound,
    onSelect = onSetKeyboardSound,
)

// after
KeyboardSoundSelector(
    selected = uiState.keyboardSound,
    onSelect = onSetKeyboardSound,
)
if (uiState.keyboardSound == KeyboardSound.Mono) {
    MonoWaveformSelector(
        selected = uiState.monoWaveform,
        onSelect = onSetMonoWaveform,
    )
}
```

---

## Step 11 — `MainScreen.kt`: update the three `@Preview` composables

All three previews at the bottom of `MainScreen.kt` call `MainScreen(...)` without
`onSetMonoWaveform`. Add the argument to each:

```kotlin
onSetMonoWaveform = {},
```

Example for `MainScreenConnectedPreview`:

```kotlin
MainScreen(
    uiState = UiState(
        permissionsGranted = true,
        connectionStatus = ConnectionStatus.Connected,
        // ... existing fields ...
        keyboardSound = KeyboardSound.Mono,       // ← set to Mono so row 2 is visible
        monoWaveform = MonoWaveform.Saw,           // ← show non-default for visibility
    ),
    onGrantPermissions = {},
    onStartScan = {},
    onStopScan = {},
    onConnect = {},
    onDisconnect = {},
    onSetDrumBackend = {},
    onSetKeyboardSound = {},
    onSetMonoWaveform = {},                        // ← add
    showSelectEngineDialog = {},
    onSelectEngine = {},
)
```

---

## Step 12 — Wire `onSetMonoWaveform` in `MainActivity.kt`

**File:** `app/src/main/java/org/egon12/btmid/MainActivity.kt`

Find where `MainScreen(...)` is called (inside `setContent { ... }`). Add:

```kotlin
onSetMonoWaveform = { viewModel.setMonoWaveform(it) },
```

---

## Test plan

1. Build and install: `./gradlew assembleDebug`
2. Open app → tap `[ Mono ]` chip → confirm second row `[ Sine ] [ Saw ] [ Square ]` appears
3. Play piano keys → confirm sound is audible sine
4. Tap `[ Saw ]` → play keys → confirm buzzier timbre
5. Tap `[ Square ]` → play keys → confirm hollow timbre
6. Tap `[ Piano ]` chip → confirm second row disappears
7. Tap `[ Mono ]` again → confirm waveform row reappears, previous selection retained
8. Force-close app and reopen → confirm Mono waveform choice is restored (DataStore)
9. Switch engine (Oboe ↔ Wifi) while Mono + Saw is active → confirm correct instrument ID
   is re-registered after the engine swap
