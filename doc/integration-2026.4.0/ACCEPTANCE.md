# Final regression audit and human acceptance preparation

Recommendation: **READY FOR HUMAN ACCEPTANCE**. This means the engineering audit
has found no integration-caused defect requiring a production change. It does
not approve the upgrade or assert that the remaining manual checks have passed.
See [the required and recommended checklist](HUMAN-ACCEPTANCE.md).

## Candidate and preservation

| Item | Verified identity |
| --- | --- |
| Starting branch | `codex/scummvm-2026.4.0-integration` |
| Starting candidate | `d2ae980043da2906ccf4548f330ef6520b1f02c4`, initially clean |
| Official release | `v2026.4.0`, `7d2709da5136866589f7602f5f5a7b17de03420e` |
| Acceptance test/tools commit | `bb454edf5f7e6755244fb3fafa39b405cf35ce3e` |
| Final candidate commit | The subsequent documentation commit containing this report; full SHA supplied in chat delivery and by `git rev-parse HEAD` |
| Rebuilt executable SHA256 | `7ed5bee7b1f415655930c9054a7974b74d7a2055311239d320be2181952e8d2f` |
| Preserved original candidate SHA256 | `65f81176d29e82e5f3b131494cea294ac19ac137a7e3dd444731278af612ea76` |

The production application source is unchanged from the starting candidate.
Changes are confined to regression tests, test-backend support, an optional
compile-only warning-audit build mode, audit tooling and documentation. The
existing preservation audit explicitly records the two test-backend adaptations.
There was no merge, configuration/save migration, engine exclusion, warning
suppression, production account change, installation, push or publication.

The source/test commit above is also recorded in the portable package's manifest.
The application was rebuilt from identical production sources before that test
commit. Later documentation does not alter the executable. The original binary
and runtime DLLs are retained under `acceptance-validation-local/baseline-binary`.
The accepted CE `main` checkout and its binaries were not built or modified.

## All 66 original compiler warnings

[The complete location/count table](WARNING-CLASSIFICATION.md) and
[machine-readable occurrence evidence](warning-classification.json) account for
all 66 original diagnostics across 35 source locations. Repeated final build
summaries and blank/template-context lines are excluded from that count.

| Primary category | Occurrences | Evidence |
| --- | ---: | --- |
| Existing upstream source warnings | 31 | Matching historical CE compiler diagnostics and unchanged source, or tagged-source targeted compilation |
| Existing CE source warnings | 2 | Original snapshot SCI source reproduces both warnings with Release settings |
| New integration-caused warnings | 0 | None established |
| Preexisting toolchain/dependency diagnostics | 33 | 15 vendored ImGui native-width format diagnostics; 18 MSVC singleton inline diagnostics, all observed historically |
| Origin not established | 0 | All five initially unclassified occurrences were reproduced |

Source provenance is a separate dimension: 64 occurrences are in upstream
release source and two in CE additions. The 33 toolchain/dependency diagnostics
are a subset of those 64, not 33 additional warnings.

Historical evidence names the exact preserved logs, including
`mi4-before-controller-build.log`. Five diagnostics lacked sufficient historical
matches. Compile-only projects extracted SCI/Fool files from snapshot
`29e2ceb01611ac18654d82bfe2116db8183bd53d` and Phoenix VR from the exact official
release. They used the real Release project flags and integrated headers; they
were not standalone full baseline builds. Successful compile logs reproduce
the same locations/codes/messages. These are source/compile evidence, not
runtime tests. Phoenix VR's two warnings became visible when GIF-dependent
engine coverage was restored; tagged-source reproduction shows their upstream
origin rather than an interface defect introduced by CE.

### Risk review

| Diagnostic / locations | Assessment and limits |
| --- | --- |
| C4775, ImGui memory editor `%IX` | MSVC extension for native-width `size_t` hexadecimal input, not a mismatched destination pointer. A new actual-UCRT runtime test passes maximum 64-bit, above-32-bit, zero and invalid-input cases. Portable format cleanup is outside scope. |
| C4506, `Singleton<ConnectionManager>::makeInstance` | Repeated MSVC inline-template diagnostic in historical builds. The template body exists and the final link resolves dependencies. No unresolved symbol was reported. This does not establish live HTTP behavior. |
| C4146, OpenGL stencil masks | `-1u` deliberately produces the all-bits unsigned mask. Defined unsigned arithmetic; no new pointer/lifetime issue. |
| C4146, Director transition offsets, Rebel Assault RNG, Tinsel coordinate reconstruction | Existing signed/unsigned negations. Director uses translated/clipped image coordinates; Tinsel masks reconstructed coordinates; Rebel uses bounded RNG results. Conversion/wrap portability remains inherited technical debt. No source change or newly triggered defect was found; broader gameplay is not proven by inspection. |
| C4018, EEM archive indices | The compared index is masked to nonnegative 0..127 before comparison and loading. No negative-to-large conversion on this path. |
| C4018, Freescape | A 4/16-color bounded loop and nonnegative C64 address/size checks. The diagnostics do not establish a new out-of-bounds path. |
| C4018, CE SCI `fadeMazePlayfield` | Signed loop index compared to unsigned `_displayPixels`. Current SCI surface dimensions are bounded far below INT_MAX; original CE source reproduces it. Extreme/corrupt dimension behavior is not certified by this audit. |
| C4245, Fool `FlushEvents(-1, 0)` | Intentional all-events mask converted to unsigned, reproduced from the original snapshot. |
| C4334, Director debugger window flags | The shift uses the 14-entry fixed window flag list (indices 0..13), so it does not reach the sign bit or an invalid shift width. Future list growth requires revisiting this bound. |
| C4334, Lua table sizing | `twoto`/`sizenode` shifts are bounded by MAXBITS and the table-overflow guard (30 or less on this target). No newly introduced invalid shift was identified. |
| C4065, Director/Phoenix VR/TeenAgent | Default-only switches in this build configuration, not missing default handling. Existing/tagged source reproduces the diagnostics. |
| C4189, CE SCI atlas local `c` | Unused computed local; it does not drive rendering or ownership. Exact original CE source reproduces it. No warning-only cleanup was made. |
| C4611, Lua/Squirrel setjmp | Genuine inherited C++ portability/destructor-unwinding concerns. Lua already replaces throw sites with its error path; Squirrel retains protected compiler/regex paths. No integration source change affects them. This audit does not claim a general proof of safe longjmp/RAII behavior for arbitrary scripts. |

No warning indicates a newly introduced truncation, dangling pointer, missing
symbol or broken integration. This is an evidence-based provenance assessment,
not a claim that all upstream technical debt is universally safe.
The four existing upstream test warnings in span/blending are separate from
the original application's 66; their test source is unchanged. The three span
conversions deliberately exercise negative/overflow bounds. The blending C4551
comes from `(void)areSurfacesEqual`, an intentionally unused helper reference in
the default `#else`, not an accidentally omitted call in an active test body.
The native test
build's NDEBUG override keeps assertions active and is not warning suppression.

## Build, tests and counting

The established isolated clean-environment script rebuilt Windows Release x64
successfully: **0 errors, 0 warnings in the incremental rebuild**, 00:02:42.55.
This does not replace the original clean build's 66-warning classification.
The original detailed log is preserved as `msbuild-release-integration-original.log`;
the new log is `msbuild-release.log`. A first incremental link attempt failed
because the running UI test held `scummvm.exe` open (LNK1104). Closing the test
and retrying resolved it; the baseline binary was preserved before relinking.

| Verification | Passed | Failed | Scope |
| --- | ---: | ---: | --- |
| CxxTest | 442 | 0 | Original 434 plus eight new CE cases; zero framework-reported skips |
| Native regressions | 2 | 0 | Existing SQ5 eight-combination executable plus actual CRT format contract |
| SQ5 importer tests | 5 | 0 | Existing deterministic importer boundaries, rerun |
| Preservation/engine audit | 8 | 0 | Both histories, upstream tree, CE customizations and complete default-engine build coverage |
| Rebuilt executable CLI checks | 3 | 0 | Version, linked engines and detection catalog; repeated only as final build validation |

Total: **460 counted checks passed, 0 failed**. The two cold-process driver
invocations repeat two of the eight new CxxTest methods in separate processes;
they are additional execution evidence, not two more tests in the total.
Compile-only warning projects, package import checks, GUI observations, individual
assertions and retained Nox suites are not added to that count.
These are framework-method counts, not branch-coverage counts. In particular,
upstream's exhaustive `test_blend_blit_unfiltered` matrix remains compiled out
unless `SLOW_TESTS` is defined; the method is still counted by CxxTest. It was
not newly disabled in this phase, and this report does not claim that matrix ran.

New production-API coverage includes:

- Actual builtin Operation Stealth event-to-achievement mapping; correct session
  ownership, wrong-variant refusal, duplicate event suppression and no-token retry.
- Independent-session repeats and completed/abandoned session refusal.
- Progress sidecar persistence without changing the test save payload.
- Master cheat/Challenge Mode transitions and permanent assisted-session history.
- All 160 Windows and four PS2 native slot boundaries, session/target namespaces
  and collision-copy names, using the production helper functions.
- Two fresh processes writing then restoring the actual CE queue, session identity
  and progress sidecar, then retrying the same event without duplicating it.

The backend fixture uses the real filesystem save manager and isolated paths;
it does not add hooks or fake behavior to the production engine. Popups are
disabled for null-backend service tests. Therefore rendered popup/repeat suppression
and earned gameplay hooks still require manual acceptance. MI4 helper tests are
not game loading, metadata parsing, gallery or save-format runtime passes.

Initial test-driver corrections are recorded: missing configuration files do not
clear the global config manager, so each case now writes and loads a fresh isolated
INI. Session IDs use time/millisecond identity; two programmatic creates in one
timer tick collided, so the human-workflow fixture separates creations by 20 ms.
This inherited same-tick API limitation was not hidden by a production change.
An initial assertion also confused currently enabled cheats with permanent
session assistance; the test now checks the correct registry/queued-event contract.

## Synchronization and retained Nox evidence

Implementation preserved: yes. Offline service behavior verified: yes, including
fresh-process durability and ownership. Live synchronization verified in this
phase: **no**. Historical staging credentials were explicitly revoked according
to the retained online-verification instructions. No new token/account was
created, no credential was printed/copied into reports, and no production sync
traffic was sent. The manual checklist specifies the approved staging procedure.

All four Nox receipts still match the preserved original candidate's SHA256.
Their recorded production source is identical to this candidate: no Nox, CE core,
GUI, backend, save-format or runtime logic changed in this phase. The relinked
binary has a new hash, so the old receipts are not misrepresented as fresh runs
of that new binary. No Nox rerun or M4 migration work was performed; original
images, save generations and local evidence were preserved.

## Windows launcher/display observations

The actual integrated Windows executable was controlled through the computer-use
skill with separate configuration/save paths. Confirmed observations:

Launcher/display observations below used the preserved starting binary before
the relink (SHA256 `65f81176...612ea76`), whose production source is unchanged.
Resumed Windows gameplay and the PS2 title were observed using the rebuilt
portable binary (`7ed5bee7...52e8d2f`). The runtime evidence records these bindings
separately rather than presenting every observation as a fresh run of both binaries.

- Titled custom window survived quit/restart. Capture bounds were 1186×893 at
  (207,100); Windows' invisible frame accounts for the difference from the requested
  outer 1200×900 geometry. The borderless capture was exactly 1200×900 at (200,100).
- A separate fullscreen startup rendered the launcher on the existing 5120×1440
  display, with aspect-preserving side bars. Returning to a separate windowed
  configuration worked; no monitor/DPI/desktop configuration was changed.
- Launcher selections, detected-target preferences, CE Cheats tab, master gating,
  platform-specific patches, Achievements settings, Challenge Mode disabling
  cheats, its warning and cancellation behavior were actually observed.
- Initially hand-written targets lacked detection GUI metadata, so their Cheats
  tabs were absent. Normal `--add` detection produced the proper options. This
  fixture setup issue was corrected rather than classified as a regression.

Interactive button hit testing worked in the launcher. Negative monitor origins
are covered by geometry unit tests only; actual secondary-monitor placement,
other DPI settings and in-game corner/cropping coordinate mapping remain manual.
Alt+Return was not observed to toggle the launcher during a bounded attempt;
fullscreen startup was verified independently. A live in-game fullscreen toggle
is still on the checklist, not claimed as passed.

## Assets, MI4 and representative upstream engines

Search was limited to known project fixture directories, the known historical
MI4 worktree location, and the installed MI4 game-data directories. The historical
MI4 junction target is no longer available. Prior runner logs show successful
older tests but do not retain their input saves. No compatible existing Windows
`.gsv` or PS2 `efmi*.ps2` seed was found there. Personal directories were not searched
in this phase. These missing seeds explain the earlier unrun regression runners.

Both legitimate game editions detect correctly. Windows was launched through
the actual engine; its original introduction and first interactive ship scene
with the normal action choices were observed. PS2 independently reached its
animated `Press Start` title screen using classic OpenGL. Both test processes
closed normally through the window close control. A bounded attempt to obtain
a normal gameplay seed did not establish a save/load pass.
No world state was injected or expected outcome manufactured. Final edition-specific
startup observations and remaining gaps are recorded in the linked checklist and
local engine logs; seed-dependent runners remain pending unless separately recorded.

Automated F1/Ctrl+F5 actions did not demonstrate native menu operation. Whether
that reflects injected input delivery, game/controller mapping or a product issue
was not established, so native menus, real keyboard/controller input and save/load
remain unverified rather than passed. If the same issue occurs with normal human
input, stop approval and treat it as an engineering investigation. The observed
startup rendering is narrower evidence than playable input/save acceptance.

Two early startup attempts used only a copied executable/DLLs and omitted the
required `monkey4-patch.m4b` search path. The source support file exists unchanged;
setting the isolated extra-data path resolves that setup error. A complete portable
candidate was then produced with the existing packaging script: **182 files**,
AMD64 PE check, DLL import-closure audit, CLI checks, licenses and hash manifest.
It includes the patch and all distributed engine-data groups. No commercial game
assets are packaged. This is a local artifact, not a release publication.

No suitable Phoenix VR, Dungeon Master, Chamber, Harvester, Avalanche, Fool, EEM
or newly supported Wintermute title exists in the inspected game-data root.
Their already-proven compiled registration was not relabeled as new runtime
coverage. Detection/startup/save-load gameplay for those engines remains an
**unavailable test-data** gap; no commercial data was downloaded.

## Outstanding items and recommendation

| Outstanding item | Classification | Approval action |
| --- | --- | --- |
| Earned achievement hooks, rendered popup suppression and live staging sync | Unverified behavior; current credential unavailable/revoked | Perform required achievement checklist with approved staging account |
| MI4 Windows/PS2 actual save-bank, gallery and older-save restore | Missing seed data and incomplete gameplay verification | Run each existing runner with a legitimate seed, then inspect visible native/gallery UI |
| In-game cursor mapping, fullscreen toggles, alternate monitor/DPI setups | Unverified behavior / environment coverage | Essential current-display checks required; extra hardware configurations recommended |
| Representative customized-game behavior | Unverified gameplay beyond retained tests | Short Deja Vu/Cine/SQ5 checks from copied saves |
| One representative newly supported engine | Unavailable game data | Test one when legitimate data is available; do not require all engines |
| Existing compiler technical debt and same-tick programmatic session IDs | Preexisting limitations, not integration regressions | Retain documented bounds; no unrelated cleanup in this phase |

There is no established new production regression requiring further engineering.
The candidate is **READY FOR HUMAN ACCEPTANCE**, with the required checks still
pending product approval. Keep the accepted CE installation in use until approval.
