# Native Snatcher integration — November 21, 2026

The isolated combined candidate is functionally qualified at the affected CE
integration boundaries and ready for an authorized merge review. It is **not
cleared for ordinary Windows gameplay or release**. The persistent Windows
slowdown remains unresolved. No merge, deployment, installation refresh, push,
publication, driver change, or full campaign replay was performed.

## Baseline and patch

Authoritative checkout: `scummvm-ce-2026.4.0-consolidated`, branch
`codex/ce-nox-official-integration`, clean HEAD
`eb4ec66b6b1044f689a042ecb976a825cc01f55e`. The candidate was created from that
exact state as `ce-snatcher-integration-20261121` on
`codex/ce-snatcher-integration-20261121`. No historical reset was necessary.
The authoritative checkout and both stable owner installations were preserved.
There is no Nox production-source difference from the baseline.

The handoff was located under `D:\Codex Projects\Snatcher\private`, rather
than beneath its development worktree. The integration review, functional audit,
baseline JSON and patch receipt were read before applying the patch. Patch SHA256:
`fe153011ffa086c8b0da728f834293cd405da1a5841e0d9b30c47459044a0d08`.
It applied without conflicts: 271 engine files and 12 shared files. All 283
reviewed paths exist. After line-ending normalization, 277 match the reviewed
new files; the six intentional differences are listed below. The frozen source
and executable were not altered or substituted for the combined build.

## Integration corrections

* `engine.h`: Snatcher returns -1 for its autosave slot and false for its
  autosave capability. `metaengine.cpp` also returns -1, so slot 0 is not
  reserved or exposed as a framework autosave slot. The existing early
  `autosave=true` rejection in the native save implementation remains intact.
  Periodic, immediate and pre-load framework autosaving therefore cannot write
  a Snatcher save. Manual slots 0–99 retain their names and schema.
* `engine.cpp`: a CLI-only fixed diagnostic exercises periodic/immediate
  framework gates, checks both slot policies, and submits automatic requests
  for slots 0 and 99. It does not synthesize save payloads.
* `module.mk`: removed the duplicate `pcm_shared.o` entry.
* `tests/CMakeLists.txt`: enabled assertions in Release component tests.
* `tests/fm_publication_tests.cpp`: corrected the stale expectation that FM
  schema 6 was unsupported. The frozen implementation supports 1–8; the test
  now accepts that range and rejects 9. No codec or schema changed.

The build script adds explicit `-SnatcherIntegration` and `-SnatcherTests`
modes. Snatcher remains default-off and unstable in normal engine discovery.
The reviewed event provenance, optional read-only 64-bit storage-capacity API,
short-read resampler correction/tests and test-CLI changes were retained.
The mixer was not replaced. A reviewed inventory update records only imported
vendor/read-only and standalone-test sites; no existing site was removed.
The detailed storage review is retained with the local validation evidence.

## Build identity

Complete static-engine Windows x64 Release build succeeded. Executable:

`build-scummvm/Releasex64/scummvm.exe`

SHA256:
`6a1dfc4a9c273640bbb0512a2c67d62b9f8e61ffcf74491e750e218fec031c50`

The installed MSVC 14.44.35207 toolset and SDK 10.0.26100.0 match the frozen
toolchain. The compiler binary identity is checked against the handoff in the
local evidence; there was no intentional compiler/toolset/SDK change.
Shared workspace vcpkg dependencies were used, rather than replacing CE's
dependency tree with the frozen Snatcher archive.
Compiler, linker and resource-compiler hashes match the frozen baseline;
all 1,622 frozen dependency files also match the shared dependency tree exactly.

The first compilation caught a diagnostic attempting to access private Engine
timer fields. It was corrected to use public framework entry points with a
nonzero autosave interval in the isolated profile. Both build logs are retained.
The successful finishing invocation reports 3 warnings and 0 errors, taking
3:33.80; the initial compile took 7:06.68 and also emitted imported-source
warnings. This is not a warning-free-build claim. The final warnings concern
existing exception settings and vendored Huffman shifts. All required engine
objects, both FM implementations and decoder dependencies linked successfully.
The diff whitespace check reports retained imported license/source whitespace;
those reviewed bytes were not reformatted as part of integration.

## Fresh qualification

All runtime receipts identify the executable above. Tests used bounded pipes,
owned disposable roots, SDL dummy video/audio, and read-only original media.

| Boundary | Result |
| --- | --- |
| CE CxxTest | 458 tests passed, including the imported short-read downsampling regressions. |
| CE native CTest | 2/2 passed. |
| Snatcher component CTest | 62/62 passed with assertions enabled: resource transports, codecs, original game services, both FM implementations, input, mouse/DPI bounds, scheduling and frame publication. |
| Engine preservation | Existing 115 playable and 128 detection engine IDs all retained. Candidate has 116 and 129 respectively; the only addition is `snatcher`. |
| Normal detection / Add Game | Original CHD and multi-BIN/CUE detected and added as `snatcher-us-segacd`, engine/game ID `snatcher`, platform `segacd`. |
| Snatcher runtime / saves | Both FM configurations passed on both transports. Manual 0 and 99 save/load/enumeration passed; hashes stayed unchanged after automatic requests. Requests against absent slots created no autosaves. Retained diagnostic predecessor loaded, exact warm save/restore probes passed, and manual slot 0 restored in a cold process. |
| Resources / mixer | All 20 audio-track lengths and decoded first/last endpoints match between original CHD and multi-BIN/CUE. Runtime FM probes and save audio-continuation checks passed. |
| Input / scenario | Native keyboard and mouse input, mapped controller pause/menu selection, focus loss/gain, repeat suppression, wheel and outside-area requests passed. An actual retained native command menu changed reference with mapped controller Down and returned with keyboard Up. |
| Options | Default uncensored mode off; original-controls-only disables mouse while preserving mapped controls. Optional uncensored mode reads the existing external sidecar, validates its resource inventory, and retains manual save/load. Sidecar was unchanged and never bundled. |
| Portable storage | Five source guards passed after explicit site review. Fresh launch-boundary checks passed for external-path rejection, Unicode/spaces, unrelated CWD and relocation. Runtime paths remained beneath owned roots. Capacity probes preserve unsupported-versus-zero semantics and manager error state. |
| Nox / CE | Established `test_ce_integration.py` passed detection, native title, nonzero title music/effects, New Game, mapped Open/cancel, rendered world frame, save, quit and cold restore. Original-message wording guard passed. |

Original four-slot compatibility is supported by unchanged game-service source,
passing native game-service/backend component contracts and the frozen functional
evidence. It is distinct from CE manual slots 0–99. The broad completed Snatcher
campaign and original-save route evidence remain historical evidence for unchanged
gameplay logic; they are not represented as a new campaign on this executable.

## Reproduction and evidence

Use this isolated checkout's clean-environment script:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\run-build-release-cleanenv.ps1 -SnatcherIntegration
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\run-build-release-cleanenv.ps1 -UnitTests
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\run-build-release-cleanenv.ps1 -SnatcherTests
```

Runtime drivers are `devtools/ce/test_snatcher_{detection,integration,options,menu}.py`,
`devtools/nox/test_ce_integration.py`, and `devtools/ce/test_strict_storage.py`.
Each provides explicit executable, media, checkpoint/sidecar where applicable,
and new owned output-directory arguments. Game data and optional sidecar remain
external. Reuse of an output directory is deliberately refused.

Local evidence resides in `validation/snatcher-integration-20261121/`, including
source identity, toolchain, registration and transport comparisons, build/test
logs, storage review and a reproducible run record. Runtime transcripts, receipts
and diagnostic save copies reside beneath `build-scummvm/Releasex64/validation-*`.
Failed diagnostic attempts are preserved separately; successful final directories
are `validation-snatcher-detection-r2`, `validation-snatcher-chd-r2`,
`validation-snatcher-cue`, `validation-snatcher-options-controller`,
`validation-snatcher-menu-cold`, and `validation-nox`.
No game data, optional game-derived sidecar or private diagnostic saves are
included in the source checkpoint.

## Remaining limits and recommendation

No integration failure remains in these affected-boundary tests. The unresolved
Windows slowdown is still a release and ordinary-use blocker, including the
reported post-process desktop effects. No real-device Snatcher gameplay,
unrestricted GPU/audio stress, driver/settings experiment or reopened slowdown
investigation was performed. Physical-device breadth, subjective presentation,
non-MSVC/dynamic-plugin configurations and exhaustive CUE packaging remain outside
this static Windows integration qualification.

The isolated combined source/build is ready for the owner's separate merge
decision. That recommendation does not authorize deployment or establish release
readiness. Both stable owner-facing installations remain the normal launch paths.
