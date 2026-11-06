# Integration baseline and preservation inventory

Status: isolated integration candidate built and automatically tested; human acceptance pending.

Original checkout: scummvm-private-feature-usable-desktop-window-layout-git, main,
HEAD de742c2c66239011dbbd4ce0d23d48ffcd0d0045, plus ten modified tracked files
and untracked engines/nox and devtools/nox. Originals are left in place.
Version: 2026.3.1git. Shared upstream ancestor: b72dad4a9c8e91139935cb83d234722199bc25c1.
Official annotated tag object: 3441393a24dbacefb831473eda728fc596bb86a3.
Official release commit: 7d2709da5136866589f7602f5f5a7b17de03420e.
Identity verified using the official HTTPS Git remote and release page.
Cryptographic signature trust has not yet been independently established locally.

The accompanying committed/local file lists and full CE commit list are the
exhaustive file/history inventory. FORK-CHANGES.md records individual original
behaviors and historical acceptance evidence; it is preserved in full.

| Feature / original behavior | Implementation locations | Required verification |
| --- | --- | --- |
| Achievement unlocks, repeat suppression, challenge/cheat gates, pack/icon/audio popups, local durable queue and link-token sync | engines/ce_achievements.*, engines/achievements.*, common/osd_message_queue.*, gui/options.*, engine hooks | Offline persistence, session ownership, isolated live sync with credentials |
| Named Play Sessions, save ownership, completion/abandon/archive/delete, casual audit, repeated per-session events | engines/ce_achievements.*, engines/metaengine.*, engines/engine.cpp, gui/launcher.cpp, gui/saveload* | Existing config/save fixtures, independent sessions and cold restart |
| MI4 Windows/PS2 save banks, native metadata and thumbnails, controller profile | engines/grim/ce_*.h, grim/savegame.*, grim/metaengine.cpp, grim_controls.cpp, emi/lua_v2.cpp | Existing devtools/ce MI4 engine regression runners and actual game assets |
| SQ5 Trash Talkin 0.2.12 detection, speech/lipsync, atlas, animation/ending, session saves and Battle Cruiser cheats | engines/sci, devtools/ce/*sq5* | Importer boundary tests, battle cheat native tests, game acceptance |
| Nox base v119 runtime, saves/history, session API, modern/classic modes and native migration work | engines/nox, devtools/nox, local changes in CE core | Preserve exact source snapshot; checkpoint-specific claims remain historical, migration is incomplete |
| Usable-desktop window layouts, per-game layout/preferences, SDL display/fullscreen/scaling/cursor conversion | backends/platform/sdl, backends/graphics/surfacesdl, events/sdl, test/backends/platform/sdl | Geometry regression and isolated display smoke checks |
| Launcher Cheats tab/master enable confirmation, challenge restrictions, option filtering | gui/editgamedialog.*, gui/options.*, engines/dialogs.*, advancedDetector.*, themes | Embedded and external themes, launcher and in-game paths |
| Cine crop with translated mouse coordinates, Operation Stealth achievement hooks, jetski/Amiga/ST fixes, sound, optional cheats | engines/cine, graphics/surface.*, backend surface SDL | Source audit and original variants/gameplay assets |
| Deja Vu detection/resource safety, title/About, expanded desktop, window/container/dialog fixes, restored transcript, achievements and cheats | engines/macventure, graphics/macgui, common/macresman.*, image/pict.cpp | Resource parser and real Deja Vu save/UI acceptance |
| Indy3 boxing invincibility/strong punches | engines/scumm detection/metaengine/script | Off-by-default gates and live boxing |
| SCI HQ AVI aspect-preserving vertical fitting | engines/sci/graphics/video32.cpp | AVI playback; DOS SEQ remains independent |
| Branding, Windows x64 manifests, dependencies, packaging and resource support | README.md, FORK-CHANGES.md, .github, devtools/create_project, devtools/github, run-build-release-cleanenv.ps1 | Isolated release build, resource/engine audit; no release publishing |
| Networking transport compatibility fix and save/gallery metadata refinements in uncommitted baseline | networkreadstream-curl.cpp, engines/metaengine.*, gui/saveload-dialog.cpp | Preserve exact snapshot and inspect upstream overlap |

The table above records the original verification requirements; final preservation status is recorded below. No feature is authorized for
removal. Save/config formats will be retained; testing must use isolated paths.
The root build script hardcodes the accepted non-Git build. It must not be run
unchanged for integration because that would overwrite the accepted executable.
Use the existing checkout-relative clean-environment script inside the isolated
worktree, preserving its configured toolchain and shared dependency paths.

## Merge and build adaptations

Local snapshot commit: 29e2ceb01611ac18654d82bfe2116db8183bd53d.
Merge commit: b7c20945716 (both CE snapshot and official release are parents).
The 162 incoming upstream commits touch 226 files; none overlap a CE-customized
file. There were zero textual conflicts. All CE source/data blobs remain exact.
The upstream changes include release metadata, translations, mm.dat, Nancy,
MADS, Harvester, Hopkins WBASE, SCUMM clipping, audio and platform fixes.
New game support already present in the shared development ancestor is retained
in addition to these later release fixes.

Intentional new divergence: devtools/create_project/create_project.cpp excludes
backends/saves/savefile.cpp from the unit-test compile list because the upstream
null_osystem.cpp includes that implementation itself with cloud disabled. This
fixes 13 duplicate symbols without changing production save handling.

The isolated clean-environment build script enables GIF, retaining HPL1 and
Phoenix VR that the previous generator otherwise disabled for unmet dependencies.
It explicitly bypasses global MSBuild vcpkg integration (which referred to an
unrelated dwd-exult checkout and failed with an empty triplet), and supplies the
existing include/library paths. No dependency installation or mutation of the
accepted dependency tree is performed. All runtime DLLs are copied into the
isolated output. The UnitTests switch generates and runs upstream tests from
their required fixture directory, propagates the real exit code (the upstream
post-build ignores it), then runs the existing SQ5 native regression via CTest.

Automated results so far: upstream CxxTest 434/434 pass (0 skipped); CE SQ5
importer 5/5 pass; SQ5 native regression 1/1 pass covering eight cheat combinations,
depleted saves, board flags, turn decisions and opponent isolation; integration
source/engine audit 7/7 pass. Initial importer run hit sandbox temp-directory
permissions; rerun uses workspace-local TEMP/TMP. Initial CJK tests failed when
launched from the wrong directory; all pass from build-tests with encoding.dat.
The Windows upstream build reports four existing test warnings in span/blending;
no warning suppression or ignored failing test is used.

## Final result (November 6, 2026)

Integration branch: codex/scummvm-2026.4.0-integration.
Worktree: D:\Codex Projects\scummvm-private\scummvm-2026.4.0-integration.
Validated application-source commit: 49b0d2a9b66; subsequent 31d4688904febf1d3380b5fedd9132994606518f
adds CLI verification and updates packaging workflows, without changing application
source. The final reporting commit contains this report; obtain its full identity
with `git rev-parse HEAD` in this worktree (also supplied in the chat delivery).
The accepted main branch, original dirty files, game images, old saves and both
accepted executable locations have not been replaced. Nothing has been pushed,
published, installed or merged into main.

Official HTTPS tag identity matches 7d2709da5136866589f7602f5f5a7b17de03420e.
The repository is not shallow. `git verify-tag` reports that the annotated tag
has no signature. `git verify-commit` cannot run because gpg is unavailable.
GitHub's official release page marks the release commit signature verified;
local cryptographic verification is therefore explicitly unperformed.

### Build and compiled engine evidence

Existing checkout-relative clean-environment script, VS2022 BuildTools MSVC
19.44.35229.0, Windows SDK 10.0.26100.0, Release x64, /m:1, /O2 and /GL.
Full build result: PASS, 0 errors, 66 warnings, elapsed 00:09:42.32.
Detailed log: worktree/msbuild-release.log; initial failed environment attempt:
worktree/msbuild-release-first-failure.log. Warning details are retained beside
this report; diagnostics include upstream signedness, shifts, format/portability
and singleton warnings. No CE feature was disabled to remove diagnostics.
This is not a warning-free build, and no clean upstream binary comparison was
performed to claim every diagnostic is identical across configurations.

Executable:
D:\Codex Projects\scummvm-private\scummvm-2026.4.0-integration\build-scummvm\Releasex64\scummvm.exe

SHA256: 65f81176d29e82e5f3b131494cea294ac19ac137a7e3dd444731278af612ea76

The executable reports ScummVM 2026.4.0. `compiled-engines.txt` records the actual
linked engine registry; it includes phoenixvr, hpl1, dm, avalanche, chamber,
fool, eem, harvester, wintermute, and CE's nox, grim, cine, sci and macventure.
The source audit independently checks every default-enabled upstream engine
against Release x64 definitions and checks real compile inputs. Build output
contains the corresponding engine libraries, not merely detection stubs.
Both Windows packaging workflows now explicitly enable GIF, monkey4 and nox;
CI workflows themselves were edited locally, not remotely executed or published.
Full game detection listing is in integration-validation-local/cli/games.stdout.txt.
Upstream engine definitions, detection data and release resources pass the
Git-blob audit; BUILTIN_RESOURCES is enabled in the production configuration.

### Automated verification counts

| Check | Passed | Failed | Notes |
| --- | ---: | ---: | --- |
| Upstream CxxTest | 434 | 0 | Zero skipped; includes CE window-layout geometry |
| SQ5 importer unit tests | 5 | 0 | Hash boundaries, path safety, source/output preservation |
| SQ5 native regression executable | 1 | 0 | All eight cheat combinations plus depleted-save/turn/opponent invariants |
| Integration audit | 8 | 0 | Both histories, full upstream paths/blobs, CE blobs, all default engines and packaging coverage |
| Linked executable CLI checks | 3 | 0 | Version, compiled engines, full detection listing |
| Nox resource/session runner | 1 suite | 0 | Seven process invocations; 74 exact resources; 8 negative, 327712 record, 3 storage, 7 schema, 22 metadata, 23 enhancement cases |
| Nox older-save session management | 4 scenarios | 0 | Archive/show/unarchive, delete-keep, delete-saves, empty session; warm/cold for each (8 processes) |
| Nox Modern gameplay runner | 1 suite | 0 | Opening/movement/modal/audio, two session saves, fresh-process restores, metadata/grid preview, pause/global menu/quit |
| Nox Classic gameplay runner | 1 suite | 0 | Same bounded checks; opposite startup default restores saved mode |

The first five rows total 451 individually counted checks. Nox cases/frames are
reported separately rather than inflated into a universal test total. Both Nox
mode runs verify four saved generations and two cold session restores; the
management run uses October 15 saves from executable bb70858867b68fca525c8508eef3ea6e674fb052c528ab77f43a96d5f2309df7,
providing actual backward-compatibility evidence. Original HDV hashes match
before/after. Metadata and ownership recovery do not change the save payload
format. No configuration migration was introduced.

Runtime evidence JSON is copied beside this report. Full local artifacts are in
D:\Codex Projects\scummvm-private\integration-validation-local (outside Git),
including copied game-image fixtures, saved worlds, receipts and screenshots.
These fixtures must remain local. Nox resource test's initial sandbox attempt
failed before running because ScummVM could not create its application-data
folder; the successful elevated invocation still used explicit isolated config
and save paths. Earlier environment/test-directory failures are documented above,
not counted as passes or hidden as skipped tests.

### CE preservation outcomes

| Inventory area | Final outcome and limits |
| --- | --- |
| Achievements, sidecars, progress and synchronization | All implementation/rules/theme blobs preserved unchanged; offline durable queue and save ownership exercised. Earned achievement unlocks/popups and live server sync still need acceptance. |
| Play Sessions and session-specific metadata | Preserved unchanged; two-session persistence, old-save recovery, archive/unarchive/delete and cold reload verified. |
| Custom saves, gallery and metadata | Preserved unchanged; Nox generation pairing, mode restoration, dates/playtime/preview and cross-session isolation pass. MI4/SQ5 real-save acceptance is pending. |
| Nox Archaist | All 723 untracked Nox source/tracking files were snapshot-committed with current core changes; baseline behavior preserved and bounded tests above pass. Existing M4 native migration remains IN PROGRESS; this upgrade does not claim campaign/native-only completion. |
| Cine/Operation Stealth | All detection, cheats, sound, achievements, patches and cropping preserved byte-for-byte in Git, compiled. Per-platform gameplay acceptance pending. |
| MacVenture/Deja Vu and resource parsing | All hooks, About/title, desktop, container/dialog/paging/save and cheat fixes preserved unchanged, compiled. Interactive gameplay acceptance pending. |
| SQ5 Trash Talkin and Battle Cruiser | Complete custom SCI source/detection retained; importer/native cheat tests pass. Real speech/atlas/ending/save checks pending. |
| MI4 Windows/PS2 and controller | Complete source/save-bank/controller integrations preserved; ENABLE_MONKEY4 and grim compile inputs verified. No existing MI4 save seed was located for engine runners; both save-bank gameplay tests remain unrun. |
| Indy3 boxing and SCI HQ AVI | Preserved unchanged, compiled; actual boxing/AVI presentation pending. |
| Window layout/scaling/fullscreen/preferences | Preserved unchanged; upstream CxxTest includes CE geometry tests. Actual multi-monitor/window transitions need human acceptance. |
| Launcher/cheat/challenge controls and themes | Preserved unchanged; Nox chooser/session management exercised actual GUI actions and both mode runs verified grid metadata preview. Full launcher/manual cheat/challenge behavior pending. |
| Branding/resources/packaging | CE README/changelog/branding/theme assets preserved; official release version retained. Local build/CI engine dependencies adapted as documented. No publication. |
| Additional core/network/local changes | All original dirty core/network files preserved in snapshot and integration, including transfer compatibility and save metadata changes. No silent unresolved textual merge. |

No inventoried customization has disappeared. The exhaustive file inventory and
snapshot commit remain the authoritative comparison, including files not named
individually in the summary. Only the build script and two Windows workflows
adapt CE-owned baseline files; their engine coverage is strengthened. The only
new upstream-file divergence is the test-generator duplicate-save compile fix.
The 8/8 audit passes with those explicit adaptations recorded.

### Remaining limitations and human acceptance

The source integration and local build are complete; broad product acceptance
is pending. Automated evidence does not establish every game, every achievement,
new-engine gameplay, all display arrangements or online service behavior.
No live achievement-server credentials were supplied or used. No live website
or account changes were made. No full Nox campaign or new native-migration
completion is claimed. The Linux SDL preload smoke test is inapplicable to this
Windows build. MI4 seed-dependent runners remain unrun, as noted above.

Before approving the upgrade, use copies of configuration and saves to:

1. Unlock an actual Operation Stealth or Deja Vu achievement in two distinct
   sessions, verify progress/sidecar popup suppression and Challenge Mode/cheat
   assistance semantics, then sync with a dedicated authorized account and
   verify offline retry/relink/session association without duplicate credit.
2. Load existing MI4 Windows and PS2 saves, run the existing save-bank runners
   with real seed saves, verify gallery thumbnails/native slots and controller.
3. Load SQ5 Trash Talkin saves and verify voice/lipsync, atlas, ending artwork,
   session ownership and all Battle Cruiser cheat combinations in gameplay.
4. Exercise Deja Vu expanded desktop/About, inventory/dialog/paging and save
   recovery; test Operation Stealth DOS VGA/Amiga/ST fixes and border cropping.
5. Test all saved window-layout options, negative-origin/multiple displays,
   fullscreen transitions, DPI/scaling, cursor mapping and launcher preferences;
   inspect SCI AVI and Indy3 boxing options.
6. Start representative new release titles (especially Phoenix VR, Dungeon
   Master, Chamber, Harvester, Avalanche, Fool, EEM, and Wintermute additions)
   with legal game data; save/reload and check installed resource availability.
7. Play longer Nox Classic/Modern sessions with normal audio and legacy saves;
   keep the existing native-migration campaign limitations distinct from this
   upstream upgrade.

Approve the isolated candidate only after those checks. Until then, keep using
the existing accepted CE build; this work has not changed the installed choice.
