# Escape from Monkey Island Windows play-session saves

Each CE play session has the Windows game's original 160 save slots: 20 pages
of eight slots. The game's Save, Load and Erase menus show only the active
session's saves. ScummVM's load chooser groups saves through their existing
CE sidecars and lists each session in native slot order, including legacy files.

Existing saves are not moved or converted. Saves without session ownership
remain in the chooser's Untracked saves group. Loading a session-owned save
restores its play-session association through the normal CE save hooks.

The game scripts still use `efmi001.gsv` through `efmi160.gsv`. CE resolves
those names to a target-specific session bank. New filenames have this shape:

```
efmi-win-ce-<target token>-<session token>-slot001.gsv
```

The tokens are portable filename hashes; full session IDs remain in the
sidecars. An existing file is reused only when its sidecar identifies the same
session. If a bank filename already exists without matching ownership, a
`-copy1` suffix (or the next unused copy number) preserves it.

The save format, save descriptions, screenshots, page layout, and native slot
selection remain unchanged. No replacement Lua scripts or game assets are
installed. Other games and other MI4 platforms retain their existing handling.

## Implementation boundaries

`engines/grim/ce_emi_windows_saves.h` owns Windows bank names, native aliases,
ownership resolution and slot ordering. Small hooks in Grim's engine,
metaengine and Lua save APIs connect that helper to the existing CE sidecars.
Internal renderer checkpoints and files outside the 160 player slots do not
change the active play session.

The ScummVM chooser uses indices in its complete file list to distinguish
multiple sessions' copies of one native slot. Those indices are resolved to
exact filenames before loading, deleting or looking up sidecars; the native
game continues to use its original slot numbers.

The chooser retains that filename snapshot until its next refresh, including
when sidecars are cleared during session deletion. MI4 Windows also advertises
the compact-load-list capability, so the shared chooser does not insert blank
rows for another session's IDs. Other targets keep the previous list behavior.

The load chooser supports gallery mode using the existing SIMG screenshot in
Windows saves. The CE-owned metadata reader restores the game's stretched
256x128 RGB565 texture to a 160x120 thumbnail and reads its original SUBS
description. Missing or malformed images leave the save available without a
thumbnail. Session filtering and slot ordering are the same in both views.

## Regression check

With a built executable, a Windows game-data directory and a compatible
Windows save, run:

```
python devtools/ce/test_mi4_windows_save_banks.py --exe <scummvm.exe> --game <Windows game directory> --save <existing Windows .gsv>
```

The test uses temporary configurations, callbacks and save directories. It
checks all 160 slots in two sessions, native enumeration and metadata, native
and launcher loads, mixed legacy/banked ordering, lost-sidecar preservation,
and native erase isolation. It never changes the input save or contacts the
achievement server. Manual testing remains necessary for the visible menus.
