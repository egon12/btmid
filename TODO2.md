# Repository Review TODOs

## 🚨 Critical / Bugs
- [ ] **Hardcoded UDP Port**: `jni_bridge.cpp:108` hardcodes port `5004` for `WifiEngine`, ignoring the `port` property defined in `AudioEngine.Wifi`. Pass it from Kotlin.
- [ ] **State Loss on Engine Switch**: `MainViewModel.kt:157` hardcodes `"sample_drum"` when switching engines, ignoring the user's currently selected `drumBackend`. Use `_uiState.value.drumBackend` instead.
- [ ] **Missing BLE Disconnect Handling**: `BleMidiConnection.kt` does not handle `BluetoothProfile.STATE_DISCONNECTED` in `onConnectionStateChange`. Unexpected drops will leave the UI stuck in "Connected" and leak resources.

## ⚠️ Stability & Safety
- [ ] **JNI Exception Crash Risk**: `OboeEngine.cpp:74` calls `env->CallVoidMethod` in a loop without checking `env->ExceptionCheck()`. If the Kotlin callback throws, the native thread will crash. Add exception clearing.
- [ ] **Floating-Point Silence Check**: `WifiEngine.cpp:90` uses `s != 0.0f` for silence suppression. Due to floating-point arithmetic, use `std::abs(s) > 1e-6f` to avoid false negatives.
- [ ] **Memory Order Consistency**: In `OboeEngine`/`WifiEngine`, `mDispatchRunning.store(false)` uses `memory_order_relaxed`. Change to `memory_order_release` to properly synchronize with the `memory_order_acquire` load in the loop.

## 🏗️ Architecture & Design
- [ ] **Enum Ordinal Coupling**: `MainViewModel.kt:138` passes `backend.ordinal` to JNI. If the enum order changes, C++ breaks. Pass `backend.name` and map it in C++ (or use explicit constants).
- [ ] **File Organization**: `AudioEngine` sealed class is defined in `MainViewModel.kt` but imported as `org.egon12.btmid.AudioEngine` elsewhere. Move it to its own `AudioEngine.kt` file.
- [ ] **Instrument Re-application**: `AudioGraph::setEngine` stops the old engine but doesn't re-apply instruments to the new one. Consider caching the channel mapping in `AudioGraph` and re-applying it automatically on swap.

## 🧪 Testing & Build
- [ ] **Min SDK 36**: Targeting API 36 is bleeding-edge. Ensure this is intentional, as it severely limits physical device testing.
- [ ] **Expand Host Tests**: The `cpp_host_tests` directory is a great start. Add tests for `MidiParser` (running status, edge cases) and `Instrument` rendering to prevent audio regressions.
- [ ] **ProGuard/R8**: `isMinifyEnabled = false` in release. Enable it and add `@Keep` annotations to `NativeAudioEngine` JNI methods to prevent stripping.
