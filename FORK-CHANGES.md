# ScummVM Private Fork Changes

This file tracks user-facing changes maintained in this private fork.

## Current custom changes

### Ultra-wide interface

- Added SDL desktop window layout options for ultra-wide displays.
- Restored the custom `Right two-thirds` windowed layout option alongside the existing layout choices.
- Updated the related SDL option handling, window placement logic, documentation, and project packaging metadata.

### Gabriel Knight 1 Windows AVI scaling

- Updated SCI AVI playback so the `enable_hq_video` option scales Windows AVI videos to fill the game window vertically while preserving aspect ratio.
- Kept this behavior scoped to AVI playback for the Windows CD version path.
- Left DOS SEQ playback alone so videos with baked-in letterbox bars keep their original presentation.

### Indiana Jones and the Last Crusade boxing cheats

- Added game-specific cheat options for Indiana Jones and the Last Crusade.
- `Invincible Indy` prevents Indy health loss during boxing damage scripts.
- `Strong punches` increases damage dealt to the opponent during boxing damage scripts.
- The implementation is based on the decompiled boxing scripts and preserves the original movement and punch state instead of modifying opponent positioning state.
