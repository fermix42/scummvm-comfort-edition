# Historical M1 execution log

These dated checkpoints are superseded by SCUMMVM-CE-PROGRESS.md. Failures and superseded pending states are retained as historical evidence.

# Nox Archaist ScummVM CE progress

Latest evidence supersedes historical pending/blocked statements below; historical failures are preserved.

Updated: October 15, 2026, America/Los_Angeles.
Authority: approved architecture decision; implementation authorization: M1 only.

## Current work

- Current milestone: M1 â€” IN PROGRESS.
- Current task: M1.7 â€” IN PROGRESS, final actual launcher and regression acceptance. M1.6 interrupted-write/corrupt-load transactions and actual chooser UI now VERIFIED; normal host and live CE synchronization/replay are VERIFIED.
- Next actionable task: demonstrate actual launcher Start/session-name flow into tested gameplay, rerun final destination suite/server audit and standalone regressions, verify original resource hashes and document limitations. Stop before M2.
- M2, M3, M4: NOT STARTED; implementation not authorized.

## Task ledger

| Task | Status | Evidence or remaining criterion |
| --- | --- | --- |
| M1.0 | VERIFIED | Canonical instructions/three documents exist; CE copies match via sync_project.py --check; actual trees and baseline hashes inspected |
| M1.1 | VERIFIED | Actual CE executable extracted all 74 files matching independent oracle; eight malformed/range checks and 327,712 native record cases passed; evidence below |
| M1.2 | VERIFIED | Actual CE executable lists Nox and detects renamed v119 plus modified-save image; rejects expansion-only input; build passed |
| M1.3 | VERIFIED | Actual opening/movement/Open plus ordinary paced global menu/quit host test pass |
| M1.4 | VERIFIED | Nonzero gameplay PCM/mixer path, cold-load reset and ordinary pacing/pause/resume/quit pass; no physical listening claim |
| M1.5 | VERIFIED | Offline/framework tests plus actual CE hermesbox synchronization/replay and independent server identity/ownership/hash audit pass |
| M1.6 | VERIFIED | Full codec, generation storage, two-session cold Engine saves, actual chooser, killed-process overwrite recovery and corrupt-load rollback pass |
| M1.7 | IN PROGRESS | Actual launcher Start/session-name flow and final same-binary suite/regressions remain |

## Baseline and working trees

- Nox directory has no `.git` repository (verified `git rev-parse --show-toplevel` fails). Do not initialize/import resources automatically.
- CE checkout: `D:\Codex Projects\scummvm-private\scummvm-private-feature-usable-desktop-window-layout-git`; initially clean `main...origin/main`. No fetch/push/deploy performed.
- Read existing CE root AGENTS.md; preserve its clean-environment build instructions. No scoped engine AGENTS.md found before work.
- Canonical tracking files: this file, implementation plan and architecture decision in Nox `conversion/`.
- Repository recovery copies: CE `devtools/nox/project/`, synchronized byte-for-byte after updates.

## Actual modified files

Nox:

- `AGENTS.md` â€” project recovery, autonomy and scope instructions.
- `conversion/SCUMMVM-CE-IMPLEMENTATION-PLAN.md` â€” approved milestones and bounded acceptance tasks.
- `conversion/SCUMMVM-CE-PROGRESS.md` â€” this evidence ledger.

CE files now modified/created (all local, no commit/push):

- `AGENTS.md` â€” appended Nox recovery/scope instructions, preserving build instructions; root file is locally ignored by existing .gitignore.
- `devtools/nox/project/{AGENTS.md,SCUMMVM-CE-ARCHITECTURE-DECISION.md,SCUMMVM-CE-IMPLEMENTATION-PLAN.md,SCUMMVM-CE-PROGRESS.md}` â€” versionable recovery copies.
- `devtools/nox/sync_project.py`, `test_resources.py`, `test_gameplay.py` â€” document synchronization, independent CE resource/detection/offline-session harness, and real CE event-loop gameplay harness.
- `engines/nox/{configure.engine,module.mk,detection.cpp,metaengine.cpp,nox.h,nox.cpp,resources.h,records.h,validation.cpp,session_validation.cpp}` â€” explicit engine registration, immutable Common-based parser/record slice and isolated validation drivers; current source includes CE host wiring.
- `engines/nox/{runtime.h,runtime.cpp,cpu.inc,cycles.inc,movement.cpp,render.cpp,speaker_audio.h,speaker_audio.cpp,mockingboard.h,mockingboard.cpp}` â€” temporary portable per-instance bridge and reused generic CPU/synthesis/native-movement/raster code; no generated game programs, original instruction payloads or ROM font copied. Latest source wires ordinary launch to the real runtime, not a placeholder frame; this path is not yet tested.
- `engines/ce_achievements.{h,cpp}`, `gui/launcher.cpp` â€” shared explicit target session capability separate from award catalog; requires opted-in exact API metadata, retains catalog-backed behavior.
- `run-build-release-cleanenv.ps1` â€” include Nox in engine selection and refresh stale generated Nox project; no global C++ standard change.

## Build and test evidence

Baseline CE commit: `5f84c5cd08cbc54083cf72da8cfa1665294c2bca`.
Baseline standalone executable SHA-256: `d26b2e8dd19efc1420ae569892f8ae3bd6f39f06cbfd835758f95cfdd1e0d695`.
Original v119 SHA-256: `c42a95c7a356548c78957f98693e92d33c9bae3ff5126d9887c2980e5a490c3c`.
Original expansion SHA-256: `90ec012a317c56ab9a5182a1a877227fa7a18ebed72567d2bf08888956b635c9`.

- PASS: `python devtools/nox/sync_project.py --check` (four byte-identical recovery documents at baseline).
- PASS: `python -m py_compile devtools/nox/test_resources.py devtools/nox/sync_project.py`.
- PASS: `git diff --check` for tracked changes at initial slice checkpoint.
- Historical build attempts: initial session 23014 failed on stale generated project; second attempt failed on two CE API compile details; third session 41328 passed. Enabling Nox initially changed generated global engine definitions and triggered a full rebuild.
- IN PROGRESS: latest CE build session 84636, same clean-environment script and `nox-m1-build.out.log`/`msbuild-release.log`; this rebuild includes the latest gameplay validation driver.
- Pending destination harness:
  `python devtools/nox/test_resources.py --exe build-scummvm/Releasex64/scummvm.exe --base "D:/Codex Projects/Nox Archaist/Nox Archaist HDV (v119).HDV" --expansion "D:/Codex Projects/Nox Archaist/Nox Archaist Lord of Storms HDV (v137).HDV" --artifacts "D:/Codex Projects/Nox Archaist/conversion/ce-m1-validation"`.

Prior assessment reports remain historical evidence, not verification of new M1 functionality. No playable CE frame, input/audio or durable gameplay save has been demonstrated in this execution.

### Verified CE resource and local-session slice

- Full CE clean-environment build succeeded with **0 warnings, 0 errors** in the final successful incremental build (2m47s). Resource/session-tested executable SHA-256: `313a7473d9925c33776e5b03a42fc0212ae2a0755b9415570f49036a533ddb00`.
- `devtools/nox/test_resources.py` passed, artifact directory `D:\Codex Projects\Nox Archaist\conversion\ce-m1-validation\20261015-104514`, machine-readable `verification.json` includes every argv/exit code.
- 74 resource files exact against independent ProDOS extraction; ordinary and cross-block/case-insensitive range checks; eight negative parser/read tests; 327,712 record cases.
- CE `--list-engines` includes Nox; `--detect` identifies renamed base v119 and a private image with a mutated GAME1 data block; expansion-only directory is not detected.
- Two zero-award CE local sessions created through shared service, separate sidecars for the same logical metadata-fixture slot; fresh-process loads switch the original IDs without creating extra records or network events. These are metadata fixtures, **not gameplay saves**. Online acceptance and actual gameplay-save association remain unverified.
- Original base and expansion SHA-256 unchanged after harness.
- Six standalone existing-binary regression tests pass; report `conversion/ce-m1-standalone-verification.json` records binary hashes/arguments/results. No standalone source changed or rebuild claimed.

### Failures diagnosed and corrected

- Initial full link failed on missing `validateSessions`: generated project lacked the newly added source; build script now detects stale Nox project and regenerates it.
- Second build found missing `engines/util.h` for initGraphics and explicit U32String conversion for keymapper action labels; fixed, subsequent build passed.
- Initial headless harness selected unsupported OpenGL with SDL dummy; local backend source identifies `surfacesdl` as the correct mode. Harness now explicitly selects it.
- Expansion assertion originally searched the full output for `nox`, matching the workspace path; fixed to parse an actual `nox:nox` detection result line. The detector correctly rejected the expansion throughout.
- Current build session: 84636; log `nox-m1-build.out.log`. Latest gameplay-driver source is newer than resource-verified executable and is not covered by that binary's evidence.

## Confirmed defects and temporary dependencies

- Audit downstream fixed-three-byte signature comparison misses shorter instructions; preserve reliable raw accounting and do not claim conversion completion from counters.
- Standalone legacy save completion mutates private memory; persistence happens later on dump/normal exit.
- CE launcher currently ties website session support to achievement definitions; Nox needs shared session capability with zero definitions.
- CE default save write/rename is not atomic replacement; M1 requires validated generations and failure tests.
- Standalone still uses generated translated execution, general fallback, CPU/bank frames and IRQ-coupled scheduling. The new CE temporary bridge reuses generic fallback and bank/IRQ semantics but does not embed generated game programs. It reads executable game resources from the owned HDV; interpreter independence is not claimed.
- Windows host and C++20/exception interfaces need bounded CE adaptation; preserve standalone regression paths.

## Decisions and external blockers

No new architecture decision requested. Website game/variant registration and
test credentials may block online acceptance; no website deployment is authorized.
Do not read or expose credentials in logs. Continue local M1 work if online testing
is unavailable, and record the remaining criterion as BLOCKED.

## Resume checklist

Read AGENTS.md, architecture, plan and this ledger; inspect CE Git status/diff and
new evidence; verify resource hashes and build provenance; resume the next unblocked
M1 task. Save the state of running builds/harnesses before ending a session. Stop
before M2 even when M1 is complete.

### October 15 continuation: approved API identity and current failures

- User approved `nox-archaist` / `apple2-v119`, display `Nox Archaist`, variant `Apple II â€” Base Game v119`, subject to conventions. Compared existing seeders and API: lowercase kebab identifiers and distinct source_engine/source_game_id fields are compatible. Adopt these identifiers; do not use synthetic fixture IDs online.
- Website local changes: `database/seeders/NoxArchaistSeeder.php`, `database/seeders/DatabaseSeeder.php`, `tests/Feature/NoxPlaythroughApiTest.php`. No achievement rows. Existing untracked `ops/dev/` preserved.
- PASS: isolated SQLite `:memory:` framework tests using PHP `C:/Users/aaron/.config/herd-lite/bin/php.exe`, `vendor/bin/pest tests/Feature/NoxPlaythroughApiTest.php tests/Feature/GamePagesTest.php tests/Feature/PlaythroughApiTest.php --compact`: 27 tests, 195 assertions. Tests create dedicated non-admin users with runtime-only random tokens; verify idempotent zero-award registration, supported catalog/detail routes, two sessions, replay, separate same-slot save metadata, reload metadata and cross-account rejection. `vendor/bin/pint --test database/seeders/NoxArchaistSeeder.php tests/Feature/NoxPlaythroughApiTest.php`: PASS.
- Initial new website test incorrectly expected `/games` to list zero-award games. Existing tests explicitly require that achievement-only page exclude them. Corrected test to supported catalog `/scummvm-ce/catalog`; no controller/view changed or acceptance claim about an achievement listing made.
- BLOCKED criterion only: real CE HTTP synchronization to a dedicated local/staging account. No configured isolated running server/token yet. Framework API tests are not CE transport tests. Remaining steps: provision a separate local/staging database, migrate and seed Nox, create non-admin test user and revocable API token through existing registration, configure isolated CE save/config with that local API URL/token outside source/reports, create A/B using approved IDs, sync, save/reload both same-slot gameplay states, verify distinct website rows and replay, revoke test token. No production deployment/account changes authorized.
- Resource/offline tests also passed on binary SHA256 `27ceaf1f34076c017f9eaf0adac5840012368e69b7790fdbb5cffb0f2ebb9344`, artifacts `conversion/ce-m1-validation/20261015-104854` (inspect verification.json for exact invocation). This does not cover subsequent source.
- Build 36147 passed (0 warnings/errors, 2m33). Gameplay artifacts `conversion/ce-m1-gameplay/20261015-105349`: FAIL, phase 3 never reached world input; trace persisted, start coordinates 24,38, frequent D560 and music IRQ. No PASS report, no gameplay verification claimed. Earlier route `20261015-104854` timed out at 90 seconds.
- Latest attempted fixes: test keyboard injection now supplies valid letter keycodes and KEYUP; failure path saves rendered PNG. This is unverified until rerun. Normal runtime error behavior preserved.
- `engines/nox/runtime_state.cpp` encodes RAM/banks/registers/private disk/timing/audio fieldwise using Common::Serializer; no pointers/callbacks dumped. No durable envelope, generation recovery, Engine save UI or actual cold gameplay load yet.
- Build 46186 FAILED: new codec included `<cmath>` after ScummVM forbidden symbol macros, producing MSVC header errors. Replaced with finite range checks using DBL_MAX, retaining C++11. Rebuild session 19980 is active; log CE `nox-m1-build.out.log`. Next: finish build, run gameplay harness, inspect failure PNG/trace if necessary, then complete durable save work.
### Current checkpoint after codec compile and website tests

- Build 19980 PASS: 0 warnings/errors, 2m38s; binary `b6913580c818a8b14b80f1c014ebb9801eb2e0caf1d8e2121669bd404f5f1bd9`. Full codec compiles; no codec round-trip claim yet.
- Gameplay `conversion/ce-m1-gameplay/20261015-110316` FAIL: KEYUP change did not satisfy harness. Captured `saves/nox-gameplay-failure.png` inspected: actual scene, party, HUD, and `Invalid Command` text. Evidence indicates Return injections continue after entering gameplay; existing harness assumed only PC71DC was world input. Do not claim movement/modal verified. Next diagnostic adds mode/bank/stack-return fields without changing acceptance criteria.
- Build 90421 PASS: 0 errors, one MSVC C4506 warning in existing Common::Singleton<Networking::ConnectionManager>::makeInstance while compiling shared CE service; 2m34s. Includes approved launcher identity mapping and registered Nox zero-award capability, plus diagnostic trace. Gameplay session 90653, artifact `conversion/ce-m1-gameplay/20261015-110750`, active at this checkpoint.
- New source AFTER that build: `engines/nox/snapshot.h` provides two checksummed generations via SaveFileManager; header and payload covered, bounded sizes, version checks, readback verification, no rename assumption. `validation.cpp` exercises round-trip, simulated interrupted newer-generation truncation and recovery/replacement. Harness now requires `storage_cases=3`. These changes are NOT YET BUILT OR VERIFIED and are storage fixtures, not durable gameplay saves.
- Approved Nox identity mapping in `gui/launcher.cpp`, known base-game capability in `engines/ce_achievements.cpp`, capability assertions in `engines/nox/session_validation.cpp`; source test assertions require the next resource/session run.
- Website tests persisted at `conversion/ce-m1-website-tests.json`: 27 passed / 195 assertions; separate Pint PASS. No credentials written in reports, no deployment, no production account changes.
- M1 remains IN PROGRESS; M2â€“M4 remain NOT STARTED. Autonomous execution objective is active in this chat; repository files remain the recovery authority.
### Generation storage verified; first ordinary movement demonstrated

- Build 36860 PASS: 0 warnings/errors, 2m35s. SHA256 `15889948175dc61dca8f5ad9e85a2a634a51629eddd7683bf4c6bd4ec223c281`.
- PASS resource/session/storage harness: `conversion/ce-m1-validation/20261015-111320/verification.json`. 74 exact resources, 8 negative checks, 327712 record cases, 3 generation-storage cases, base/modified-save detection, expansion rejection, two offline metadata-fixture sessions and cold metadata reload. Registered Nox capability/mismatched variant checks also execute in this binary. Source HDV hashes unchanged.
- Storage cases use actual SaveFileManager files: two writes and latest selection; truncate the newest generation to a four-byte header and recover the previous complete generation; replace and validate afterward. This simulates an interrupted file; it is not a killed-process or gameplay-save test. Header and payload checksums cover generation metadata too.
- Gameplay `conversion/ce-m1-gameplay/20261015-111320` still FAIL overall, but trace demonstrates normal world input at frame143/PC71DC, then east movement from (24,38) to (25,38). Captured failure frame inspected: game's tutorial says `Press (O) to open the door.` This is a tutorial input dependency, not a CPU halt. M1.3 remains IN PROGRESS until modal/menu/quit acceptance completes.
- Actual preceding harness defect: stopping at common animation wait D560 and injecting Return even in world mode prevented reaching normal world input. Diagnostic pause now ignores this shared wait after a world scene appears. The next run demonstrably reached PC71DC and moved, so that correction has evidence.
- New source in build 47260: respond to the post-movement tutorial with O and north through normal CE events; record nonzero PCM sample count, require it in harness; at the completed command boundary serialize actual state, initialize/decode a fresh Runtime, compare reserialized bytes, reject truncation, and persist one actual-state snapshot fixture through SnapshotStore. Codec checks and modal completion remain unverified until that run passes.
- Engine save/load UI, session-qualified gameplay slots, cold gameplay continuation, recovery after interrupted actual gameplay saves, audible/pause/quit acceptance remain incomplete. No M1 VERIFIED claim.
### Actual Open action and serializer defect reproduced

- Build 47260 PASS: 0 warnings/errors, 2m35s. Gameplay artifacts `conversion/ce-m1-gameplay/20261015-111808` contain its exact executable hash/invocation.
- Run reached phase5 and returned to world input at frame162, position (25,38). Failure PNG inspected: door is open; game text says `Open Door`. Ordinary CE O/north input answered the tutorial/action. This establishes movement and action evidence, but the whole harness correctly FAILED on its later serialization negative check.
- Actual world-boundary state encoded, fresh candidate initialized/decoded, and re-encoded bytes matched exactly. Then truncating the final serialized byte was incorrectly accepted. Confirmed codec defect: Common memory stream reports end-of-stream on a short read without setting `err()`; position still equals truncated size. `Runtime::loadState` now rejects `stream.eos()` as well. No acceptance criterion weakened.
- Build 7871 active for this correction; rerun gameplay and resource/session/storage harness afterward. No gameplay snapshot fixture was written by the failing run (the negative test precedes persistence).
- Added CE `devtools/nox/ONLINE-VERIFICATION.md` with approved identifiers, verified isolated API command and exact remaining CE transport/account/save verification procedure; updated canonical AGENTS with the user's identity/credential/production restrictions.
### Latest verified checkpoint: actual gameplay and complete runtime codec

- Build 7871 PASS: 0 warnings/errors, 2m36s. Executable SHA256 `8962c1120725ad905e3abe1f11f02a50578b5b7b961e1aa47cefc7c4174ca683`.
- PASS gameplay: `conversion/ce-m1-gameplay/20261015-112122/verification.json`. 162 host iterations, 18 CE key inputs; real new game, east movement (24,38)->(25,38), Open/north action and return to normal world input. Final PNG inspected: open door and `Open Door` game text. One guarded native sector movement call executed. Audio: 727831 generated samples, 137857 nonzero; mixer queue path used. Accelerated headless route does not establish normal pacing or physical audibility.
- Same run now passes actual full-state byte-identical fresh-candidate codec round-trip AND truncated-input rejection after EOS fix. Persisted actual-state fixture: `conversion/ce-m1-gameplay/20261015-112122/saves/nox-gameplay-snapshot-fixture.g0`, 1638762 bytes including generation envelope. It contains user resource/private runtime state and must remain local outside Git. This is not yet an Engine save slot or cold-process continuation test.
- PASS resource/session/storage on SAME binary: `conversion/ce-m1-validation/20261015-112136/verification.json`: 74 exact files, 8 negative cases, 327712 native record cases, 3 storage cases, detection matrix, two offline fixture sessions/cold metadata. Original HDV hashes unchanged by both passing harnesses.
- An initial resource harness run launched concurrently with gameplay (`ce-m1-validation/20261015-112122`) FAILED before report: SDL_BlitSurface reported invalid src. Sequential retry passed immediately. Do not run multiple CE SDL-dummy processes concurrently on this host; serialize engine harnesses. No engine acceptance claim is based on the failed attempt.
- PASS current `git diff --check`, Python harness syntax, recovery document synchronization; website tests remain 27/195 PASS. No push, deployment, original-resource change or M2 implementation.

| Bounded subtask | Status | Actual evidence / remaining work |
| --- | --- | --- |
| M1.3a | VERIFIED | 112122 actual CE opening/movement/Open PNG and trace, above |
| M1.3b | VERIFIED | Native Windows SDL host run 121746 opens/closes real global menu then session-aware quit |
| M1.4a | VERIFIED | Nonzero PCM measured in actual gameplay run and queued to mixer |
| M1.4b | VERIFIED | 121746 normal clock/audio pacing, PauseToken stability/resume, actual menu/quit; cold Engine loads clear old audio |
| M1.5a | VERIFIED | Resource/session harness, offline fixtures only |
| M1.5b | VERIFIED | Isolated website registration/API tests, no live CE transport claim |
| M1.5c | VERIFIED | Actual CE queue sync/replay, two cold gameplay saves, independent hermesbox database audit, 120800 artifacts |
| M1.6a | VERIFIED | Actual quiescent state byte-identical candidate round-trip and truncated payload rejected |
| M1.6b | VERIFIED | Three actual SaveFileManager storage fixture cases |
| M1.6c | VERIFIED | Fresh-process identical frame and ordinary left movement in 112804 run, details below |
| M1.6d | VERIFIED | Actual save/load chooser plus cold two-session slot1 API checks pass, 122933 artifacts |
| M1.6e | VERIFIED | Actual owned-process kill during half-written gameplay overwrite; fresh recovery and live-state corrupt-load rollback, 123350 artifacts |

All parent tasks retain their full criteria. M1.3/M1.4/M1.5/M1.6 remain IN PROGRESS;
M1.7 NOT STARTED; M1 overall IN PROGRESS. M2â€“M4 NOT STARTED. No running build/test
sessions at this checkpoint. Continue the active objective without requesting a new
prompt. Native standalone sources/binaries remain preserved.
### Cold-process continuation verified; Engine integration compiling

- Previous goal turn classified as progress: implementation and tests changed authoritative source/evidence; no repeated external-blocker restatement.
- Build 96618 PASS: 0 warnings/errors, 2m40s. Binary SHA256 `e12f188a7690876e2c6ed3be935d71845e7ef27a7820ca03c2d2f4d9d89bdebe`.
- PASS `conversion/ce-m1-gameplay/20261015-112804/verification.json`: new process loaded the 1.6MB actual snapshot fixture, restored identical PNG bytes, then ordinary CE left input moved (25,38)->(24,38) and returned to world input. Both process invocations/executable hash recorded, originals unchanged. M1.6c VERIFIED. This binary predates the Engine slot changes below.
- M1.6d IN PROGRESS: `engines/nox/saves.h` adds bounded metadata (target/session/description/logical slot/playtime), hashes physical keys by target/session, groups two generations into one slot, and requires matching CE sidecar before accepting a committed generation. No original resources are written.
- `engines/nox/nox.{h,cpp}` now implement Engine save/load availability and hooks, startup load, candidate decoding before runtime replacement and mixer queue reset on load. Writer preserves the latest committed generation, removes only the other generation's stale sidecar before reuse, verifies body then commits existing CE sidecar. `snapshot.h` accepts an optional protected generation index for this transaction.
- `engines/nox/metaengine.cpp` implements logical slots in save mode and cached distinct file IDs in load mode, using CE's existing compact-load/filter mechanism. `engines/metaengine.h`: saveMode overload of listSaves becomes virtual; default implementation/behavior unchanged for other engines. This requires a broad rebuild; no M2 classic/modern policy or enhancement-state implementation.
- Extended gameplay driver prepares actual A/B states using normal left input, starts two offline sessions with approved Nox identifiers, saves logical slot1 twice in each, then cold loads both through Engine/MetaEngine APIs; harness requires separate reports, frame equality, session identity and empty audio queues. These new acceptance checks are NOT YET VERIFIED.
- Build session 36685 active. Source received a subsequent phase-transition correction (queue the B move before awaiting its completed world boundary), manual-only autosave declaration, and a shared-header comment while compiling. After this build, rerun the same script to ensure all newest sources are incorporated before claiming verification. Do not start a second simultaneous build.
- Next: finish/reconcile build, diagnose compiler errors if any, rebuild newest source, run gameplay+Engine cold tests sequentially, then failure-injection/lifecycle checks. Live CE HTTP criterion alone remains BLOCKED as previously documented; independent M1 work continues.
- Source review found that `loadPopupStateForSave` restores only popup state, not active session identity. Engine loader now prevalidates the session registry, decodes a fresh candidate, calls existing `recordSaveLoaded` to activate the original session, then restores popup state and swaps the runtime/clears audio. Writer verifies the sidecar again after `recordSaveCreated` completes. These corrections require the newest-source rebuild mentioned above.
- Confirmed shared-interface audit: only Nox defines the saveMode listSaves overload outside MetaEngine; making that overload virtual does not activate unintended overrides in existing engines (rg across engines). M1 disables autosaving explicitly; M2 saving policy remains deferred.
- Known deferred management edge to audit before broad release: existing CE delete-without-saves clears only the selected generation sidecar; Nox backup generations carry their own sidecars. Full archive/delete/recovery matrix is M2, not implemented here. Do not claim complete lifecycle isolation from M1 snapshot tests.

### Hermesbox authorized; live identity defect under correction

- User supplied `http://hermesbox:8123/` and explicitly authorized creating a dedicated test account. Verified host `hermesbox`, Laravel environment `local`, and exact application URL through dedicated SSH account/key. Existing application models registered only the approved Nox game/variant and a new verified non-admin test user with revocable token. No server source deployment, migration, production account or production database changes.
- Private credential directory is outside source and artifacts at `C:/Users/aaron/AppData/Local/Temp/codex-nox-m1-hermesbox-r2d1v8j6`; Windows ACL restricted. Use its `ce-private.ini` for tests; never copy credential values into reports. HTTP Link and Nox detail returned 200. Token remains available for continued testing; revoke after testing is finished.
- New CE helpers: `devtools/nox/test_online.py`, `provision_hermesbox.py`, `verify_hermesbox.py`; existing `test_gameplay.py` accepts external private configuration. Local isolated-server preparation also passed HTTP checks and shut down afterward.
- Build 58873 passed (23 warnings in broad build, 0 errors). Offline gameplay artifact `conversion/ce-m1-gameplay/20261015-114447` passes two actual Engine session-qualified logical-slot1 writes per session, then cold-process Engine loads with identical frames, distinct positions/original client identities and cleared audio queues. This verifies Engine APIs, not actual chooser UI or failure transactions.
- Build 59372 passed (0 warnings/errors, 2m37s), SHA256 `6cad1bbdb1b649f8bbbc83eab065792b3b6324ea94f3353c757af1013714097f`. Actual hermesbox run `conversion/ce-m1-hermesbox/20261015-115107` passed local gameplay/cold checks but FAILED independent database audit: CE associated both distinct client sessions with the same server ID, and uploaded both sessions' saves under server session A. Server created distinct sessions correctly; session B had no saves. Zero achievement definitions/events and non-admin account checks passed. This run does NOT verify online separation; failed artifacts retained.
- Shared CE correction now compiling in build 44465: captured request serial plus immutable event key rejects stale callbacks and resolves queue entry by identity; recursive state mutex protects request claiming/callbacks and relevant session/save operations. Start responses must match requested client/game/variant before binding a server ID. Suspected trigger is timer-thread callbacks racing with main-thread retry across the idle transition. Correction requires fresh live evidence.
- Existing `recordSaveLoaded` accepts optional human label; Nox supplies saved description so physical generation filenames do not overwrite website labels. Existing callers retain defaults. No credential-bearing queue payload introduced.
- Next: finish build; require all online queue entries synchronized plus distinct registry server IDs; rerun independent server audit of client identity, ownership, logical slot, loaded metadata and durable file SHA256. Then complete remaining host/UI and interruption criteria. M1 remains IN PROGRESS; M2â€“M4 NOT STARTED.

### Hermesbox transport, immutable replay and server separation verified

- Build 44465 PASS: 33 warnings, 0 errors, 8m10s. Binary SHA256 `6e390d3cd5c2a7955a7d286cb40f5bf9cec4a27256d2946ea8d31585cdddef1a`.
- Same-binary offline gameplay/cold Engine checks PASS at `conversion/ce-m1-gameplay/20261015-120642`; resources/detection/session/storage PASS at `conversion/ce-m1-validation/20261015-120709`. Original HDVs unchanged.
- Actual CE hermesbox gameplay/cold run PASS at `conversion/ce-m1-hermesbox/20261015-120650`; independent `server-verification.json` PASS: two distinct matching server/client identities, one logical-slot1 row in each session, correct human labels, loaded timestamps, matching actual durable file SHA256, non-admin user and zero achievement definitions/events.
- Added explicit lost-acknowledgement test: reset only local queue transport status to pending, preserving each event ID and every signed payload field. Fresh CE process resends original events, then cold loads both saves again. Harness requires all events synchronized, original attempt counts increased and immutable payloads unchanged. PASS at `conversion/ce-m1-hermesbox/20261015-120800`; independent database audit also PASS, still exactly two matching sessions and one save each. M1.5c and M1.5 VERIFIED.
- One intervening launch `120740` FAILED before gameplay with SDL_BlitSurface invalid source. No concurrent ScummVM process existed, so earlier attribution solely to concurrency is unproven; intermittent SDL-dummy startup defect remains recorded. Sequential retry passed full replay/server audit. Do not count the failed run as acceptance evidence or mask it with automatic retries.
- Remaining M1: actual chooser interaction, normal pacing/pause/menu/quit and interruption/corrupt-load transaction tests; final destination/regression. No M2â€“M4 implementation or production changes.

### Normal host lifecycle driver compiling

- New `engines/nox/host_validation.h` supplies test-only ordinary Escape input after the actual global main menu has paused the engine for 100ms, with unchanged CPU-clock assertion. It unregisters its event source on exit. Runtime exposes a read-only CPU clock counter.
- `nox.cpp` can continue the cold Engine test through the normal elapsed-clock loop (no accelerated boundary stepping), measuring one second of CPU/audio pacing, holding a real PauseToken for 150ms, resuming, opening/closing the global menu, then issuing an ordinary quit event. Required report `nox-host.tsv` is written only after all phases and quit complete.
- `test_gameplay.py --host-lifecycle` requires that report; ordinary harness behavior stays available. Build 23648 is active for these sources. These host criteria remain IN PROGRESS until actual reports pass. Next command: normal gameplay harness arguments plus `--host-lifecycle`, then diagnose failures without weakening criteria.
- Build 23648 subsequently PASS: 0 warnings/errors, 2m39s. First host run `conversion/ce-m1-host/20261015-121222` timed out in its cold process; no host PASS report. Source inspection confirms CE active sessions always require a separate exit dialog even with confirm_exit=false. New test source clicks that dialog's actual Quit button, leaving completion unchecked, and logs intermediate phases to diagnose any remaining hang. This explanation is source-derived; the first run did not persist a phase proving which modal dialog blocked. Build 82015 active for correction and diagnostics. Host acceptance remains IN PROGRESS.
- Build 82015 FAILED on a protected GUI getTopDialog accessor. Removed that dependency; the test now uses the existing CE exit dialog's fixed button geometry from its constructor. Build 48599 active. No generic GUI visibility/API changes made.
- Build 48599 PASS: 0 warnings/errors, 2m33s. Dummy-video launch `conversion/ce-m1-host/20261015-121733` FAILED before gameplay with the intermittent SDL_BlitSurface invalid-source issue. Added explicit `--native-video` to use the Windows SDL video backend; no retry masking or acceptance relaxation.
- PASS `conversion/ce-m1-host/20261015-121746/verification.json`, command `test_gameplay.py` with standard executable/game/artifacts plus `--host-lifecycle --native-video`: actual opening/cold Engine tests, then 1011ms ordinary elapsed-time pacing with 998483 CPU clocks and generated audio, 150ms real pause with unchanged CPU/generated samples, resume with increasing clocks/samples, real global menu paused for 100ms and closed via Escape, ordinary quit accepted through existing CE session exit dialog without marking complete. Required `nox-host.tsv` PASS. SDL audio is dummy; generated nonzero PCM/mixer behavior tested, physical listening not claimed. M1.3b/M1.4b and parents M1.3/M1.4 VERIFIED.
- Current remaining M1: actual chooser save/load, interrupted gameplay overwrite/corrupt-load rollback, final launcher/regression evidence. Next use native Windows video for host/UI tests; keep intermittent dummy startup limitation visible.
- Host-verified build 48599 SHA256: `b5e4bfafac79882ba31fd595bb9ff57d842e3e57041dc7d95f5809e533d30c11`.

### Actual chooser driver compiling

- Added `test_gameplay.py --chooser-ui` and Nox diagnostic flag to invoke actual `Engine::saveGameDialog()` / `loadGameDialog()` after the cold session tests. Test-only event source selects save row1 (logical slot1) and a real compact-load entry, then clicks actual Choose buttons using the public theme layout. Includes CE's load-session filter row and list padding/spacing; no widget selection, dialog result or gameplay state is injected.
- Requires `nox-chooser.tsv` PASS and restored active session/position matching its durable metadata. New checks remain unverified. Build 43763 FAILED due local variable shadowing under repository warning-as-error settings; renamed variables. Current build 8954 is active and includes layout correction.
- Next command after build passes: standard gameplay harness arguments with `--native-video --host-lifecycle --chooser-ui`, artifacts under `conversion/ce-m1-chooser`. Diagnose actual reports/logs. Keep M1.6d IN PROGRESS until UI evidence passes; then begin M1.6e failure transaction work.
- Build 8954 FAILED during optimized code generation with C4701 on layout output `x`. Initialized every theme layout output explicitly. Source correction not yet verified. The source was updated after 8954 had compiled it; new clean-environment build 81018 is active. Do not run the old host-verified executable expecting chooser support. No task status advanced for this code.
- Previous goal turn classified as progress: actual hermesbox replay/server audit and normal host lifecycle passed, persistent evidence changed, and chooser implementation advanced; not a repeated blocker turn.
- Build 81018 PASS: 0 warnings/errors, 2m35s. Actual chooser run `conversion/ce-m1-chooser/20261015-122626` FAILED at restored session/gameplay check after completing save/load dialogs. Source and durable metadata identify a production label defect: MetaEngine prepended `Slot 1 - ` to the editable description; actual UI saved that prefix back into metadata, which also broke exact-label state classification. No chooser VERIFIED claim.
- Correction: MetaEngine exposes original human description (logical numbering already provided by chooser); driver now also requires actual slot1 generation advance and unchanged description when saving without edits. Build 92615 active. After it passes rerun `test_gameplay.py` with standard paths and `--native-video --host-lifecycle --chooser-ui`; inspect all reports before marking M1.6d VERIFIED. Then implement actual interrupted-write/corrupt-load transaction tests.
- Build 92615 PASS: 0 warnings/errors, 2m35s. `conversion/ce-m1-chooser/20261015-122933/verification.json` PASS, including actual Engine save dialog (slot1 sequence increases, description unchanged), actual load dialog restoring matching active session/position, two cold distinct session states, plus normal host menu/pause/quit. Host pacing measured 1028ms/1017923 CPU clocks. M1.6d VERIFIED.

### Actual failure transaction driver compiling

- New `devtools/nox/test_failures.py` first runs healthy offline gameplay/cold saves with native Windows SDL, then starts its own CE child for a different actual session A position (25,38)->(24,38) through ordinary left input. A test-only observer in SnapshotStore flushes half the replacement payload and publishes a ready marker, keeping the actual generation file open for a bounded 10-second interruption window.
- Python kills only its owned Popen child while the file is partial; requires the prior committed generation's exact SHA256 unchanged and no sidecar commit for the incomplete generation. It then launches a fresh recovery process, requires prior (25,38) state/session/frame, prepares a checksummed yet truncated-runtime save, and requires failed Engine load with byte-identical live runtime and unchanged active session. Invalid oversized save must also return failure. No original HDV or standalone source is written.
- New diagnostic modes are scoped to the isolated nox-gameplay-test target for partial write injection. Ordinary writes have no observer. Snapshot envelope/serializer versions remain unchanged. These tests remain IN PROGRESS, not VERIFIED; build 85086 active. Next: `python devtools/nox/test_failures.py --exe build-scummvm/Releasex64/scummvm.exe --game "D:/Codex Projects/Nox Archaist" --artifacts "D:/Codex Projects/Nox Archaist/conversion/ce-m1-failures"`.
- Build 85086 PASS: 0 warnings/errors, 2m33s. Actual `test_failures.py` PASS at `conversion/ce-m1-failures/20261015-123350/failure-verification.json`: owned child killed with half a real replacement payload flushed, prior committed file SHA256 unchanged, incomplete generation has no sidecar; fresh CE process restores original session A (25,38) and identical frame, not attempted new (24,38). Checksummed truncated-runtime candidate is rejected through Engine load; complete live runtime reserialization and active session stay unchanged. Oversized save request returns failure. Original HDV hashes unchanged. M1.6e and M1.6 VERIFIED. Fault test artifacts deliberately retain invalid injected generation; do not treat them as ordinary user saves.
- Remaining M1.7: actual launcher UI launch (current harness launches configured target directly), final same-binary gameplay/chooser/host/failure/resource/session suite and hermesbox audit, standalone regression/hash preservation, coherent final limitations. M1 overall IN PROGRESS, M2â€“M4 NOT STARTED.

### Final launcher acceptance compiling; standalone preservation rechecked

- New `devtools/nox/launcher_validation.h` is a test-only source compiled with ENABLE_NOX and activated only by explicit isolated launcher-validation configuration with nox-gameplay-test present. It presses the actual selected launcher's Start widget and the existing CE session-name dialog Start button. Launcher close records a transient proof only after the selected target becomes active. No dialog result or active target is injected by the driver.
- `test_gameplay.py --launcher-ui` omits the direct target from its first process, uses the existing last-selected-game setting, and requires `nox-launcher.tsv` after actual opening gameplay/durable save. Nox reuses the real launcher-created session as A, then creates B normally; cold continuation still uses direct startup. Launcher UI test is offline; online transport remains independently audited.
- Driver also now checks full runtime/session equality after a rejected oversized save request. Build 18119 FAILED because the test helper attempted a protected Dialog::findWidget accessor. Launcher now passes its own Start widget through its existing protected access; no GUI visibility change. Current build 78637 active.
- Next after build: `test_gameplay.py` with standard executable/game/artifacts plus `--native-video --launcher-ui --chooser-ui --host-lifecycle`, artifacts `conversion/ce-m1-final`. Then rerun failures/resources and hermesbox queue/replay/server audit on that final binary.
- PASS fresh six standalone existing-binary regressions, `conversion/ce-m1-standalone-final-verification.json`: every test binary still matches its baseline SHA256, standalone nox_live SHA256 unchanged, both original HDVs unchanged. No standalone rebuild claimed and no standalone source modified.
