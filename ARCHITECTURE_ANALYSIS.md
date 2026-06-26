# Architecture Analysis

Harsh evaluation of the current C++ native audio architecture and proposed improvements.

## Current Architecture Problems

### 1. ChannelEngine is a God Object

It's doing 6 different things:
- MIDI I/O (AMidi port open/close/poll)
- MIDI routing (channel → instrument)
- Audio rendering (renderChannels)
- Loop recording integration
- Event queue management (SpscRing for UI)
- JNI callback registration

That's too many responsibilities for one class. It violates single responsibility.

### 2. The Inheritance Hierarchy is Backwards

```
AudioEngine (interface) → ChannelEngine (partial impl) → OboeEngine/WifiEngine
```

`ChannelEngine` does 90% of the work. `OboeEngine` and `WifiEngine` just add I/O specifics. This should be **composition**, not inheritance. You have a shallow hierarchy where the "partial implementation" is actually the main implementation.

### 3. pollMidi() in the Audio Callback is a Side Effect

MIDI polling happens in `onAudioReady()` for low latency — that's correct. But `pollMidi()` is a method on `ChannelEngine`, which means the audio engine is doing I/O. This is a leaky abstraction. The audio engine should only know about rendering audio, not polling MIDI ports.

### 4. Event Queue and JNI Callbacks are Infrastructure, Not Audio Engine Concerns

The `SpscRing<MidiEvt>` and JNI callback management are for UI notification. This is orthogonal to audio rendering. It shouldn't live in the audio engine. It's a cross-cutting concern that got dumped into `ChannelEngine` because it was convenient.

### 5. Protected Methods Create Tight Coupling

`pollMidi()` and `renderChannels()` are protected methods. Subclasses must call them in the right order. This is fragile and hard to test.

## Proposed Design

```
AudioGraph
├── InstrumentRepository
├── LoopRecorder
├── MidiRouter (new — pure MIDI routing + rendering)
│   ├── mChannels[16] (atomic<Instrument*>)
│   ├── noteOn/Off/CC() — routes to instruments
│   ├── render(float* buf, int32_t frames) — renders all active instruments
│   └── clear() — resets all channels
├── MidiIO (new — MIDI I/O + platform integration)
│   ├── AMidi port management
│   ├── pollMidi() — drains AMidi, calls midiRouter.noteOn/Off/CC
│   ├── Event queue (SpscRing)
│   ├── JNI callback management
│   └── onMidiEvent callback → LoopRecorder
└── unique_ptr<AudioOutput>
    ├── OboeOutput
    │   ├── Oboe stream
    │   ├── onAudioReady() → midiIO.pollMidi() → midiRouter.render()
    │   └── dispatch thread → drains event queue → UI
    └── WifiOutput
        ├── UDP + Opus
        └── udpRenderLoop() → midiIO.pollMidi() → midiRouter.render()
```

### Why This is Better

- **MidiRouter** — pure MIDI routing and rendering. No I/O, no platform glue, no event queue. Easy to test.
- **MidiIO** — MIDI I/O and platform integration. Owns AMidi, event queue, JNI callbacks. Notifies LoopRecorder.
- **AudioOutput** — audio output strategy. Calls into MidiIO and MidiRouter. Doesn't know about MIDI routing details.

**AudioGraph wires them together:**
- `midiIO.onMidiEvent = midiRouter.noteOn/Off/CC + loopRecorder.onMidiEvent`
- `audioOutput.onAudioReady = midiIO.pollMidi() + midiRouter.render()`

## Is This More Complicated?

Yes, it adds two new classes. But each class has a **single, clear responsibility**. The current design has one class doing everything, which is harder to understand and maintain.

## The Tradeoff

The current design is **simpler in terms of class count**, but **more complicated in terms of responsibilities per class**. The proposed design is **more classes**, but **each class is simpler**.

For a project this size, the tradeoff is worth it. But if minimal changes are preferred, there is a smaller refactor option.

## Minimal Refactor Option

If you want to minimize changes:

1. **Keep ChannelEngine but rename it to MidiEngine** — make it concrete, not a base class
2. **Extract AudioOutput as a strategy** — OboeOutput and WifiOutput become members, not subclasses
3. **Leave event queue and JNI callbacks in MidiEngine for now** — refactor later

This is a smaller refactor that still improves the design.

## Recommendation

- **Larger refactor**: go with the 3-class split (MidiRouter, MidiIO, AudioOutput)
- **Minimal changes**: extract AudioOutput as a strategy, keep everything else in MidiEngine

Either way, **get rid of the ChannelEngine inheritance hierarchy**. It's the root of the complexity.
