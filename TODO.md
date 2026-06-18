# TODO

## Bugs

- [ ] WifiEngine has no dispatch thread — `mEventQueue` filled by `pollMidi()` is never drained, so MIDI activity indicator and event log are dead in WiFi mode (`WifiEngine` missing `dispatchLoop` + override of `setOutputPort`)
- [ ] Hardcoded UDP port — `jni_bridge.cpp:115` passes `5004` ignoring the `port` property of `AudioEngine.Wifi`; pass it from Kotlin
- [ ] Missing BLE disconnect handling — `BleMidiConnection.kt` ignores `STATE_DISCONNECTED` in `onConnectionStateChange`; unexpected drops leave the UI stuck in "Connected" and leak resources

## Stability & Safety

- [ ] JNI exception crash risk — `OboeEngine.cpp:71` calls `env->CallVoidMethod` in a loop without `env->ExceptionCheck()`; a Kotlin callback throw will crash the native thread
- [ ] Floating-point silence check — `WifiEngine.cpp:89` uses `s != 0.0f`; use `std::abs(s) > 1e-6f` to avoid false negatives from floating-point arithmetic
- [ ] Memory order on dispatch start — `mDispatchRunning.store(true, memory_order_relaxed)` (`OboeEngine.cpp:49`) and `mUdpRunning.store(true, memory_order_relaxed)` (`WifiEngine.cpp:55`) should use `memory_order_release` to pair with the `acquire` loads in the loops

## Missing Features

- [ ] Persist WiFi IP address — `DrumBackendStore` pattern already exists; add a `WifiHostStore` so users don't retype the IP every launch
- [ ] Octave shift for on-screen piano — up/down arrows to shift the one-octave keyboard range

## Performance / Architecture

- [ ] `PianoSinTable` is implemented but never wired up in `InstrumentRepository` — either replace `Piano` with it or delete it
- [ ] `dispatchLoop` spins on 1ms nanosleep — post a semaphore from the audio thread on each ring push so the dispatch thread sleeps until work arrives (`OboeEngine.cpp:77`)
- [ ] `SustainButton` calls `NativeAudioEngine` directly and uses local Compose state — resets on configuration change; move to `UiState` and route through ViewModel (`MainScreen.kt:221`)
- [ ] Enum ordinal coupling — `MainViewModel.kt` passes `backend.ordinal` to JNI; if the enum order changes, C++ breaks silently; pass `backend.name` and map in C++ instead
- [ ] `AudioEngine` sealed class defined in `MainViewModel.kt` — move to its own `AudioEngine.kt` file
- [ ] Instrument re-application on engine swap — `AudioGraph::setEngine` stops the old engine but doesn't re-wire instruments to the new one; cache the channel mapping in `AudioGraph` and re-apply automatically

## Testing & Build

- [ ] Expand host tests — add `MidiParser` tests (running status, edge cases) and `Instrument` rendering tests to `cpp_host_tests` to prevent audio regressions
- [ ] Enable ProGuard/R8 — `isMinifyEnabled = false` in release build; enable it and add `@Keep` to `NativeAudioEngine` JNI methods to prevent stripping
