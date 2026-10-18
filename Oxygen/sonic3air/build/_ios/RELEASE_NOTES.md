Experimental native Sonic 3 A.I.R. for iPhone and iPad, without WebView.

- Added English mod installation instructions to the empty Mods screen.
- Added **Open mods folder**, opening the native Files picker with the engine's actual `Documents/mods` folder as its starting location.
- The folder is created automatically if missing. Browsing does not import or replace any files.
- Extract each mod into its own folder inside `mods`, then restart the game to scan for new mods.
- Includes the Retina rendering and app icons from the previous releases.

Download **Sonic3AIR-iOS-unsigned.ipa** below and sign/install it using your usual method. Your compatible Sonic 3 & Knuckles ROM is required and is not included.

The iOS SDK build and automated bundle checks run in CI. Opening the folder and managing mods still need verification on an actual iPhone/iPad. iOS controls the Files picker's starting location; if it restores another location, browse to **On My iPhone / iPad > Sonic 3 AIR > mods**.

Source tag: `ios-v0.1.3`. Project license: GPL-3.0.
