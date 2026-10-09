# ScummVM Comfort Edition Changelog

This file tracks user-facing changes maintained in ScummVM Comfort Edition.

Format: `Date/timestamp: Category: Description/Explanation`

Timestamps use ISO 8601 local time with UTC offset. Entries are newest first.

- 2026-10-09T13:06:04-07:00: Patch: Added `Patch: Crop Cine display borders` for Future Wars and Operation Stealth to trim Cine's top presentation padding before fit-to-window scaling while preserving the bottom status/action text area, with mouse coordinates translated back to the original `320x200` game space.
- 2026-10-09T12:53:30-07:00: Docs: Documented that Cine/Operation Stealth can include black space inside the engine's `320x200` game surface, separate from backend fit-to-window side bars.
- 2026-10-09T11:44:29-07:00: Achievements: Added configurable Comfort Edition achievement popup cards with icon/title-only display, popup location choices, optional voice alert playback from `ce-achievements.dat`, challenge-mode accent color, longer card visibility, compact width sizing, and updated shipped theme packages.
- 2026-10-09T09:22:37-07:00: Achievements: Added Comfort Edition achievement unlock tracking, local queueing, link-token sync controls, repeat-popup control, and Challenge Mode controls in Global Options.
- 2026-10-09T09:22:37-07:00: Achievements: Added Operation Stealth achievement event watchers and catalog/rule support for Comfort Edition achievement unlocks.
- 2026-10-09T09:22:37-07:00: UI: Updated bundled themes and embedded fallback theme data so the Global Options Achievements tab is available across shipped themes.
- 2026-10-09T09:22:37-07:00: Docs: Added Comfort Edition achievement support-pack install directions and example catalog documentation; internal decompiled atlas files remain local-only and ignored.
- 2026-10-09T09:22:37-07:00: UI: Clarified that ultra-wide window layout places the ScummVM window only; Fit to window and aspect correction still control game-image scaling.
- 2026-10-09T09:22:37-07:00: Patch: Clarified SCI high-quality video scaling text and documented that SCI AVI fitting is tied to `enable_hq_video` while other SCI video paths keep their own sizing.
- 2026-10-09T01:58:00-07:00: UI: Added a per-game master `Enable cheats` gate to the Cheats tab; Challenge Mode keeps individual cheat selections visible but inactive, and enabling cheats requires confirmation because it disables Challenge Mode and challenge/l33t credit.
- 2026-10-09T01:29:15-07:00: Achievements: Added `ce-achievements.dat` loading for the Comfort Edition achievement metadata and icon pack so website-downloaded updates can be installed in ScummVM's extra path.
- 2026-10-04T16:08:00-07:00: Release: Verified the Windows Release x64 build with the Deja Vu Macintosh patches and launcher Cheats tab wiring.
- 2026-10-04T14:21:01-07:00: Cheat: Added `Mugger won't kill` for Deja Vu Macintosh to keep the mugger encounter active while preventing repeated punches from advancing the mugger counter into the death branch.
- 2026-10-04T13:54:28-07:00: Cheat: Added `Unlimited inventory` for Deja Vu Macintosh to allow moves into the trench coat when only the normal capacity check would reject them.
- 2026-10-04T13:34:00-07:00: Cheat: Added `No alligators` for Deja Vu Macintosh to prevent the random sewer alligator encounter from spawning.
- 2026-10-04T13:34:00-07:00: Cheat: Added `Unlimited ammo` for Deja Vu Macintosh so gunshots keep their loaded cartridge while preserving the normal shot handling.
- 2026-10-04T12:55:42-07:00: Patch: Fixed Deja Vu Macintosh save loading so restored console history opens at the current bottom state instead of requiring `Click to continue` through the whole transcript.
- 2026-10-04T12:38:09-07:00: Cheat: Added `Rig slot machine` for Deja Vu Macintosh to redirect the first-pull loser script and force the slot machine roll down the existing jackpot branch.
- 2026-10-04T11:59:05-07:00: Cheat: Added `Freeze police timer` for Deja Vu Macintosh to prevent the police arrest timer from reaching the game-over state.
- 2026-10-04T11:40:46-07:00: Patch: Fixed Deja Vu Macintosh `Click to continue` paging so long intro text starts at the first hidden line instead of skipping ahead.
- 2026-10-04T11:40:46-07:00: Patch: Fixed a Deja Vu Macintosh startup crash caused by decoding the raw title resource with the wrong image path.
- 2026-10-04T11:40:46-07:00: Patch: Fixed Deja Vu Macintosh title-screen loading from raw resource-fork data.
- 2026-10-04T11:40:46-07:00: Patch: Added Deja Vu Macintosh filename fallbacks for extracted installs that use plain ASCII names instead of the original accented resource filenames.
- 2026-10-04T11:40:46-07:00: Patch: Added detection for the 1993 Macintosh rerelease of Deja Vu: A Nightmare Comes True.
- 2026-10-04T08:37:04-07:00: UI: Filtered Operation Stealth platform-specific patch options so Amiga text cleanup, Amiga final countdown, Atari ST final room, and European VGA jetski patches only appear on their matching detected versions.
- 2026-10-04T08:20:27-07:00: UI: Limited `Freeze final countdown` to Amiga Operation Stealth entries, matching the cheat's Amiga-only runtime behavior.
- 2026-10-04T08:13:13-07:00: Patch: Added `Patch: Atari ST final room` for Operation Stealth to ignore an invalid `SALLE59.REL` object-table load that corrupted Dr. Why, razor, and cigarette state.
- 2026-10-04T00:48:16-07:00: Patch: Restored music and sound effects for Atari ST Operation Stealth installs that use `MIDI.ON` and H32 sound resources.
- 2026-10-04T00:19:17-07:00: Cheat: Added `Freeze final countdown` for Operation Stealth to prevent the Amiga final timed escape sequence from expiring.
- 2026-10-04T00:19:17-07:00: Patch: Added `Patch: Amiga text cleanup` for Operation Stealth to hide Amiga inline text layout codes and fix known mixed-language English text leftovers.
- 2026-10-04T00:19:17-07:00: Cheat: Added Operation Stealth cheat options for the DOS 256-color and Amiga releases.
- 2026-10-03T20:22:01-07:00: Cheat: Added `Disable rat maze darkness` for Operation Stealth to remove the rat maze darkness overlay while preserving normal movement and collision.
- 2026-10-03T20:22:01-07:00: Patch: Enabled the Operation Stealth European VGA jetski sequence patch by default and limited it to the European DOS VGA/256-color version.
- 2026-10-03T20:22:01-07:00: Patch: Retired stale Operation Stealth European VGA dock scripts before the jetski minigame so duplicate dock actors do not corrupt the ocean sequence.
- 2026-10-03T20:22:01-07:00: Patch: Added `Patch: EU 256-color/VGA jetski sequence` for the European DOS VGA/256-color release of Operation Stealth.
- 2026-10-03T15:34:57-07:00: Release: Kept game data out of release archives; users must provide their own supported game files.
- 2026-10-03T15:34:57-07:00: Release: Added release archives containing the executable, required runtime DLLs, ScummVM data files, bundled dependency license notices, Comfort Edition README, upstream ScummVM README, build manifest, and SHA-256 checksum.
- 2026-10-03T15:34:57-07:00: Release: Added tagged GitHub Release publishing for Windows x64 portable builds.
- 2026-10-03T14:46:21-07:00: UI: Added `Right two-thirds` and `Left two-thirds` windowed layout options.
- 2026-10-03T14:32:11-07:00: Cheat: Added `Disable shark collision` for Operation Stealth so the large underwater shark swims through the player without triggering the death sequence.
- 2026-10-03T14:32:11-07:00: Cheat: Added `Freeze jetski energy` for Operation Stealth to prevent the jetski energy meter from depleting during jetski minigames.
- 2026-10-03T14:32:11-07:00: Cheat: Added `Accept any color code` for Operation Stealth so the color copy-protection screen plays normally while any selected colors count as accepted.
- 2026-10-03T14:32:11-07:00: Cheat: Added `Disable guard detection` for Operation Stealth to prevent guard and labyrinth failure scripts from killing the player.
- 2026-10-03T14:32:11-07:00: UI: Updated bundled themes so the Cheats tab has matching layout entries.
- 2026-10-03T14:32:11-07:00: UI: Marked cheat options separately from ordinary engine options so gameplay cheats no longer appear on the main Game tab.
- 2026-10-03T14:32:11-07:00: UI: Added a separate Cheats tab to the launcher game-options dialog and the in-game options dialog.
- 2026-10-02T10:17:45-07:00: Patch: Left DOS SEQ playback unchanged so videos with baked-in letterbox bars keep their original presentation.
- 2026-10-02T10:17:45-07:00: Patch: Updated SCI AVI playback so `enable_hq_video` scales AVI videos to fill the game window vertically while preserving aspect ratio.
- 2026-10-02T10:14:12-07:00: Cheat: Added `Strong punches` to increase damage dealt to the opponent during boxing damage scripts.
- 2026-10-02T10:14:12-07:00: Cheat: Added `Invincible Indy` to prevent Indy health loss during boxing damage scripts.
- 2026-10-02T10:14:12-07:00: Cheat: Added Indiana Jones and the Last Crusade game-specific boxing cheat options.
- 2026-10-02T10:14:12-07:00: UI: Added SDL desktop window layout options for ultra-wide displays, including supporting option handling, window placement logic, documentation, and packaging metadata.
