# TODO

## Bugs

- [ ] `selectEngine` never updates `UiState.engine` — after switching to WiFi, `EngineSelector` still shows Oboe and `typedIp` reverts to empty on re-open (`MainViewModel.kt:154`)
- [ ] Engine switch silently resets drum backend to `sample_drum` — `selectEngine` ignores `_uiState.value.drumBackend`, so FM/Noise selection is lost on engine change (`MainViewModel.kt:157`)
- [ ] WiFi Save button always enabled — can save with empty/invalid IP; `WifiEngine` construction silently fails via `inet_pton` (`EngineSelector.kt:148`)
- [ ] WifiEngine has no dispatch thread — `mEventQueue` filled by `pollMidi()` is never drained, so MIDI activity indicator and event log are dead in WiFi mode (`WifiEngine` missing `dispatchLoop` + override of `setOutputPort`)

## Missing Features

- [ ] Persist WiFi IP address — `DrumBackendStore` pattern already exists; add a `WifiHostStore` so users don't retype the IP every launch
- [ ] Octave shift for on-screen piano — up/down arrows to shift the one-octave keyboard range

## Performance / Architecture

- [ ] `PianoSinTable` is implemented but never wired up in `InstrumentRepository` — either replace `Piano` with it or delete it
- [ ] `dispatchLoop` spins on 1ms nanosleep — post a semaphore from the audio thread on each ring push so the dispatch thread sleeps until work arrives (`OboeEngine.cpp:77`)
- [ ] `SustainButton` calls `NativeAudioEngine` directly and uses local Compose state — resets on configuration change; move to `UiState` and route through ViewModel (`MainScreen.kt:221`)
