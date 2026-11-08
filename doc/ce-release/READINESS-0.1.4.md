# CE 0.1.4 release-readiness checkpoint

Prepared November 8, 2026. Publication is not authorized. No push, tag, merge or asset publication was performed.

## Source identity

Branch: `codex/ce-2026.4.0-consolidated`.
Release source and proposed `v0.1.4` tag target: `3a6a4412ed2d728b4e4f88b52f2a97d0b259ba1f`.
This readiness record is a subsequent documentation-only commit; it does not change the executable's source identity.

Logical history:

- `ea764d2d39006dc01d941dcb7f67b1ad4c30d4f7`: accepted permanent strict executable-relative storage.
- `ff0316062a95f04ed64ccd59c6eb33081f87bbb4`: recovered, pinned Gooka engine import.
- `38861abf58659b0bbd030acd01fde73bc423e761`: Windows build and package integration.
- `2ceaaf034fe084e5b19f7eb663fa096701ae5895`: importer, real-data regression coverage and documentation.
- `6879c0788f95d9908fd2e7c47a116fc4cdf70ac9`: CE version, packaging and publication guards.
- `3a6a4412ed2d728b4e4f88b52f2a97d0b259ba1f`: exact preservation-policy pins for reviewed build adaptations.

All 167 original working-tree files have hash-verified recovery copies. All 153 Gooka engine files remain byte-identical to the recovered implementation. Existing modifications were Gooka integration or its build/documentation dependencies; no unrelated source changes were incorporated. The original source and detached final-build worktrees were clean before this record.

## Build and validation

A fresh detached worktree at the release source was built using its checked-in clean-environment Windows x64 Release script: 0 errors, 67 existing-source warnings, elapsed 00:09:50.11. All 139 engine defines and 63 backend/feature defines are retained, including the additional Gooka engine. The preservation audit passed nine checks across 22,139 inherited runtime paths.

Final checks passed: 442 CxxTest methods, two CE CTest executables, and 33 Python cases covering import, packaging, storage observation, SQ5, upstream preservation and strict storage. Packaged runtime checks passed for Gooka, MI4 Windows and PS2, AGS Primordia, Nox, strict storage boundaries and CLI smoke. These include Gooka room progression 854 → 800 → 817 and cold save restoration, MI4 save-bank activation and cold restores on both platforms, AGS autosave and cold restore, 92 portable persistence/relocation checks, 13 strict boundary launches and three CLI checks. No final test failures remain.

Gooka's Czech Windows CD v1.0.3.0 data matched the existing detector. The separately supplied patch replaces the original GAME.EXE, which the native engine does not execute; it was not needed or applied. Original game archives and runtime assets were preserved.

Strict storage remains accepted and permanent. Normal CE Windows build validation and release validation run its regression gates. No AppData fallback was introduced. Official-installation isolation, configuration/save persistence and complete-installation relocation passed. No CE-owned profile writes were observed; Windows/driver caches and SAPI resources are distinguished in the evidence.

Earlier preflight failures exposed two missing exact preservation-policy registrations; these were fixed without disabling tests. An unattended Gooka intro-video pilot timed out; the final gameplay check used ordinary Escape to skip the intro. Full intro playback is not certified.

## Artifacts and reproducibility

- Executable SHA256: `9600932ff4da2e490b3ae62e904909da9075f45405bb26b73315c88dfa159850`.
- ZIP: `scummvm-comfort-edition-0.1.4-windows-x64.zip`.
- ZIP SHA256: `9993dcc02e43dd9c4bb4231c933d4d0c4d9a574a6340704ccaf5649525af494d`.
- Complete portable package: 191 files, required runtime DLLs/resources/licenses, clean local configuration, manifest and validation summary; no game data, personal saves or development logs.
- Repeated deterministic ZIP assembly produced identical hashes and passed archive CRC validation.
- Clean committed-source build identity, MSVC 14.44.35207 / SDK 10.0.26100.0 and tool/dependency fingerprints are recorded. Vcpkg is pinned to `ef7dbf94b9198bc58f45951adcf1f041fcbc5ea0`; 928 dependency inputs were verified unchanged.
- Independent bit-identical compiler rebuild verification has **not** been performed. Do not describe this executable as proven bit-reproducible.

Local evidence and deliverables are under workspace `release-validation-local/ce-0.1.4/`: `VALIDATION-SUMMARY.json`, `final-build-inputs.json`, `final-engine-identity.json`, `final-preservation.json`, runtime reports, and `candidate/release-artifacts.json`. The candidate directory contains the ZIP, checksum and completed `DRAFT-RELEASE-NOTES.md`. Source release notes are [0.1.4.md](0.1.4.md). Private game fixtures and raw runtime evidence stay outside Git and the distribution.

Remote release conventions use CE-specific `v0.1.x`; the previous release is v0.1.3. Read-only inspection found no v0.1.4 tag. Existing tags/assets were not changed.

## Remaining limitations

Gooka retains its upstream experimental/unstable designation and incomplete NPC/room behavior. Representative gameplay and native save/load passed; full-game completion, save-dialog navigation, audible audio and full intro playback are unverified. This is an experimental Gooka integration, not a full-game compatibility certification.

Retain the Windows/C-runtime path-length limitation. Recommend a short installation directory such as `D:\Games\ScummVM CE`; never route storage to a Windows profile to bypass it. Filesystem notifications are not process-attributed kernel tracing. Physical display/DPI/controller and live synchronization acceptance remain separate where unverified.

The candidate is ready for owner review with these disclosed limitations. Publication requires separate authorization.
