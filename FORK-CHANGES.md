# ScummVM Comfort Edition Changelog

This file tracks user-facing changes maintained in ScummVM Comfort Edition.

Format: `Date/timestamp: Category: Description/Explanation`

Timestamps use ISO 8601 local time with UTC offset.

- 2026-10-02T10:14:12-07:00: UI: Added SDL desktop window layout options for ultra-wide displays, including supporting option handling, window placement logic, documentation, and packaging metadata.
- 2026-10-02T10:14:12-07:00: Cheat: Added Indiana Jones and the Last Crusade game-specific boxing cheat options.
- 2026-10-02T10:14:12-07:00: Cheat: Added `Invincible Indy` to prevent Indy health loss during boxing damage scripts.
- 2026-10-02T10:14:12-07:00: Cheat: Added `Strong punches` to increase damage dealt to the opponent during boxing damage scripts.
- 2026-10-02T10:17:45-07:00: Patch: Updated SCI AVI playback so `enable_hq_video` scales AVI videos to fill the game window vertically while preserving aspect ratio.
- 2026-10-02T10:17:45-07:00: Patch: Left DOS SEQ playback unchanged so videos with baked-in letterbox bars keep their original presentation.
- 2026-10-03T14:32:11-07:00: UI: Added a separate Cheats tab to the launcher game-options dialog and the in-game options dialog.
- 2026-10-03T14:32:11-07:00: UI: Marked cheat options separately from ordinary engine options so gameplay cheats no longer appear on the main Game tab.
- 2026-10-03T14:32:11-07:00: UI: Updated bundled themes so the Cheats tab has matching layout entries.
- 2026-10-03T14:32:11-07:00: Cheat: Added `Disable guard detection` for Operation Stealth to prevent guard and labyrinth failure scripts from killing the player.
- 2026-10-03T14:32:11-07:00: Cheat: Added `Accept any color code` for Operation Stealth so the color copy-protection screen plays normally while any selected colors count as accepted.
- 2026-10-03T14:32:11-07:00: Cheat: Added `Freeze jetski energy` for Operation Stealth to prevent the jetski energy meter from depleting during jetski minigames.
- 2026-10-03T14:32:11-07:00: Cheat: Added `Disable shark collision` for Operation Stealth so the large underwater shark swims through the player without triggering the death sequence.
- 2026-10-03T14:46:21-07:00: UI: Added `Right two-thirds` and `Left two-thirds` windowed layout options.
- 2026-10-03T15:34:57-07:00: Release: Added tagged GitHub Release publishing for Windows x64 portable builds.
- 2026-10-03T15:34:57-07:00: Release: Added release archives containing the executable, required runtime DLLs, ScummVM data files, bundled dependency license notices, Comfort Edition README, upstream ScummVM README, build manifest, and SHA-256 checksum.
- 2026-10-03T15:34:57-07:00: Release: Kept game data out of release archives; users must provide their own supported game files.
- 2026-10-03T20:22:01-07:00: Patch: Added `Patch: EU 256-color/VGA jetski sequence` for the European DOS VGA/256-color release of Operation Stealth.
- 2026-10-03T20:22:01-07:00: Patch: Retired stale Operation Stealth European VGA dock scripts before the jetski minigame so duplicate dock actors do not corrupt the ocean sequence.
- 2026-10-03T20:22:01-07:00: Patch: Enabled the Operation Stealth European VGA jetski sequence patch by default and limited it to the European DOS VGA/256-color version.
- 2026-10-03T20:22:01-07:00: Cheat: Added `Disable rat maze darkness` for Operation Stealth to remove the rat maze darkness overlay while preserving normal movement and collision.
- 2026-10-04T00:19:17-07:00: Cheat: Added Operation Stealth cheat options for the DOS 256-color and Amiga releases.
- 2026-10-04T00:19:17-07:00: Patch: Added `Patch: Amiga text cleanup` for Operation Stealth to hide Amiga inline text layout codes and fix known mixed-language English text leftovers.
- 2026-10-04T00:19:17-07:00: Cheat: Added `Freeze final countdown` for Operation Stealth to prevent the Amiga final timed escape sequence from expiring.
- 2026-10-04T00:48:16-07:00: Patch: Restored music and sound effects for Atari ST Operation Stealth installs that use `MIDI.ON` and H32 sound resources.
- 2026-10-04T08:13:13-07:00: Patch: Added `Patch: Atari ST final room` for Operation Stealth to ignore an invalid `SALLE59.REL` object-table load that corrupted Dr. Why, razor, and cigarette state.
- 2026-10-04T08:20:27-07:00: UI: Limited `Freeze final countdown` to Amiga Operation Stealth entries, matching the cheat's Amiga-only runtime behavior.
- 2026-10-04T08:37:04-07:00: UI: Filtered Operation Stealth platform-specific patch options so Amiga text cleanup, Amiga final countdown, Atari ST final room, and European VGA jetski patches only appear on their matching detected versions.
