# Escape from Monkey Island PS2 startup in Comfort Edition

## Build enablement

Escape from Monkey Island uses the Grim engine's optional `monkey4`
subengine. Detection can recognize the game even when that subengine is
disabled, in which case startup reports:

> Escape from Monkey Island support is not compiled in

The CE Windows release and layout-test workflows, and the local
`run-build-release-cleanenv.ps1` script, now pass
`--enable-engine=monkey4` to `create_project`. The local script also
regenerates an existing project when its x64 properties lack
`ENABLE_MONKEY4`, so an older generated solution picks up the change.

The PS2 version additionally requires MPEG2 support. The verified Windows
build already enabled `USE_MPEG2` and `USE_BINK`; no additional codec
dependency was needed.

## Renderer workaround

The English PS2 version was detected with `artAll.m4b` hash
`0dc9a4df0d8553f277d8dc8e23b6249d` and size 34,593,974 bytes.
After enabling the subengine, starting it with OpenGL with shaders caused
an access violation (`0xc0000005`) that terminated ScummVM.

Two local crash dumps showed NVIDIA OpenGL driver addresses on the
crashing thread. In the first dump, the main game thread was in
`Grim::GfxOpenGLS::createFont`. An isolated shader-renderer launch reproduced
the crash. Two isolated classic OpenGL launches remained responsive past
startup and loaded game resources.

For affected installations, select **OpenGL**, rather than **OpenGL with
shaders**, in the game's graphics settings. The equivalent target setting
is:

```ini
[monkey4-ps2]
renderer=opengl
```

This was saved for the tested local target. It is a per-game configuration
workaround, not an engine or NVIDIA driver fix, and is not automatically
applied by the build changes.

## Validation and limits

Validation on October 12, 2026:

- Windows Release x64 build succeeded with zero errors.
- The executable's `--list-games` output included `grim:monkey4`.
- Classic OpenGL passed the two startup process/log checks described above.
- Visible graphics, save/load reliability, and a complete PS2 playthrough
  were not established by those checks.

The game remains marked unstable in detection, and upstream lists PS2 as
unsupported. Enabling compilation does not establish complete game support.
Controller mapping changes are a separate concern from these startup fixes.

## PlayStation controller profile

For the PS2 game target, a controller whose backend-reported name identifies
it as a PS2, PS4, PlayStation 2/4, or DualShock 2/4 selects the console
keymap when the game starts. Other controllers and other game platforms
retain the existing MI4 defaults. No USB-adapter IDs are guessed.

The console keymap forwards each face button, D-pad direction, shoulder,
trigger, stick click, Start, and Select to the PS2 scripts' corresponding
native controller key. Options corresponds to Start; Share corresponds to
Select. Both sticks feed the native joystick axes rather than emulating
mouse movement or sending PC keyboard shortcuts.

PS4 touchpad click opens ScummVM's menu. Ctrl+F5 remains available on all
controllers, and F1 opens the game menu. The profile does not rely on the
PS/Guide button, which other applications can intercept.

All actions can be changed through the game's keymap settings. Console
bindings use the separate `monkey4_ps2_console` keymap ID, so existing
`monkey4` PC-style overrides do not inject overlapping keyboard actions
into the console profile. Connect the controller before launching MI4;
if changing controller families during play, relaunch to select the
corresponding profile.

Physical PS2 adapters must report a recognized PlayStation controller
name and standard SDL button/axis mappings. Devices reported as generic
joysticks use the existing defaults and remain manually configurable.

Validation: the Windows Release x64 build completed with zero errors.
An isolated launch with the connected PS4 Controller logged selection of
the console profile and remained responsive. A launch with controller
input disabled retained the fallback profile and remained responsive.
The user confirmed the controller profile worked in gameplay on October
13, 2026. A physical PS2 controller was not tested locally.

## CE playthroughs and native PS2 saves

The game's native save/load menus remain responsible for its four player
slots. CE hooks run after a successful save-file finalization or completed
restore, using the actual file resolved for the session's slot. Internal renderer
checkpoints and attract-mode files do not change playthrough membership.

The existing CE service stores session identity, Casual/tainted state, and
popup progress in the adjacent `.ce-achievements.json` sidecar. Loading
through either menu restores that state. Saves without a sidecar remain
untracked; their original session is not inferred. Saving over a player
slot overwrites only that session's file. Each named session has its own
four slots; starting another session cannot overwrite the previous one.

Filename routing is centralized in the CE-owned `ce_ps2_saves.h` helper.
The game continues asking for `efmi000` through `efmi003`. An existing
file whose sidecar belongs to the current session remains usable at its
original filename. Otherwise, CE uses a target/session-specific filename
ending in `-slot1.ps2` through `-slot4.ps2`. Stable filename tokens avoid
Windows filename restrictions; the sidecars retain human-readable full
session IDs. Existing legacy saves are neither moved nor converted.
If a preferred namespaced filename already exists without matching
ownership metadata, CE allocates a separate `-copyN-slotN.ps2` file rather
than overwriting it. Subsequent operations find the session's owned file.

The PS2 scripts query four fixed slots through `GetSaveGameData`, rather
than using ScummVM's save chooser. Their metadata and thumbnail requests
are therefore also filtered by CE session identity. A save from another
session is reported using the existing empty-slot response. With no
active session, only untracked saves are visible. This hides entries
without deleting files; ScummVM's session chooser remains the way to
switch to another session or an untracked save.

The Grim metaengine supplies the real PS2 filenames through upstream's
`getSavegameFile()` API. CE's chooser uses that filename for filtering and
session deletion even when the engine is not running. Legacy filename
fallbacks remain available to other engines. ScummVM's chooser displays
`Slot 1 - Mansion Exterior`, for example; stored descriptions and the
game's own menu remain unchanged.

ScummVM's chooser uses a temporary index into the current file list to
distinguish slot 1 in session A from slot 1 in session B. That index is
resolved to an exact filename before loading; restoring its sidecar then
activates the owning session. The player-facing slot remains 1-4. Session
deletion runs backwards through the chooser list, preserving those indices
until the list is refreshed.

MI4 PS2 enumerates saves by sidecar session ownership, then native slot
1-4, with filename as a deterministic tie breaker. This keeps an owned
legacy slot 1 ahead of that session's newer slots 2-4. The same ordering
resolves chooser IDs to files; shared chooser sorting and other games
are unchanged.

The slot-order build passed with zero warnings and zero errors. The
isolated regression verified 1-4 ordering in both banks and in a bank
mixing legacy slot 1 with newer slots 2-4, plus native metadata mapping
and preservation of the original input save.

The integration does not enable saving through ScummVM's global menu.
Use MI4's native menu to save, and either MI4's menu or ScummVM to load.

Validation on October 13, 2026 used temporary save directories and copies
of an existing save. The final Release x64 build passed with zero warnings
and zero errors. Runtime checks exercised the native Lua Save/Load calls,
startup slot loading, tracked/tainted session restoration, untracked
loading, an incompatible save, slot overwriting, and a failed save-open.
Overwriting replaced the slot's session association; failed save/load
operations preserved it. The save listing displayed all four player slot
numbers. The player's original save remained byte-identical.

The automated native-save fixture supplied a test metadata callback through
a temporary script override. The production game scripts were not changed.
These checks do not replace a full playthrough or physical testing of the
in-game menu and CE session chooser, and server sync was disabled in the
isolated fixtures.

Additional native metadata checks verified four visibility cases: a new
session saw no occupied slots; session A saw only its slot; session B saw
only its slot; untracked play saw only the untracked slot. These checks
used the actual `GetSaveGameData` opcode against isolated copies and CE
sidecars. The filter build passed with zero warnings and zero errors.

The independent-bank build also passed with zero warnings and zero errors.
Two test sessions each saved all four slots into the same save directory.
Session B's writes left session A and the legacy copies byte-identical.
Native metadata reads returned only the current bank. Startup loading
through chooser IDs restored the correct owning session, and native
deletion of B's slot 1 left A's four slots and the legacy files intact.

`devtools/ce/test_mi4_save_banks.py` provides a repeatable engine test. It
checks four slots per session, native reads, chooser enumeration, original
save preservation, and preservation of a namespaced file with a lost
sidecar. All game data and save inputs are read-only; temporary fixtures
are isolated and removed afterwards. Example from the repository root:

```powershell
python devtools/ce/test_mi4_save_banks.py --exe build-scummvm/Releasex64/scummvm.exe --game "D:\Games\ScummVM\Escape from Monkey Island (Multi-Platform)\Escape from Monkey Island (Playstation 2)" --save "$env:APPDATA\ScummVM\Saved games\efmi000.ps2"
```

The fixture emits a completion marker after its operations. The runner
stops only its own test process after that marker to tolerate SDL/audio
shutdown stalls. Files are then checked for correct ownership and hashes.

## ScummVM gallery thumbnails

MI4 PS2 exposes its existing `SIMG` screenshot through ScummVM's standard
save metadata descriptor. The reader decodes little-endian RGB565 pixels
from the 256x128 texture and restores the original 160x120 screenshot
aspect ratio for the chooser. It does not write or migrate saves. Missing
or unexpected screenshot sections leave the save available without an
image.

The chooser checks metadata and thumbnail capabilities for its target.
The new `MetaEngine::hasFeatureForTarget` default delegates to the existing
engine-wide feature check; only Grim's MI4 PS2 target overrides these two
capabilities. Grim Fandango, PC MI4, and other engines retain their prior
chooser behavior. The decoding and MI4 filename mapping live in the
CE-owned helper rather than the shared GUI. Gallery selection follows
ScummVM's existing chooser preference and window-size requirements.

The gallery Release x64 build passed with zero warnings and zero errors
after retrying an MSVC internal compiler error in unchanged detection
code. The isolated engine suite passed bank separation, mixed legacy/new
slot ordering, metadata enumeration with existing screenshots, missing
and malformed screenshot fallback, and original-save preservation.
On October 13, 2026, the player confirmed the completed save/load
handling works as expected. This is gameplay confirmation of the
save/load integration, not a full-game compatibility claim.

The native PS2 game binds a new game to the slot selected at startup and
continues saving to that slot. Returning to the title screen permits a
new game in another slot. CE preserves that behavior while isolating the
four native slots for each play session.
