# Experimental native iOS port

This target compiles the C++ Oxygen Engine, LemonScript interpreter and SDL2 for
arm64 iOS. It does not use WebKit, WebView, Emscripten or a web server.

Status: the user confirmed version 0.1.1 launches and plays on iPhone. Version
0.1.2 enables Retina output to address blurred low-resolution presentation.
The Retina change still needs device testing; not all features/mods are verified.

## Download

The unsigned IPA is published as a prerelease under **Releases**, tag
`ios-v0.1.2`. The Actions artifact remains available as well. Version 0.1.2
enables native Retina backing pixels for window viewports, clipping and final
upscaling while keeping logical layout/input and offscreen texture sizes intact.
Version 0.1.1 fixed the script entry point and added iPhone/iPad icons.

## Build from Windows using GitHub Actions

1. Open the repository's **Actions** tab. If GitHub has disabled workflows for the
   new fork, enable them with **I understand my workflows, go ahead and enable them**.
2. Select **Native iOS IPA**, then **Run workflow** on the `ios-native` branch
   (or `main` after merging the changes).
3. Download the `Sonic3AIR-iOS-unsigned` artifact from the completed run and unzip
   it to obtain `Sonic3AIR-iOS-unsigned.ipa`.
4. Sign/install the IPA with your compatible iOS installation method. The archive
   is unsigned; it is not an App Store or TestFlight release. This workflow does
   not require Apple signing certificates or a ROM in GitHub secrets.

On first launch, choose your compatible, unmodified Sonic 3 & Knuckles ROM from
Files. The app validates it with the same checksum and compatibility byte as the
engine and keeps a copy in Documents. No ROM is included or downloaded.

Files → On My iPhone → Sonic 3 AIR contains the `mods` folder, settings, saves and
`logfile.txt`. Unzip mods into `mods` using the usual Sonic 3 A.I.R. layout.
The engine's existing mobile touch controls are used.

## Local build on macOS

```sh
cmake -S Oxygen/sonic3air/build/_ios -B build-ios -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO
cmake --build build-ios --config Release --parallel 3
mkdir -p ipa/Payload
cp -R build-ios/Release-iphoneos/Sonic3AIR.app ipa/Payload/
(cd ipa && zip -qry ../Sonic3AIR-iOS-unsigned.ipa Payload)
```

## Changes from the unfinished upstream iOS target

- An independent CMake target builds all vendored dependencies for the iOS SDK;
  it avoids the old Xcode project's hand-maintained source lists.
- SDL's UIKit application lifecycle runs the native entry point.
- Native ROM selection uses UIDocumentPicker; an invalid ROM can be replaced
  before the engine starts.
- Screen rendering binds SDL's UIKit framebuffer, rather than framebuffer zero.
  The handle is queried again for each use so it can change after rotation.
- The render-method limit is OpenGL ES presentation with the software game
  renderer, matching the existing GLES-compatible path.
- Read-only assets and script sources live in the app bundle. Settings, mods,
  save states and script-cache writes use Documents.
- Rendering pauses while iOS marks the app minimized; settings are saved on
  entering the background.
- The bundle declares the minimum iOS version and iPhone/iPad device families,
  including when built with Ninja instead of Xcode.
- CI packages a device build into the required `Payload/*.app` IPA structure.

## Current limits and testing

The editing environment has no physical iPhone. Compilation runs on a macOS
GitHub runner with the iOS SDK; portable changes also pass a Linux C++ syntax
check. Test the IPA on-device. In
particular, validate first-launch ROM selection, landscape rotation, multitouch,
audio interruption/background resume, saves and representative mods.

The initial bundle includes source assets and the remastered soundtrack. Script
compilation on first launch can take longer than an official packaged release.
Automatic downloading/updating is unavailable in this iOS target. Simulator
builds and App Store distribution are outside this initial workflow.

Original project credits and GPL-3.0 license remain in the repository root.
