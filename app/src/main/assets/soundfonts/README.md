# SoundFonts

Drop a General MIDI SoundFont here named **`gm.sf2`**. It is loaded at startup by
`SoundFontBank` and played by the C++ `SfInstrument` (TinySoundFont) when the
**SF** keyboard sound is selected.

Requirements:
- Format: SF2 (or SF3). TinySoundFont parses it directly — no conversion needed.
- License: pick a permissively-licensed font you may redistribute (e.g. GeneralUser
  GS, or a small set such as TimGM6mb). Verify the license before committing.
- Size: the file is packaged into the APK as-is, so prefer a small font to keep the
  APK lean.

If `gm.sf2` is absent or invalid, startup does not crash — the **SF** chip simply
stays disabled.
