# Sonic 3 A.I.R. for Windows x64

This experimental desktop build includes Classic and Anniversary modes,
shared saves, Speedrun Mode, and act and full zone replays.

1. Download `Sonic3AIR-Windows-x64.zip` from the [Windows release](https://github.com/jentisalive/sonic3airios/releases/tag/windows-v0.2.7).
2. Extract the entire ZIP into a folder you can write to and run `Sonic3AIR.exe`.
3. If prompted, select your own compatible Sonic 3 & Knuckles ROM.

The `savedata` folder next to the executable contains your settings, progress,
mods and replays. Keep it when updating. Find edition, shared saves and replay
recording options in the game menu, and play recordings from **Extras → Replays**.
Recordings from port versions 0.2.5 and 0.2.6 need to be recorded again.

To build from source, open Developer PowerShell for Visual Studio 2022 with the
Desktop development with C++ workload installed. From the repository root, run
`./Oxygen/sonic3air/build/_windows/build.ps1`. The ZIP goes to `dist-windows`.
GitHub Actions also builds the ZIP from the vendored dependencies and checks that
the packaged executable starts without a missing DLL.
