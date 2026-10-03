# ScummVM Private Fork Changes

This file tracks user-facing changes maintained in this private fork.

## Current custom changes

### Ultra-wide interface

- Added SDL desktop window layout options for ultra-wide displays.
- Restored the custom `Right two-thirds` windowed layout option alongside the existing layout choices.
- Updated the related SDL option handling, window placement logic, documentation, and project packaging metadata.

### SCI Windows AVI scaling

- Updated SCI AVI playback so the `enable_hq_video` option scales AVI videos to fill the game window vertically while preserving aspect ratio.
- This is a general SCI AVI playback behavior, not a Gabriel Knight 1-only patch. Gabriel Knight 1 Windows CD was the test case that exposed the issue.
- Left DOS SEQ playback alone so videos with baked-in letterbox bars keep their original presentation.

### Indiana Jones and the Last Crusade boxing cheats

- Added game-specific cheat options for Indiana Jones and the Last Crusade.
- `Invincible Indy` prevents Indy health loss during boxing damage scripts.
- `Strong punches` increases damage dealt to the opponent during boxing damage scripts.
- The implementation is based on the decompiled boxing scripts and preserves the original movement and punch state instead of modifying opponent positioning state.

### Dedicated cheats tab

- Added a separate Cheats tab to the launcher game-options dialog and the in-game options dialog.
- Marked cheat options separately from ordinary engine options so gameplay cheats no longer clutter the main Game tab.
- Updated all bundled themes so the new Cheats tab has matching layout entries.

### Operation Stealth cheats

- Added Operation Stealth cheat options for the DOS 256-color release.
- `Disable guard detection` prevents guard/labyrinth failure scripts from killing the player.
- `Accept any color code` lets the color copy-protection screen play normally while counting any selected colors as accepted.
- `Freeze jetski energy` prevents the jetski energy meter from depleting during the jetski minigames.
- `Disable shark collision` lets the large underwater shark swim through the player without triggering the death sequence.

### Local build tooling

- Added a clean Release build script for this Windows checkout so builds do not depend on the ambient shell `PATH`.
- Added a separate trace Release build script that produces `scummvm-trace.exe`, enables Cine script dumping/tracing, and records source hashes for trace diagnostics.
- Added a local Operation Stealth Cine script extractor that writes decompiled PRC/REL scripts plus a hash manifest for repeatable script analysis.
