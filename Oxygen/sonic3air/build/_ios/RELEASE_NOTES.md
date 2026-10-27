<div align="center">

## Sonic 3 A.I.R. for iOS · v0.2.3

**Original menu spacing, shifted left for Classic Mode**

[Download unsigned IPA](https://github.com/jentisalive/sonic3airios/releases/download/ios-v0.2.3/Sonic3AIR-iOS-unsigned.ipa)

</div>

### Classic menu layout

- Removed the taller 400 × 300 menu canvas. Classic gameplay and menus now share the original **320 × 224** field with 4:3 output.
- Restored the original main-menu row positions and moved the entries **80 pixels left**. Classic Mode / Anniversary Mode remains below Mods.
- Moved submenu column centers, controls and footers inward for the narrower field. Options use the compact font so labels and values fit.
- Retained the original background style and adjusted the left panel and logo to fit without cropping the menu text.

### Safety and progress

The scroll-buffer overflow fix is retained: Classic stays at **224 scanlines**, so the dynamic-object list is not overwritten. There are no extra gameplay rows at the bottom. The **30 FPS** presentation cap, fixed Classic preferences, separate saves and Anniversary preferences are preserved.

### Install & testing

Install **Sonic3AIR-iOS-unsigned.ipa** over the regular Sonic 3 AIR app, keeping Documents. Your compatible Sonic 3 & Knuckles ROM is required and is not included.

Checks cover save-profile isolation, the scroll/object memory boundary, 224-line screen limits, coordinate mapping and main-menu/Options font bounds using the actual font metadata. CI builds the native arm64 app and verifies its bundle. The compact layout and the affected AIZ section still need verification on a device.

[Source](https://github.com/jentisalive/sonic3airios/tree/ios-native) · [Original project](https://sonic3air.org/)

Unofficial, non-profit port of Eukaryot's Sonic 3 A.I.R. GPL-3.0. Sonic the Hedgehog and related properties belong to SEGA. Not affiliated with SEGA or Sonic Team.
