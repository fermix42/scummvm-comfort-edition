# Snatcher in ScummVM Comfort Edition

Snatcher is a normal experimental CE engine. The standard Windows x64 Release
build includes it through ScummVM's existing `--enable-engine=snatcher`
configuration, alongside Nox and the other CE engines. No separate launcher,
installation or distribution is required.

From the workspace root, use the existing build command:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\Codex Projects\scummvm-private\run-build-release-cleanenv.ps1"
```

The ordinary executable is
`scummvm-ce-2026.4.0-consolidated/build-scummvm/Releasex64/scummvm.exe`.
The existing `-UnitTests` workflow runs the CE CxxTests, CE native regressions,
and Snatcher component CTests. There are no Snatcher-specific build switches.

Add original Sega CD game resources using CE's normal Add Game workflow.
Detection retains engine/game ID `snatcher`, platform Sega CD, and preferred
target `snatcher-us-segacd`. Supported inputs include original CHD and supported
single/multi-BIN CUE resources. Optional uncensored data remains an external,
owner-provided resource; the option is off by default.

Detection reports `kUnstableGame`. Engine creation uses ScummVM's existing
unsupported-game warning and its normal preference. The experimental designation
does not establish ordinary-use safety: severe system-wide Windows slowdown,
including reported effects after process exit, remains unresolved. Prior
investigations and evidence are preserved. Automated regression work uses bounded
SDL dummy-device tests; do not run uncontrolled real-device Snatcher tests on
the owner's desktop or claim that this issue is fixed.

Snatcher uses slot 0 for ScummVM autosaves, slots 1–4 for Metal Gear's four
original save choices, and slots 5–99 for manual save-anywhere saves. F5 opens
the ScummVM save chooser; F7 opens the load chooser. The save chooser labels
0–4 as reserved and disables manual writes there. Internal saves are listed as
`Internal Save Slot 1` through `Internal Save Slot 4` and load through CE using
the original checkpoint-resume semantics. They retain the original SGR2 codec;
autosaves and manual saves retain the existing full-state SNSV codec.

Autosaving follows the configured ScummVM interval and safe-save checks. CE also
saves the prior state before loading another slot; loading slot 0 does not
overwrite the autosave being loaded. Autosave identity is kept in a small
save-manager-owned fingerprint file, without changing either save schema.

Before the first new-policy launch, legacy save-anywhere files in 0–4 are moved
to free manual slots starting at 5. Their payloads, backup generations and CE
ownership sidecars move together; occupied destinations are skipped. A selected
legacy startup slot follows its relocated file. Genuine new autosaves and
original-format internal saves are not moved on later launches. If no free
manual slot exists, relocation refuses to overwrite data. Original `.sgr`
filenames remain readable for existing in-game saves; new internal writes use
the CE slot filenames. FM selection/restoration, input, graphics, resource
loading and strict portable storage remain intact.

Save compatibility recognizes the verified original/decensor SP25 pair in both
directions. Turning the option off does not discard narrative identity checks:
the approved `snatcher-decensor.dat` must remain available to verify the saved
counterpart, and changed active instructions still reject restoration.
`devtools/ce/test_snatcher_decensor_saves.py` checks cold option transitions.

Animation plane-position and velocity commands post the same secondary visual
request as explicit positioning. This resumes Main's displayed scroll publication
after title initialization; updating only the Sub position left the first chief
office background at an obsolete origin. The scene-runtime regression preserves
the distinct computed and displayed frame boundaries.
`devtools/ce/test_snatcher_office.py` follows ordinary reception dialogue and
the first hallway/office transition from a supplied reception checkpoint.
Its `--extended` route adds chief conversations, item presentation, office exit
and revisit, computer-room entry and cold continuation. `--office-start` accepts
an already reached first-office checkpoint for that continuation. Check authored
revisit variants separately: the office uses list1 initially and list0 on revisit.
Shared rendering changes require these gameplay handoffs in addition to isolated
component checks; a title-only pass does not establish later scene correctness.

Successful native saves use CE's normal session-recording hook, and successful
loads restore the saved session through the corresponding load hook. Existing
untagged saves remain Untracked until deliberately assigned. An untracked save
replaces stale ownership metadata instead of inheriting an old session, and a
manual write cannot overwrite another session's occupied manual slot. Loading
an untagged backup does not inherit the primary generation's session. Save
payloads and schemas are unchanged. `devtools/ce/test_snatcher_sessions.py` checks two
sessions, save ownership, cold continuation, untracked saves, autosave protection
and backup fallback using owned dummy-device roots.
`devtools/ce/test_snatcher_save_slots.py` additionally checks migration from the
previous executable, reserved-slot protection, actual F5/F7 dialogs, periodic
and pre-load autosaving, and ordinary Metal Gear input to all four slots followed
by CE loading and a cold restore. Its fixture is a retained genuine original
checkpoint; no campaign state or save outcome is injected.

The qualified source was merged from
`b906b0006d4c8ac005504acbdbe0d58d73db152f` without conflicts. The earlier
[integration report](INTEGRATION-20261121.md) is a historical record of the
isolated qualification; its temporary build-switch instructions apply only to
that preserved candidate. Normal CE development now uses this checkout and the
ordinary commands above. Current merge/build evidence is kept under
`validation/snatcher-merge-20261121/` and the ordinary build's owned regression
roots. Existing owner installations and profiles are not updated by a source
merge or a build.

## Save thumbnails and gallery

New manual saves, autosaves, and the four internal checkpoint saves embed a
standard RGB565 screenshot thumbnail in the same verified save generation.
Snatcher advertises thumbnail support to the normal ScummVM gallery/grid chooser.
Existing saves remain loadable and appear without an image until resaved.
Full snapshots use an optional thumbnail section; internal SGR2 records retain
their core story/checkpoint payload with an optional bounded SNTH attachment.
Older executables can skip the optional full-snapshot section, but pre-thumbnail
executables cannot read internal records containing the new attachment.
Thumbnail capture reads the game framebuffer, excluding the GUI overlay, without
a GPU readback. Gallery metadata follows the same primary/backup generation as
save validation; a malformed preview is ignored rather than preventing loading.
