<div align="center">

## Sonic 3 A.I.R. for iOS · v0.2.0

**Classic Mode & Anniversary Mode — in one app**

[Download unsigned IPA](https://github.com/jentisalive/sonic3airios/releases/download/ios-v0.2.0/Sonic3AIR-iOS-unsigned.ipa)

</div>

### Switch editions from the main menu

Select **Classic Mode** below **Mods** to restart at the intro/title screen in Classic Mode. The same entry becomes **Anniversary Mode** to switch back. The selection is remembered after closing the app.

### Classic Mode

- **Real 320 × 240 gameplay** with square pixels and a 4:3 aspect ratio. The wide frame is not squeezed horizontally. Black bars appear on wider displays.
- **30 FPS presentation**, with game logic continuing at 60 Hz so gameplay and music retain normal speed. Frame interpolation is disabled in Classic Mode.
- **Locked visual settings:** Character Rotation: Original; Time Display: Original; Lives Display: Mobile; Speed Shoes Effect: None (Original); Fast Run Animation: None (Original); Flicker Effects: As Original.
- **Separate progress:** save slots, achievements, unlocks, time-attack records and save states use `Documents/classic/`. Existing Anniversary data stays in its usual location.
- Full-size native menus retain their original aspect ratio, keeping text readable. The 4:3 format applies to game screens and the intro/title sequence.

### Anniversary Mode

Switching back restores widescreen gameplay, normal frame presentation, existing saves and your original visual preferences. The locked Classic preferences do not overwrite them.

### Install

This updates the **regular Sonic 3 AIR app**, using its original bundle identifier. Keep the app's Documents folder when updating. The previous standalone **Sonic 3 AIR 4x3** app is a separate older edition; this release adds the switch to the regular port.

Sign/install **Sonic3AIR-iOS-unsigned.ipa** using your usual method. A compatible Sonic 3 & Knuckles ROM is required and is not included. Both modes share the same ROM and mods folder.

### Validation

Automated checks exercise the actual persistent-data read/write implementation across separate profiles, repeated reloads, Classic presets, square-pixel output and touch mapping, plus existing script-profile and Retina regressions. Native arm64 iOS compilation and bundle checks run in CI. Mode transitions, the title sequence, gameplay, 30 FPS pacing and mod compatibility still need device testing.

[Source](https://github.com/jentisalive/sonic3airios/tree/ios-native) · [Original project](https://sonic3air.org/)

Unofficial, non-profit port of Eukaryot's Sonic 3 A.I.R. GPL-3.0. Sonic the Hedgehog and related properties belong to SEGA. Not affiliated with SEGA or Sonic Team.
