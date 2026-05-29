# Drum Sound Upgrade — FM Synth + Sample Playback

## Goal

Upgrade the drum engine from the current noise-burst synthesis to two higher-quality backends that the user can switch between at runtime:

| Backend | Description |
|---------|-------------|
| **FM Synth** | Operator-based frequency modulation — no assets, fully algorithmic |
| **Samples** | Pre-loaded OGG per drum type — highest realism, same onset latency |

Piano is unchanged. The existing `PianoSynth` and `AudioEngine` stay as-is.

---

## Architecture

Introduce a `DrumSynth` interface so both backends are interchangeable:

```kotlin
interface DrumSynth {
    fun noteOn(note: Int, velocity: Int)
    fun noteOff(note: Int)
    fun render(buffer: FloatArray, offset: Int, length: Int)
    fun allNotesOff()
}
```

The existing `DrumSynth.kt` becomes `NoiseDrumSynth.kt` (rename, implement interface) — kept as a fallback.

---

## New / Changed Files

### New
```
synth/DrumSynth.kt              — interface (above)
synth/FmDrumSynth.kt            — FM operator engine per GM drum type
synth/SampleBank.kt             — decodes OGG assets → FloatArray[], keyed by drum name
synth/SampleDrumSynth.kt        — plays back pre-loaded samples
ui/DrumEngineSelector.kt        — Compose toggle: FM Synth | Samples (+ current noise-burst)
```

### Renamed / Modified
```
synth/DrumSynth.kt   → synth/NoiseDrumSynth.kt  (implement DrumSynth interface)
synth/AudioEngine.kt — hold a DrumSynth reference; expose fun setDrumBackend(backend)
MainViewModel.kt     — expose drumBackend: StateFlow<DrumBackend>; action setDrumBackend()
ui/MainScreen.kt     — embed DrumEngineSelector in settings/toolbar area
```

### Unchanged
```
synth/PianoSynth.kt, synth/Voice.kt
midi/MidiRouter.kt, midi/MidiParser.kt, midi/MidiEvent.kt
bluetooth/
```

---

## FM Drum Synthesis — Per-Instrument Design

Each GM note maps to an FM "patch" (carrier + one modulator unless noted):

| GM Note(s) | Sound | Carrier | Modulator | Env |
|------------|-------|---------|-----------|-----|
| 35 / 36 | Bass Drum | 50 Hz sine, sweep → 30 Hz | self-FM index 4→0 | 180 ms exp |
| 38 / 40 | Snare | 200 Hz + white noise | mod index 2, 180 Hz mod freq | 120 ms |
| 42 | Closed Hi-Hat | 6-op ratio stack (8:1 ratio) → metallic | high mod index | 45 ms |
| 46 | Open Hi-Hat | same as closed hat | — | 350 ms |
| 49 / 57 | Crash | noise-seeded 6-op stack | random phases | 900 ms |
| 51 | Ride | 600 Hz + 3-op metallic | mod index 1.5 | 500 ms |
| 41 / 43 / 45 / 47 / 48 / 50 | Toms | 120–180 Hz sweep, self-FM | index 3→0 | 150 ms |

FM formula per sample:
```
mod  = sin(2π × fMod × t + modIndex × sin(2π × fMod × t))   // self-FM or separate mod
out  = amplitude × env(t) × sin(2π × fCarrier × t + modDepth × mod)
```

Frequency sweep (kick/toms): `f(t) = fEnd + (fStart - fEnd) × e^(-t / sweepTau)`

---

## Sample Assets

**Location:** `app/src/main/assets/samples/drums/`

| File | GM notes | Target size |
|------|----------|-------------|
| `kick.ogg` | 35, 36 | ~80 KB |
| `snare.ogg` | 38, 40 | ~60 KB |
| `closed_hat.ogg` | 42 | ~30 KB |
| `open_hat.ogg` | 46 | ~50 KB |
| `crash.ogg` | 49, 57 | ~90 KB |
| `ride.ogg` | 51 | ~70 KB |
| `tom_low.ogg` | 41, 43 | ~60 KB |
| `tom_mid.ogg` | 45, 47 | ~60 KB |
| `tom_high.ogg` | 48, 50 | ~60 KB |

**Total APK addition: ~560 KB**

Source options (all CC0/public domain): Freepats, samples-from-mscore, or LMMS default kit.

**OGG decoding:** `MediaCodec` + `MediaExtractor` at startup (~100 ms for ~560 KB). Decoded once → mono 44100 Hz `FloatArray` per file. No external library.

---

## UI — Backend Selector

A segmented control (three-chip row) in `MainScreen`:

```
[ Noise (current) ]  [ FM Synth ]  [ Samples ]
```

- Shown below the connection status row, always visible (not hidden in a settings sheet)
- Switching is instant — `AudioEngine.setDrumBackend()` swaps the reference between render passes using `@Volatile` + a pending-swap queue (same pattern as note events)
- "Samples" chip is greyed out with a loading indicator until `SampleBank` finishes decoding at startup

---

## Thread Safety

Same pattern as existing code — no new locking primitives:

- `noteOn`/`noteOff` post a command into a `ConcurrentLinkedQueue`
- Render loop drains queue at top of each pass
- Backend swap: `AudioEngine` holds `@Volatile var activeDrumSynth: DrumSynth`; swap is written from main thread, read from render thread — one-word volatile write is safe

---

## Delivery Steps

- [ ] **Step 0** — This plan ✓
- [ ] **Step 1** — Extract `DrumSynth` interface; rename existing class to `NoiseDrumSynth`; wire `AudioEngine` to hold interface reference — app still works identically
- [ ] **Step 2** — `FmDrumSynth`: implement kick + snare only; swap manually in code to verify FM sounds play
- [ ] **Step 3** — Complete `FmDrumSynth` with all remaining GM types (hats, crash, ride, toms)
- [ ] **Step 4** — `SampleBank` + `SampleDrumSynth`; add sample assets; verify sample playback
- [ ] **Step 5** — `DrumEngineSelector` UI + `MainViewModel` state; live switching between all three backends
- [ ] **Step 6** *(optional)* — Persist selected backend across app restarts via `DataStore`

---

## Deferred

- Piano upgrade (samples / FM) — separate plan
- Control Change (CC) handling
- Per-velocity sample layers
