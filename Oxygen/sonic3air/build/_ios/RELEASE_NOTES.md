# Sonic 3 A.I.R. for iOS · v0.2.7

## Replay crash fix

- Confirmed and fixed an engine VM restore error: a saved call frame identified
  only a function name/signature, silently resolving an overridden/base function to
  the highest-priority implementation. The active base-call depth was also lost.
- Exact runtime snapshots now store the implementation index, base-call depth and
  bytecode fingerprint for each frame, retaining original caller PCs instead of
  guessing another call site. Runtime loads validate into temporary storage before
  replacing the live call stack; frame/local/value-stack sizes are bounded.
- Replay restoration resumes the saved VM without pushing script Init again, which
  previously reset fade/cutscene globals on top of the restored game.
- Replay playback now uses the normal simulation-state loader, resetting its
  clocks, frame counters, rewind and recorder state. Failed initial loads rebuild
  the menu simulation; failed in-run checkpoint loads stop before another script
  frame can execute. Added diagnostic logging around playback/state rejection.
- Binary primitive reads/writes use alignment-safe copies instead of casting byte
  buffers to wider pointers; the new VM regression exposed these accesses under UBSan.
- Script compatibility checks now hash actual compiled opcodes. The previous
  getCompiledCodeHash hook is disabled in release builds and returned zero.

## Existing recordings

**Create new replays after updating.** Format 1/2 recordings from 0.2.5/0.2.6 do
not contain enough call-frame identity to restore safely. They remain listed and
can be deleted, but attempting to play them shows a clear legacy-recording message.
They are never rewritten or deleted automatically. Format 3 recordings use exact
runtime v2 / Oxygen state v8; older app versions cannot load the new state format.
Normal progress, shared/Classic save profiles and mods are preserved.

Validation includes a regression that fails on the old loader (wrong function
restored despite a success result), exact overrides/base calls and indirect caller
PCs, atomic rejection of truncated/malformed states, old ambiguous-state rejection,
and stack allocation growth. Existing profile/storage/menu/achievement/replay-codec
checks and native iOS CI also run. Physical iPhone playback still needs validation.

Experimental unsigned sideloaded IPA. App version 0.2.7, build 13.
