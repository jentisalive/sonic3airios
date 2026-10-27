<div align="center">

## Sonic 3 A.I.R. for iOS · v0.2.1

**Classic Mode: memory safety & complete 4:3 menus**

[Download unsigned IPA](https://github.com/jentisalive/sonic3airios/releases/download/ios-v0.2.1/Sonic3AIR-iOS-unsigned.ipa)

</div>

### Fixed zone crashes

Version 0.2.0 incorrectly used a 240-line game field. The original horizontal-scroll buffer has room for **224 lines**, ending at `0xffffe37f`; the dynamic-object list starts immediately afterward at `0xffffe380`. Extra scroll writes could corrupt that list while moving through AIZ and other zones.

- Restored the supported **320 × 224** Classic simulation field.
- Constrained Classic resolution changes at the video and script interfaces.
- Bounded the native scroll writer to the real buffer size as an additional safeguard.
- Removed the additional gameplay area at the bottom of the screen. Intro and gameplay use the original 224-line field with Genesis-style pixel-aspect correction for 4:3 display.

### Full 4:3 menus

- Native menus now render into a separate **400 × 300 presentation canvas**, with square-pixel text and UI. The simulation remains 224 lines high.
- Reflowed the main-menu rows, scrolling areas, backgrounds and footer positions for the taller menu layout.
- All menu screens use a centered 4:3 output frame. They are not cropped from a narrow framebuffer or horizontally squeezed from a wide menu.
- The animated decorative backdrop fills the canvas; its logo is rendered separately to preserve its proportions.
- Locked Classic options show their actual preset values instead of “not available”.

### Preserved

**Classic Mode / Anniversary Mode** remains below Mods. Switching returns to the intro/title screen. Classic retains its 30 FPS presentation cap, normal-speed 60 Hz logic, fixed visual settings and separate progress under **Documents/classic/**. Anniversary preferences and saves remain in their existing locations. Both modes share ROM and mods.

### Install & validation

Install **Sonic3AIR-iOS-unsigned.ipa** over the regular Sonic 3 AIR app, preserving Documents. Your compatible Sonic 3 & Knuckles ROM is required and is not included.

Regression tests exercise the scroll-buffer boundary, oversized resolution requests, the independent save profiles, 4:3 menu geometry and input mapping. CI compiles the arm64 iOS app and checks the bundle. A physical-device replay of the affected AIZ section, other zones and menu transitions is still needed.

[Source](https://github.com/jentisalive/sonic3airios/tree/ios-native) · [Original project](https://sonic3air.org/)

Unofficial, non-profit port of Eukaryot's Sonic 3 A.I.R. GPL-3.0. Sonic the Hedgehog and related properties belong to SEGA. Not affiliated with SEGA or Sonic Team.
