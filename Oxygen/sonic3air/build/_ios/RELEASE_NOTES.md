<div align="center">

## Sonic 3 A.I.R. for iOS · v0.2.4

**53 more challenges, built into the native port**

[Download unsigned IPA](https://github.com/jentisalive/sonic3airios/releases/download/ios-v0.2.4/Sonic3AIR-iOS-unsigned.ipa)

</div>

### Achievements

- Added all **53 challenges** from the [RetroAchievements Sonic 3 & Knuckles base set](https://retroachievements.org/game/4874) as local AIR achievements. The original 18 AIR achievements remain available.
- Find them in **Extras → Achievements**, with English descriptions, local point labels and the existing unlock notifications.
- Covers zone clears, character endings, emeralds, rings, lives, secret areas, bonus stages, five Competition tracks and Grand Prix.
- Restricted challenges track damage (including shield loss), death, transformations, bonus entries, Left input and pausing where applicable. Merely loading a checkpoint does not complete these challenges.
- Unlocks use the existing **separate Classic and Anniversary save profiles**. New achievements do not change the original AIR secret-unlock thresholds.
- Long descriptions wrap within each menu width, and achievement cards grow to fit the text.

### Local progress

This is an **offline adaptation to AIR's native events**, with no RetroAchievements account connection or online point submission. Normal Game awards the adventure challenges; Competition Mode awards the race challenges. AIR Act Select and AIR Time Attack are excluded. Using Level Select or Debug blocks other imported unlocks for the current session. Loading a save state cancels restricted attempts.

Original challenge design: **Totaya, blazethecat and RetroAchievements contributors**. Descriptions are rewritten for this port; artwork uses AIR's existing achievement images.

### Install & validation

Install **Sonic3AIR-iOS-unsigned.ipa** over the regular port and keep Documents. Your compatible Sonic 3 & Knuckles ROM is required and is not included.

Automated tests exercise all 53 award paths, failure conditions and timing boundaries. Existing save-profile, scanline-safety, Retina and compact-menu checks are retained. CI compiles the native arm64 app and verifies its bundle. Full gameplay and achievement notifications still need verification on an iPhone.

The Classic presentation and menu layout from v0.2.3 are retained.

[Source](https://github.com/jentisalive/sonic3airios/tree/ios-native) · [Original project](https://sonic3air.org/)

Unofficial port of Eukaryot's Sonic 3 A.I.R. GPL-3.0. Sonic the Hedgehog and related properties belong to SEGA. Not affiliated with SEGA or Sonic Team.
