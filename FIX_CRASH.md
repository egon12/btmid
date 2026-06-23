# Fix Disconnect Crash (Use-After-Close on AMidi Port)

## Context

When the user taps Disconnect, `BleMidiConnection.disconnect()` calls `NativeAudioEngine.clearOutputPort()` but does **not** stop the Oboe audio stream first. The Oboe audio thread keeps running `onAudioReady()` → `pollMidi()`, which atomically loads `mMidiPort` and then calls `AMidiOutputPort_receive()`. Meanwhile, `ChannelEngine::clearOutputPort()` atomically exchanges `mMidiPort` to `nullptr` and closes the port. There is a window where the audio thread has already loaded the port pointer but hasn't called `AMidiOutputPort_receive()` yet — by the time it does, the port is closed. This is a use-after-close / use-after-free crash.

**Race condition:**
```
Audio thread                            Main thread (Disconnect)
pollMidi():
  port = mMidiPort.load(...)            clearOutputPort():
  // port is non-null here                port = mMidiPort.exchange(nullptr)
                                           AMidiOutputPort_close(port)  ← port closed
  AMidiOutputPort_receive(port, ...)   ← CRASH: port already closed
```

## Root Cause Location

- `app/src/main/java/org/gilbertxenodike/btmid/bluetooth/BleMidiConnection.kt` line 62–69:
  `disconnect()` calls `clearOutputPort()` but never calls `stop()`.
- `app/src/main/cpp/ChannelEngine.cpp` line 32–59: `pollMidi()` loads and uses `mMidiPort` in the audio thread.
- `app/src/main/cpp/ChannelEngine.cpp` line 140–142: `clearOutputPort()` closes the port atomically, but doesn't wait for the audio thread to finish its current `pollMidi()` invocation.

## Fix

**One-line change location**: `BleMidiConnection.kt::disconnect()` — call `stop()` before `clearOutputPort()`, then `start()` to resume the stream for on-screen keyboard use.

```kotlin
// BleMidiConnection.kt
fun disconnect() {
    NativeAudioEngine.stop()          // stops Oboe stream → no more onAudioReady callbacks
    NativeAudioEngine.clearOutputPort()  // now safe: audio thread is not running
    NativeAudioEngine.start()         // restart stream for on-screen piano/drum pads
    midiDevice?.close()
    midiDevice = null
    gatt?.close()
    gatt = null
    Log.d(TAG, "Disconnected")
}
```

### Why this works

- `OboeEngine::stop()` calls `mStream->requestStop()` + `mStream->close()`, which fully tears down the Oboe stream and guarantees `onAudioReady` is not running when it returns.
- `clearOutputPort()` can then safely close `AMidiOutputPort` and `AMidiDevice` with no audio thread racing against it.
- `start()` opens a fresh Oboe stream so the on-screen piano and drum pads keep working.

### Why not fix it in C++

`OboeEngine::clearOutputPort()` could call `stop()`+`start()` internally, but that couples lifecycle concerns into what should be a port-only teardown, and it would make the destructor path (which also calls `stop()` then `clearOutputPort()`) call `stop()` twice. Fixing it in Kotlin is cleaner and mirrors the existing `onCleared()` order (`bleMidiConnection.disconnect()` then `NativeAudioEngine.stop()`).

## Files to Modify

- `app/src/main/java/org/gilbertxenodike/btmid/bluetooth/BleMidiConnection.kt` — add `NativeAudioEngine.stop()` and `NativeAudioEngine.start()` around `clearOutputPort()` in `disconnect()`.

## Verification

1. Build: `./gradlew assembleDebug`
2. Install on device; connect a BLE MIDI device.
3. Play some notes, then tap Disconnect — no crash (previously crashed or produced logcat SIGSEGV/SIGBUS).
4. Confirm on-screen piano and drum pads still produce audio after disconnect.
5. Connect again and verify MIDI input resumes.
6. Check logcat for `AMidiOutputPort` errors or native crashes.
