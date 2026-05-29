# Sound Quality Upgrade — Raw PCM Samples

## Goal

Replace the current additive sine synthesis (PianoSynth) and noise-burst synthesis (DrumSynth) with real PCM samples pre-loaded into memory. Priority: **note-onset latency above all** — everything stays in RAM so onset is identical to the current synth.

## Approach Rationale

| Option | Ruled out because |
|--------|------------------|
| SF2 / MikroSoundFont | Pure-JVM sample rendering has unknown JIT/GC latency on first playback |
| FluidSynth AAR | Bundles native `.so` for 4 ABIs + large SF2 file — overkill |
| Wavetable / FM synth | Better than additive but still synthetic; same latency as samples, worse realism |
| **Raw PCM samples** | **Best realism, identical latency (pre-loaded FloatArray), pure Kotlin** ✓ |

## Sample Assets

**Piano** — `app/src/main/assets/samples/piano/`
- One OGG file per 3 semitones (15 files), covers MIDI 21–108
- Pitch-shifted ±1.5 semitones max via linear interpolation at render time
- Source: Freepats, GeneralUser GS, or Piano from Above (CC0)
- Target size: ~150–200 KB/file × 15 = ~2.5–3 MB

**Drums** — `app/src/main/assets/samples/drums/`
- One OGG per drum type (~10 files): kick, snare, closed_hat, open_hat, crash, ride, etc.
- No pitch-shifting needed
- Target size: ~100 KB/file × 10 = ~1 MB

**Total APK addition: ~4 MB**

## New / Changed Files

### New
```
synth/SampleBank.kt         — decodes OGG assets → FloatArray[], keyed by MIDI note
synth/SampleVoice.kt        — playing instance: pcm, readPos, pitchRatio, gain, ADSR state
synth/SamplePianoSynth.kt   — drop-in replacement for PianoSynth
synth/SampleDrumSynth.kt    — drop-in replacement for DrumSynth
```

### Modified
```
synth/AudioEngine.kt        — swap PianoSynth/DrumSynth → Sample* variants; add suspend init()
MainViewModel.kt            — call audioEngine.init() before first scan/connect
```

### Unchanged
```
midi/MidiRouter.kt          — no changes needed
midi/MidiParser.kt          — no changes needed
bluetooth/                  — no changes needed
```

## Key Implementation Notes

**OGG decoding:** `MediaCodec` + `MediaExtractor` (Android built-in). Decode once at startup (~200 ms), store as mono 44100 Hz `FloatArray`. No external library needed.

**Pitch-shifting:**
```
pitchRatio = 2^((targetNote - rootNote) / 12.0)
readPos advances by pitchRatio per output sample
linear interpolation between floor/ceil indices
```

**Piano ADSR:** keep existing values (attack 5ms, decay 80ms, sustain 60%, release 300ms).

**Voice cap:** 8 piano voices (steal oldest), unlimited drum voices.

**Thread safety:** same `ConcurrentLinkedQueue` pattern as current PianoSynth/DrumSynth — no changes to threading model.

## Delivery Steps

- [ ] **Step 0** — This file ✓
- [ ] **Step 1** — `SampleBank` + `SampleDrumSynth`; keep existing `PianoSynth`; test drums
- [ ] **Step 2** — `SamplePianoSynth` without pitch-shifting; verify sample playback on root notes
- [ ] **Step 3** — Add pitch-shifting; full 88-key range works
- [ ] **Step 4** — ADSR envelope on piano voices; proper attack/release shape
- [ ] **Step 5** *(optional)* — Second velocity layer (soft/hard) if sample library supports it

## Deferred

- **Control Change (CC) handling** — next task after sound upgrade
- **SF2 / FluidSynth** — valid upgrade path later if GM instrument variety is needed
- **Wavetable synth** — fallback if sample assets need to be removed
