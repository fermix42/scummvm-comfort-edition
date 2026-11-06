# Integration baseline and preservation inventory

Status: IN PROGRESS. No runtime or build success is claimed.

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

All rows initially pending integration verification. No feature is authorized for
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
