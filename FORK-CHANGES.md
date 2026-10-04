# ScummVM Comfort Edition Changes

This file tracks user-facing changes maintained in ScummVM Comfort Edition.

Timestamps use ISO 8601 local time with UTC offset.

## Binary releases

Timestamp: 2026-10-03T15:34:57-07:00

- Windows x64 portable builds are published from tagged GitHub Releases for users who cannot or do not want to build ScummVM Comfort Edition from source.
- Release archives include the executable, required runtime DLLs, ScummVM data files, bundled dependency license notices, the Comfort Edition README, the upstream ScummVM README, a build manifest, and a SHA-256 checksum.
- Game data is never included. Users still need their own supported game files.
- The source tree remains the canonical form for review and modification; binary releases are convenience packages built from tagged commits.

## Current custom changes

### Ultra-wide interface

Timestamp: 2026-10-02T10:14:12-07:00

- 2026-10-02T10:14:12-07:00: Added SDL desktop window layout options for ultra-wide displays.
- 2026-10-03T14:46:21-07:00: Added the custom `Right two-thirds` and `Left two-thirds` windowed layout option alongside the existing layout choices.
- 2026-10-02T10:14:12-07:00: Updated the related SDL option handling, window placement logic, documentation, and project packaging metadata.

### SCI Windows AVI scaling

Timestamp: 2026-10-02T10:17:45-07:00

- 2026-10-02T10:17:45-07:00: Updated SCI AVI playback so the `enable_hq_video` option scales AVI videos to fill the game window vertically while preserving aspect ratio.
- 2026-10-03T14:45:37-07:00: This is a general SCI AVI playback behavior.
- 2026-10-02T10:14:12-07:00: Left DOS SEQ playback alone so videos with baked-in letterbox bars keep their original presentation.

### Indiana Jones and the Last Crusade boxing cheats

Timestamp: 2026-10-02T10:14:12-07:00

- 2026-10-02T10:14:12-07:00: Added game-specific cheat options for Indiana Jones and the Last Crusade.
- 2026-10-02T10:14:12-07:00: `Invincible Indy` prevents Indy health loss during boxing damage scripts.
- 2026-10-02T10:14:12-07:00: `Strong punches` increases damage dealt to the opponent during boxing damage scripts.
- 2026-10-03T14:45:37-07:00: The implementation is based on the decompiled boxing scripts.

### Dedicated cheats tab

Timestamp: 2026-10-03T14:32:11-07:00

- 2026-10-03T14:32:11-07:00: Added a separate Cheats tab to the launcher game-options dialog and the in-game options dialog.
- 2026-10-03T14:32:11-07:00: Marked cheat options separately from ordinary engine options so gameplay cheats no longer clutter the main Game tab.
- 2026-10-03T14:32:11-07:00: Updated all bundled themes so the new Cheats tab has matching layout entries.

### Operation Stealth cheats

Timestamp: 2026-10-04T00:19:17-07:00

- 2026-10-04T00:19:17-07:00: Added Operation Stealth cheat options for the DOS 256-color and Amiga releases.
- 2026-10-03T20:22:01-07:00: Confirmed by completing the European VGA version with all Operation Stealth cheats enabled and the European VGA jetski patch enabled.
- 2026-10-03T14:32:11-07:00: `Disable guard detection` prevents guard/labyrinth failure scripts from killing the player.
- 2026-10-03T14:32:11-07:00: `Accept any color code` lets the color copy-protection screen play normally while counting any selected colors as accepted.
- 2026-10-03T14:32:11-07:00: `Freeze jetski energy` prevents the jetski energy meter from depleting during the jetski minigames.
- 2026-10-03T14:32:11-07:00: `Disable shark collision` lets the large underwater shark swim through the player without triggering the death sequence.
- 2026-10-03T20:22:01-07:00: `Disable rat maze darkness` removes the darkness overlay from the rat maze while preserving normal maze movement and collision.
- 2026-10-04T00:19:17-07:00: `Patch: Amiga text cleanup` hides Amiga inline text layout codes and fixes known mixed-language English text leftovers.
- 2026-10-04T00:19:17-07:00: `Freeze final countdown` prevents the Amiga final timed escape sequence from expiring.
- 2026-10-04T00:19:17-07:00: `Click to advance cutscenes` lets timed Operation Stealth cutscene captions wait for a player click before advancing.

### Operation Stealth European VGA jetski patch

Timestamp: 2026-10-03T20:22:01-07:00

- 2026-10-03T20:22:01-07:00: Added `Patch: EU 256-color/VGA jetski sequence`, a game option for the European DOS VGA/256-color release of Operation Stealth.
- 2026-10-03T20:22:01-07:00: The patch prevents duplicated dock sequence actors and retires stale dock scripts before the jetski minigame so the player jetski does not drift left or reload the dock scene over the ocean.
- 2026-10-03T20:22:01-07:00: The option is enabled by default and is limited to the European DOS VGA/256-color version.
- 2026-10-03T20:22:01-07:00: Confirmed by completing the European VGA version with the patch enabled.
