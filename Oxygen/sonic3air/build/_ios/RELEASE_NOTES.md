<div align="center">

## Sonic 3 A.I.R. for iOS · Classic 4:3

**A separate classic-format edition · v0.1.4**

[Download the 4:3 IPA](https://github.com/jentisalive/sonic3airios/releases/download/ios-v0.1.4-4x3/Sonic3AIR-iOS-4x3-unsigned.ipa)

</div>

### Display

- Gameplay uses a classic **320 × 224** field, presented at **4:3** with pixel-aspect correction.
- The picture is centered with black bars on wider screens. The 4:3 edition keeps this format regardless of saved scaling settings.
- Native menus retain their full **400 × 224** layout and are scaled into the same 4:3 frame, so text and buttons are not cropped. Menus will look narrower than in the widescreen edition.
- Retina output and touch-coordinate conversion are retained.

### Install separately

The app is named **Sonic 3 AIR 4x3** and has a separate bundle identifier. It can coexist with the widescreen edition and has its own ROM, settings, saves and mods. Select your compatible Sonic 3 & Knuckles ROM on first launch; no ROM is included.

Download **Sonic3AIR-iOS-4x3-unsigned.ipa** under Assets, then sign/install it using your usual method. To copy existing saves or mods, use **Files → On My iPhone / iPad → Sonic 3 AIR 4x3**.

### Validation

CI builds the native arm64 app and checks its resources, separate identity and bundled classic resolution. Automated tests cover 4:3 letterboxing and input mapping at phone/tablet sizes, plus the existing script-profile and Retina mapping regressions. Visual presentation and gameplay still need testing on a physical device.

[Source](https://github.com/jentisalive/sonic3airios/tree/ios-native-4x3) · [Widescreen release](https://github.com/jentisalive/sonic3airios/releases/tag/ios-v0.1.3)

Unofficial, non-profit port of Eukaryot's Sonic 3 A.I.R. GPL-3.0. Sonic the Hedgehog and related properties belong to SEGA. Not affiliated with SEGA or Sonic Team.
