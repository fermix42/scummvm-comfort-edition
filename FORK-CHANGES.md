# ScummVM Comfort Edition Changes

This file tracks user-facing changes maintained in ScummVM Comfort Edition.

## Current custom changes

### Ultra-wide interface

- Added SDL desktop window layout options for ultra-wide displays.
- Added the custom `Right two-thirds` and `Left two-thirds` windowed layout option alongside the existing layout choices.
- Updated the related SDL option handling, window placement logic, documentation, and project packaging metadata.

### SCI Windows AVI scaling

- Updated SCI AVI playback so the `enable_hq_video` option scales AVI videos to fill the game window vertically while preserving aspect ratio.
- This is a general SCI AVI playback behavior.
- Left DOS SEQ playback alone so videos with baked-in letterbox bars keep their original presentation.

### Indiana Jones and the Last Crusade boxing cheats

- Added game-specific cheat options for Indiana Jones and the Last Crusade.
- `Invincible Indy` prevents Indy health loss during boxing damage scripts.
- `Strong punches` increases damage dealt to the opponent during boxing damage scripts.
- The implementation is based on the decompiled boxing scripts.

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
