# Consolidated ScummVM CE 2026.4.0 — release readiness

Recommendation: **ENGINEERING-COMPLETE; READY FOR FOCUSED HUMAN ACCEPTANCE.** No material consolidation regression is confirmed. This does not approve publication, installation, live synchronization or the remaining human checks. This branch is the proposed authoritative development baseline, subject to the user's acceptance.

## Identity and confirmed ancestry

- Branch: `codex/ce-2026.4.0-consolidated`.
- Worktree: `D:\Codex Projects\scummvm-private\scummvm-ce-2026.4.0-consolidated`.
- Package preparation/source commit: `e66fc0817b621d849c71fa548a4497e79b3d67ed`.
- Final local delivery SHA: recorded in `delivery.json` beside the ZIP and appended to its sealed copy of this report after the reporting commit. Subsequent commits contain test helpers/documentation, not application changes.
- Fresh Windows Release x64 executable SHA256: `8cd74f6dd386c991dd2a4e9983e0bd4135ae99e45c26607fac46bf6f6818d80c`.

| Milestone | Confirmed ancestor |
| --- | --- |
| Official upstream `v2026.4.0` | `7d2709da5136866589f7602f5f5a7b17de03420e` |
| Validated integration | `2d0de89e8b875852a0aca2c69a2de9b3c8b445b9` |
| Completed Native Test CLI | `80f136b4e8d662225d132041d134d3acf0c079b7` |
| Completed native portable storage | `d9868b2d7bc10be6752c6b9899f01c8aea20c3cd` |

All three milestones were already present through normal ancestry. No duplicate merge, cherry-pick, reset or change to historical branch pointers was necessary. Runtime source/resources remain identical to the portable milestone. The executable was freshly built here; Git-blob comparison and its hash establish identity without making assumptions about an embedded Git-version string. Historical receipts are retained separately and are not counted as fresh tests of this executable.

## Complete preservation audit

**22,139 runtime source/resource blobs are exact**, all upstream paths remain present, and all **138 baseline engine-enable defines** remain enabled. Generated Release projects have real compile inputs. No engine, adapter, assertion, serializer or production feature was removed to force a pass.

| Required CE feature group | Exact files | Fresh runtime boundary |
| --- | ---: | --- |
| Achievements, sync services, sessions and ownership | 6 | Local service/queue/sidecar tests; live sync separate |
| Grim/MI4 banks and existing adapter | 303 | Four actual gameplay/save/bank/cold-restore scenarios |
| Nox Archaist | 307 | Modern, Classic, resources and offline sessions |
| SCI, SQ5 Trash Talkin and SCI video | 218 | Importer/native logic and bounded startup; interactive gameplay separate |
| Deja Vu/MacVenture | 35 | Exact preservation; representative gameplay requires human acceptance |
| Operation Stealth/Cine | 50 | Exact preservation and rule/service coverage; gameplay separate |
| SCUMM/Indy3 boxing | 425 | Exact preservation; physical gameplay separate |
| Launcher, cheats and themes | 222 | Exact resources/code and existing unit/service checks |
| SDL layouts, fullscreen, scaling and positioning | 100 | Geometry/service checks and hidden Windows rendering; human display/input separate |
| Native CLI entry points/adapter | 3 | Protocol, isolation and gameplay scenarios |
| TinyGL/headless | 37 | Actual rendering/gameplay |
| Portable filesystem/save/config layers | 91 | Normal launches, overrides, observation and relocation |
| Branding/distributed resources/packaging inputs | 752 | Source identity and complete-package checks |

Groups overlap; they are not summed into a misleading total. All ten tracked uncommitted files in original main still match preserved CE snapshot `29e2ceb01611ac18654d82bfe2116db8183bd53d` after CRLF normalization. Their original on-disk bytes are unchanged. **51 recorded original files**, including accepted binaries, milestone executables, older saves and fixtures, retain their hashes. Original main remains at its original HEAD with its work intact.

Consolidation changes only policy/docs, audits, test helpers and package preparation. Windows CI/trace commands now use the validated Unicode setting; preservation checks verify these as exact transformations rather than broadly exempting them. Application code and save formats are unchanged.

## Fresh build and tests — separate counts

The checked-in clean-environment script built normal Windows Release x64: **PASS, 0 errors**, full build **00:09:46.66**. Its 67 inherited diagnostics include the prior Native CLI shadowing warning; no application source changed during consolidation. The fresh test build passed. Build-hook and explicit repeated CxxTest executions are counted once.

| Category | Fresh result |
| --- | --- |
| Existing upstream/CE CxxTest methods | **442 passed** |
| CE standalone CTest executables | **2/2 passed**, including SQ5's eight cheat combinations |
| SQ5 importer boundaries | **5 passed** |
| Observer classification boundaries | **5 passed**, including rejection of CE profile files |
| Upstream/CE preservation tests | **10 passed**, with immutable portable-adaptation pins |
| Consolidation ancestry/source/engine audit | **9 checks passed** |
| Packaged CLI smoke checks | **3 passed**: version, engines, detection catalog |
| Native CLI protocol/isolation | Two renderer runs, each 4 activation and 8 protocol checks |
| MI4 actual gameplay scenarios | **4 passed**: Windows/PS2 × headless TinyGL/hidden classic OpenGL |
| Additional MI4 storage checks | **24 per scenario**, including screenshot/cache/temp/nested data, auxiliary isolation and read-only config |
| Windows native Load-menu scenario | **1 passed**; actual menu/confirmation, no direct CLI load command |
| Complete-package D: relocation | **50 checks passed** |
| Complete-package D:-to-C: relocation | **50 checks passed** |
| Nox gameplay | Modern and Classic passed; **4 independently inspected saved-mode generations per runner** |
| Nox resources/sessions | **7 invocations**, **74 exact resource files**, **2 offline sessions**, create/cold checks passed |
| SQ5 enhanced/original startup | **2 bounded 12-second scenarios passed**; **2,851 WAV and 6 timing files** audited |
| Fresh human gameplay tests | **0**; historical observations are not substituted |

No combined total conflates unit methods, assertions, scenarios, files and human checks. CxxTest methods are not branch-coverage claims; the inherited `SLOW_TESTS` blending matrix remains outside the default suite.

MI4 reached actual Wedding Ship gameplay, changed the actor through normal input, created native A/B saves, decoded descriptions/160×120 thumbnails, preserved A while saving B, and restored exact state and ownership in fresh processes. Desktop observations found zero visible process-owned windows. The supplemental Windows test changed the actor after saving, chose native Load, confirmed the retail dialog and restored the saved actor and owner. No new engine adapter, evaluator, world-state mutation or OS input takeover was introduced.

Nox uses its established compatibility/gameplay fixtures; controlled owner-contract clones are not represented as earned gameplay. Nox migration milestones were not reopened. SQ5 startup/audio initialization does not prove audible quality, lip sync or interactive gameplay; its established smoke harness deliberately terminates the bounded processes after observation.

## Portable behavior and actual observations

Configuration, saves/autosaves, sessions, achievement queues/progress/sidecars, screenshots, logs, cache, temp and managed engine data default beneath the actual executable. No toggle, launcher workaround, AppData fallback or auto-import exists. Explicit overrides remain supported. Relative INI storage follows the executable; relative explicit CLI filenames retain caller-directory meaning.

Both relocation runs copied the **complete prepared package**, including DLLs, resources, themes, docs and licenses, to Unicode A. Normal launches without test mode registered Nox and created actual saves/CE metadata. The full folder moved to B, the old A address was removed by parking that disposable copy, and B launched from an unrelated working directory. Game state, session identities and sidecar ownership/progress survived; restored frames matched. New default writes stayed beneath B and old A save hashes remained intact.

One B stayed on D:. The other executed at `C:\ScummVM-CE-Consolidation-Validation\e66fc081-complete\B relocated Ω`. These are fresh test folders, not permanent game installations. External absolute game-data references were preserved, not assumed valid on another computer. Final acceptance outputs remain on D:.

Tests used an OS-only PATH and absolute Python/CE executables. Import closure and application-local DLL checks supplement those runs. CLI owned-root restrictions remain intact; edited test profiles cannot redirect auxiliary writes, and fixed diagnostic files cannot be overwritten. Read-only config errors preserve the original and retain recovery data. No ACLs were changed.

Stream tracing and recursive real/controlled-profile notifications found **no CE-owned profile writes in the observed operations**. Headless/offscreen matrices recorded 216/220 successful native stream writes. The cross-drive run separately retained NVIDIA cache events, Direct3D databases/journals, shared SAPI user-lexicon activity and empty shell-cache directories. No feature was disabled or OS cache/lexicon redirected to manufacture a pass.

Microsoft documents [D3DSCache as a Windows DirectX component](https://github.com/microsoft/DirectX-Specs/blob/master/d3d/ShaderCache.md) and the [SAPI user lexicon as shared across applications](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee125054%28v%3Dvs.85%29). CE's speech backend creates an `ISpVoice` and contains no lexicon registration/pronunciation-writing code. OS-resource classification is an inference from observed paths, SDK usage and primary documentation, not process-attributed kernel tracing. Unknown files still fail observation. **Every traced CE write into the controlled profile is rejected even if its filename resembles an OS resource.** Raw notifications remain available; this is representative measured coverage, not a universal library guarantee.

## Complete local portable package

The existing CE packager supplied the executable, Release dependencies, Microsoft CRT, all distributed engine-data groups, CE themes/branding, networking/virtual-keyboard support resources, licenses and notices. The curated folder contains **61 DLLs**, **43 dependency notices**, and an expected **190 files** after the final report/changelog. The sealed manifest and delivery metadata give the authoritative count.

No commercial game data, personal saves, credentials, validation images, test profiles or runtime fixtures are shipped. Internal layout-test profiles/launchers were removed from the newly created output. The only `scummvm.ini` is the fresh `[scummvm]` / `extrapath=.` template. Writable directories are created on demand. No installer was made.

Output directory: `release-preparation-local\candidate` beneath this worktree.

- Executable: `scummvm-comfort-edition-windows-x64\scummvm.exe`.
- ZIP: `scummvm-ce-2026.4.0-consolidated-candidate-windows-x64.zip`.
- Per-file SHA256: `BUILD-MANIFEST.json` and `SHA256SUMS.txt` inside the folder.
- Archive SHA256: the ZIP's `.sha256` sidecar.
- Final commit/archive hash/locations: `delivery.json` beside the ZIP.
- Final report and human checklist are also copied beside the ZIP.

Application/DLL/resource bytes are the tested payload. Final docs and delivery metadata are sealed afterward; every ZIP entry is CRC/hash checked. Final archive round-trip and copied-package checks are recorded in an external archive-verification receipt, avoiding a circular ZIP/self-checksum claim.

## Final acceptance classification

| Area | Classification | Remaining action |
| --- | --- | --- |
| Ancestry, source/resources, all default engines | **Verified** | No further consolidation work |
| Build, suites, CLI isolation, MI4 saves/banks/cold restoration | **Verified** | Physical input/display separate |
| Nox modes/resources/sessions and cross-drive relocation | **Verified** | No campaign-wide claim |
| Windows native Load menu/confirmation | **Verified** | Headless Windows scenario; human experience separate |
| PS2 native Load/profile navigation | **Requires human acceptance** | Use retail UI; API restoration is already verified |
| Earned achievement hooks and rendered popups | **Requires human acceptance** | Legitimate supported-game event and repeat/presentation check |
| Live authorized staging synchronization | **Unavailable test data** | No staging account/token supplied; no production credentials read or sync traffic sent |
| Older/pre-repair MI4 compatibility | **Unavailable test data** | Use legitimate copied seeds if available; do not conclude they are lost |
| Representative SQ5/Cine/MacVenture/Indy3 gameplay | **Requires human acceptance** | Short edition-appropriate checks, not complete playthroughs |
| Representative newly supported upstream title | **Unavailable test data** | No suitable assets found in the inspected library/fixture locations |
| Human fullscreen, monitor/DPI, cursor/controller | **Requires human acceptance** | Focused checks using available hardware |
| HPL1/Harvester/Director/QDEngine storage gameplay | **Existing limitation** | Inherited paths audited/compiled; representative assets needed for full gameplay acceptance |
| Optional WinSparkle/shared Windows speech/OS-driver state | **Existing limitation** | WinSparkle absent in baseline/build; OS resources are not CE progress |
| Protected Program Files/read-only media | **Existing limitation** | No protected installation modified; invalid components, writable-temp probes and read-only config were exercised |
| Material consolidation regression | **Confirmed regression: none** | Investigate reproducible new failures before approval |

Use [HUMAN-ACCEPTANCE.md](HUMAN-ACCEPTANCE.md) for the focused checklist. Missing data or human input does not become an automatic pass or conceal a confirmed regression.

## Evidence and delivery boundaries

Fresh summaries under `doc/ce-consolidated/evidence` cover source/original preservation, separate test counts, MI4 matrices/native Windows Load, Nox modes/resources, SQ5 startup and relocations. Raw logs, screenshots, profiles, saves, failed-pilot diagnostics and notifications remain in ignored `consolidation-validation-local`; they are not distributed. Historical reports remain unchanged and separate.

The branch/worktree is left clean after local reporting commits. Nothing was pushed, published, installed, copied into a permanent game installation or merged into original main. Promotion to the accepted baseline and public release remain the user's decisions.
