# CE v0.1.5 release-readiness checkpoint

Prepared November 8, 2026. This is a complete CE candidate on upstream ScummVM
2026.4.0. The existing development branch is authorized for push; publication,
version-tag creation/push and merging remain pending owner acceptance.

## Immutable candidate identity

- Branch: `codex/ce-2026.4.0-consolidated`.
- Exact committed build source / proposed `v0.1.5` target: `620232f72909f7017ea4f5eb4e9d4f87912cb6b5`.
- This report and its JSON record form a subsequent documentation-only commit.
  Branch HEAD therefore differs from build source. No build/runtime/package inputs
  change in that later commit. Use the source above for the eventual annotated tag.
- Gooka: https://github.com/fermix42/scummvm at
  `ff162f3b121aa8e9a98d5d592f5f3f16d97110a4`, verified by the existing importer.
- ZIP: `scummvm-comfort-edition-0.1.5-windows-x64.zip` (223862560 bytes).
- ZIP SHA256: `2eb43ca7eb62d49657f844093a0d2f20208d85bb78a793db3361a57afac47336`.
- Executable SHA256: `cf25c0876d336f3012a27604a7e89167ae7bb9b4be395f3eb5abd5c30fdb6a97`.
- Candidate directory:
  `D:\Codex Projects\scummvm-private\release-validation-local\ce-0.1.5\candidate\`.
- Complete candidate manifest: `release-artifacts.json`; per-file hashes:
  distribution `BUILD-MANIFEST.json` and `SHA256SUMS.txt`; external ZIP sidecar:
  `scummvm-comfort-edition-0.1.5-windows-x64.zip.sha256`.
- Draft GitHub notes with exact identity: candidate `DRAFT-RELEASE-NOTES.md`.
  Canonical changelog: [0.1.5.md](0.1.5.md).

## Complete difference from v0.1.4

Player-facing additions are Gooka Czech/English selection; all 835 mapped IDs
(819 translations / 16 intentionally retained entries); 127 editorial revisions;
English dialogue wrapping and original book-column layout; off-screen opening
subtitle support; optional aspect-preserving full-height intro/ending video.
The engine remains experimental and original Czech game files are required.
CE v0.1.5 identity is now visible in version output and About, independently of
the unchanged upstream semantic version.

Internal work includes the maintained revision-pinned Gooka importer and focused
receipts; immutable-candidate explicit publication and permanent legacy-publisher
retirement; nonrecursive gh.exe resolution and JSON-argument fixes; historical
v0.1.4 readiness records; consistent v0.1.5 metadata/notes and packaged localization
documentation/credits. The Game-tab location of the movie option is documented.
No other engine changed since v0.1.4. Existing CE enhancements and complete engine
coverage remain included, rather than being described as new features.

Exact history through the build-source commit:

```
a935413668fd07b00885745e5e6b6430ce8c773c DOCS: record CE 0.1.4 release readiness and limitations
8e4b1cac2674195babd2ac853f30feba59769054 RELEASE: publish immutable approved candidates explicitly
de2ae57a3a21bd813c75838574919a96ac9a15ae RELEASE: resolve GitHub CLI executable without helper recursion
a30420df5074147da7ff455575715c011015f3a2 RELEASE: pass JSON fields as one CLI argument
10498baabddb4052e9380881e938bfe99f15642a GOOKA: pin maintained fork with Czech and English language support
24cbd3d9936b786e02db0c3d35ccbdab8ff8cc90 TESTS: record bilingual Gooka rendering and shared-save checks
c92b67a28f78b955b00d15bfce58d1768329bcbb GOOKA: import and verify the off-screen opening subtitle fix
984503fbef7d71ab88e57fdf6dbed683e1ca96d7 GOOKA: import optional full-height AVI scaling and playback checks
77a64b2ea89df2cc51735a74d33c37442961fdb7 GOOKA: import pinned 127-entry English editorial update
620232f72909f7017ea4f5eb4e9d4f87912cb6b5 RELEASE: prepare complete CE 0.1.5 identity and portable candidate
```

## Build and focused checks

- Full Windows x64 Release solution built with the worktree's checked-in
  `run-build-release-cleanenv.ps1` from a clean committed source tree.
  Result: **0 errors, 0 warnings**, elapsed **00:02:46.21**.
- This used existing up-to-date objects and recompiled the changed version/About
  sources before linking. It was not a from-empty-output rebuild. No unrelated
  source or uncommitted changes were required; no feature was disabled.
- All compiler preprocessor definitions match the v0.1.4 build, including 139
  engine-enable definitions. All **115 registered engine IDs** match v0.1.4,
  including Gooka. (Subengine defines and registered engine counts differ.)
- Fresh checks: four release-mechanics tests, three selected engine-coverage /
  exact identity-pin checks, Gooka snapshot hash verification, generated English
  header check (835 IDs / seven retained flags), and three packaged CLI checks
  for version, engine registration and game detection catalog. All passed.
- Packager verified x64 dependency closure for the executable and all bundled
  DLLs. Distribution has **198 files**, including **61 DLLs**, engine resources,
  themes, translations, licenses and CE/Gooka documentation.
- Fresh INI contains only `[scummvm]` and `extrapath=.`. No personal config/saves,
  original commercial game files, private bilingual exports, logs, test presets
  or temporary development artifacts are included. Existing package file/hash
  guards, ZIP CRC and local check-only publisher identity checks passed.
- Existing sealer's second assembly was byte-identical. This verifies repeatable
  archive assembly from the same inputs, **not independently bit-identical
  compilation**, which remains unverified.

## Reused feature evidence, clearly separated

Four existing Gooka development receipts remain applicable to the unchanged
included engine implementation: bilingual rendering/shared saves, opening
subtitles, real AVI frame sizing/skip/completion, and the editorial import.
Their original executable hashes and source pins remain in
[VALIDATION-0.1.5.json](VALIDATION-0.1.5.json). They are not fresh gameplay tests
of this candidate executable.

Earlier v0.1.4 MI4 Windows/PS2, AGS, Nox, strict-storage and CE acceptance receipts
remain the evidence for unchanged functionality. The full 442-method suite and
unrelated gameplay/storage acceptance were deliberately not repeated.
Strict portable storage remains permanently accepted at
`ea764d2d39006dc01d941dcb7f67b1ad4c30d4f7`.

## Publication checkpoint and limitations

GitHub repository: https://github.com/fermix42/scummvm-comfort-edition . Remote
v0.1.5 tag/release were absent. Legacy workflow **374228258** remains
`disabled_manually`. Existing [PUBLICATION.md](PUBLICATION.md) and
`publish-approved-release.ps1` are the supported publication path. Its local
check-only entry point passed with these exact hashes. Publication requires
GitHub CLI on PATH; the installed directory is `C:\Program Files\GitHub CLI`.
No new publisher/workflow was introduced. No tag/release/assets/merge were made;
v0.1.4 remains untouched. The later readiness commit is documentation only.

No preparation blocker remains. Owner acceptance is required before publication.
Gooka has no full-game completion certification; seven translation context flags
remain (856, 918, 1338, 1339, 1373, 3216, 3221). Original Czech speech/video and
baked-in image text are not translated; audible audio and comprehensive save-dialog
navigation remain incompletely verified. Use a short writable installation path
such as `D:\Games\ScummVM CE`, preserve the complete portable installation/data
when upgrading, and retain the documented Windows path-length limitation without
AppData fallback. Independent bit-identical executable rebuilding is unverified.
