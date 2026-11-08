# Gooka in Comfort Edition

CE imports the maintained experimental Gooka engine from
[fermix42/scummvm](https://github.com/fermix42/scummvm), with original provenance
from [Kungergely/scummvm](https://github.com/Kungergely/scummvm), initially at
`66b8f9703e73fa7a2c9bd8e81ea312a824e97fdf` (November 4, 2026).
Only `engines/gooka` is imported; the fork's ScummVM core is not merged.
The upstream GPL notices and engine credits are preserved.

The current detector recognizes the Czech Windows CD release v1.0.3.0,
using `0.BIN` and `1.BIN`. Game data is not included. Upstream marks the
game unstable; integration does not establish that it is completable.

## Updating

From this CE checkout, with Python 3.9 or later:

```powershell
python devtools/ce/update_gooka.py --verify
python devtools/ce/update_gooka.py --ref <reviewed-40-character-fork-commit>
python devtools/ce/test_update_gooka.py
```

The updater requires an exact reviewed commit (floating branches are rejected), verifies Git blob
checksums, and records upstream and installed SHA-256 checksums in
`devtools/ce/gooka-upstream.json`. Use `--ref <commit>` to import a specific
revision or roll back to a known snapshot. `--checkout <local-fork-checkout>` reads
the same pinned Git blobs locally, verifies their object hashes and fork origin,
and never imports uncommitted working-copy changes. It refuses updates when engine files
have local edits, additions, or deletions. Reconcile those edits before updating.
All downloads finish before the installed snapshot is changed.

The only CE source adaptation expands upstream's GNU make wildcard room-object
expression into an explicit list, because Visual Studio's `create_project`
does not expand that expression. The updater reapplies this adaptation and
includes new room sources automatically. Inspect the diff after each update,
especially if upstream changes its module format or adds dependencies outside
the engine directory. Run the offline checks before building.

The checked-in Windows release and layout workflows explicitly enable Gooka;
other builds can use `--enable-engine=gooka`. The upstream build-by-default
setting and unstable detection flag remain intact.

## Czech and English (development after v0.1.4)

Game Options → Language offers Czech and English for the same Czech CD data
through ScummVM's standard `ADGF_ADDENGLISH` capability. Restart to apply.
The English resource-ID table is compiled into the executable, so the portable
distribution needs no additional runtime loader, network or language asset.
Default/Czech uses original CP1250 text; missing English entries warn and fall
back to Czech. Game assets and save serialization/directories are unchanged.
Original Czech voices/video remain; enable subtitles for English dialogue.
Old targets with stale GUI options may need to be re-added to refresh the list.

The owned data yields 835 text IDs (834 nonempty). All have English table entries;
819 change text and 16 retain names, existing English, markers or whitespace.
Seven entries remain flagged for contextual review: 856, 918, 1338, 1339, 1373,
3216 and 3221. This is complete ID mapping, not a claim of complete reviewed
screen coverage. Text baked into graphics/video is outside this lookup.

The maintained extraction, translation, glossary, layout and synchronization
instructions are in `engines/gooka/localization/README.md`. Keep full original
Czech catalogs and owned data private; only translation resources/tools and
original credit names belong in the fork. `english.json` is the correction source;
`compile.py` generates the checked-in CP1250 table. English-only TextSprite
wrapping and font-measured book layouts retain Czech presentation.

Use `devtools/ce/test_gooka_localization_runtime.py` for bilingual resource
rendering and cross-language original-save checks with the existing native CLI.
This is focused localization development; the published v0.1.4 tag, binaries and
assets remain untouched. Do not repeat unrelated engine or release acceptance.

From the intended clean CE checkout, use its checked-in build script:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\run-build-release-cleanenv.ps1"
```

The executable is `build-scummvm/Releasex64/scummvm.exe` beneath the consolidated
checkout. Check `--list-engines` and `--list-games --engine=gooka` after building.
Then test detection, startup, room transitions, sound, and save/load with owned
game data. Future updates are manual; no recurring updater is installed.

## Initial integration validation (November 7, 2026)

- Release x64 clean-environment build: passed, zero errors. The full CE build
  reported 36 warnings in existing code; none were emitted for Gooka.
- All 124 Gooka implementation sources and its detector are registered in the
  generated Visual Studio projects.
- Four offline importer checks passed, including snapshot integrity, local-edit
  protection, complete room registration, and repeatable module adaptation.
- Executable smoke checks passed: version, engine registration (including CE's
  existing engines), and the `gooka:gooka` detection-catalog entry.
- Gameplay, game-data detection, and save/load acceptance remain unverified.

Local CLI output and executable checksum are recorded in
`gooka-validation-local/verification.json` beneath this checkout.

## Release integration review (November 8, 2026)

The existing engine snapshot and GNU make adaptation were recovered intact and
committed separately from Windows build integration. Gooka adds one engine to
the complete CE set; it does not replace the upstream ScummVM core or change
the accepted strict-storage implementation. Saves use the standard extended
SaveFileManager format and developer PNG dumps use the guarded filesystem API.
No profile/registry/default-user storage producer was found in Gooka.

The supplied Czech CD BIN files match the existing detector's sizes and MD5
signatures exactly. The accompanying patch contains only `GAME.EXE`, which this
engine does not use; it was not applied. Extract owned `0.BIN`..`11.BIN` and
videos (`12.AVI`, `13.AVI`) into a separate game-data folder. Game files and
screenshots are never included in CE release archives.

`devtools/ce/test_gooka_runtime.py` uses the existing native controller to skip
the opening video, and the imported opt-in `GOOKA_TEST` hooks for real rendered
room transitions, click/tick handling, engine save/load and cold restoration.
These exercise real game resources and serializers, not fabricated game state.
They do not certify in-game save-dialog navigation, audible sound, full video
playback or completion of the game. The initial unattended pilot timed out at
100 seconds during the opening video; the bounded regression uses an ordinary
Escape event instead of disabling video support.

Upstream still marks Gooka unstable and includes incomplete shared NPC/room
behavior (including rooms 1200, 2000, 2037 and 2049). Keep the unstable flag and
experimental release-note status. Final candidate hashes and fresh results
belong in the CE v0.1.4 readiness report, not the historical November 7 results.
