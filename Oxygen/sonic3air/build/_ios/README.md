# Classic 4:3 edition for iOS

This branch builds **Sonic 3 AIR 4x3**, a separate native iPhone/iPad app.

- Gameplay: 320 × 224, presented at 4:3 with pixel-aspect correction.
- Native menus: full 400 × 224 layout, scaled into the 4:3 frame without cropping.
- Centered picture, black bars on wider screens, Retina backing pixels.
- Separate bundle ID: `org.sonic3air.native-ios.classic43`.
- Separate Documents folder, ROM import, saves, settings and mods.

Install `Sonic3AIR-iOS-4x3-unsigned.ipa` from the `ios-v0.1.4-4x3` release using your signing method. The existing widescreen app can remain installed. Copy saves/mods manually if needed, and choose your compatible ROM on first launch.

Files path: **On My iPhone / iPad → Sonic 3 AIR 4x3**.

Build through **Actions → Native iOS IPA - Classic 4x3** on branch `ios-native-4x3`, or configure the `_ios` CMake target on macOS using the iOS SDK. This branch enables `SONIC3AIR_IOS_CLASSIC_ASPECT` by default. The workflow packages an unsigned arm64 device build.

The classic format is locked in this edition; scaling options cannot stretch it to fullscreen. Mods that change screen size may need adaptation. Device testing remains necessary for visual quality, controls, gameplay and mod compatibility.

See [release notes](RELEASE_NOTES.md) for details. Original credits and GPL-3.0 license apply.
