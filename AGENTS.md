# Codex Notes

## Building ScummVM

This local checkout is intentionally not built from the ambient shell `PATH`.
Use the checked-in clean environment build script from this repository root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\Codex Projects\scummvm-private\scummvm-ce-2026.4.0-consolidated\run-build-release-cleanenv.ps1"
```

The script sets up the expected Windows build environment:

- creates `S:` as `subst S: "D:\Codex Projects\scummvm-private"` when needed
- points `PATH` at the Visual Studio 2022 BuildTools MSBuild/CMake/Ninja locations
- sets `VCPKG_ROOT`, `VCPKG_DEFAULT_TRIPLET`, `VCPKG_INSTALLED_DIR`, `VCPKG_OVERLAY_TRIPLETS`, and `VCPKG_OVERLAY_PORTS`
- reuses the populated `vcpkg_installed` tree from the sibling non-Git checkout unless this Git clone has its own
- generates `build-scummvm\scummvm.sln` if it does not already exist
- builds the release x64 solution
- writes the detailed build log to `D:\Codex Projects\scummvm-private\scummvm-ce-2026.4.0-consolidated\msbuild-release.log`

The expected executable is:

```text
D:\Codex Projects\scummvm-private\scummvm-ce-2026.4.0-consolidated\build-scummvm\Releasex64\scummvm.exe
```

Do not try to rediscover `msbuild`, `cmake`, `ninja`, `bash`, `make`, `g++`,
`cl`, or `nmake` from the ambient `PATH` first.

## Nox Archaist migration work

For Nox migration tasks, first read `devtools/nox/project/AGENTS.md` and the
architecture decision, implementation plan and progress document in that folder.
Canonical editable copies are in `D:\Codex Projects\Nox Archaist` as documented
there. Reconcile the two copies and actual Git/build evidence before resuming.
M1 and M2 implementation/testing are authorized (M2: user's October 15, 2026
"proceed" after M1 completion); continue unblocked current-milestone tasks autonomously
and update progress after meaningful work. M3–M4 implementation, remote pushes,
website deployment and destructive repository operations require separate user
authorization. Preserve all unrelated instructions and engine behavior.

Nox M2 completion is recorded in devtools/nox/project/SCUMMVM-CE-M2-VERIFICATION.json. When progress says M2 VERIFIED, stop before M3/M4 until explicitly authorized; preserve the completed evidence and current working tree.


## Native gameplay test CLI

Prefer the opt-in native test interface for supported gameplay regressions. Read doc/ce-test-cli/README.md and RESULTS.md for protocol, isolation, tested renderer/engine boundaries and current evidence. Use this worktree's clean-environment script. Keep the validated integration worktree and accepted binaries unchanged. Do not use desktop input automation for these tests.

## Permanent CE storage policy

Strict portable storage was accepted as functionally complete on November 8,
2026. It is permanent CE architecture. The accepted implementation commit is
ea764d2d390; Gooka integration must retain it unchanged. Release preparation may
create local candidates, but no push, remote tag, branch merge or publication
is authorized until the owner approves the final readiness checkpoint.

ScummVM Comfort Edition is portable by default. Application-owned persistent
state belongs beneath the executable directory permanently. External managed
storage overrides are rejected on the CLI and normalized to local defaults in
legacy INIs; game-data asset paths remain independent. Normal testing must not use the Windows user
profile as a storage destination. Use disposable portable installation copies
or the native CLI's existing owned isolated roots. Preserve accepted binaries,
legacy profiles and saves. Never add a silent AppData/Documents fallback.
Read doc/ce-portable-storage/README.md before changing storage policy.
Run devtools/ce/test_strict_storage.py after storage or upstream changes. Review
every raw-write inventory change; never refresh it merely to silence a failure.

## Consolidated development baseline

For release publication, read `doc/ce-release/PUBLICATION.md`. Publish only the
owner-approved immutable candidate using `devtools/ce/publish-approved-release.ps1`.
Permanently retire legacy GitHub workflow ID 374228258 before any release-tag
push: old tagged commits still contain the automatic publisher. Never temporarily
disable/restore it, rebuild during publication, or replace approved artifacts.
Publication and remote workflow configuration require explicit owner authorization.

Use this `codex/ce-2026.4.0-consolidated` worktree for subsequent CE work, subject
to the user's final acceptance. It contains upstream v2026.4.0, the Native Test
CLI and always-on portable storage through normal ancestry. Preserve historical
branches, the dirty original main checkout and all accepted installations.
Use the native `--test-cli` for gameplay tests whenever suitable engine
capabilities exist; keep its owned-root restrictions and hidden rendering intact.
Do not add unrelated adapters during consolidation. Read
`doc/ce-consolidated/RELEASE-READINESS.md` and `HUMAN-ACCEPTANCE.md` for the
candidate's evidence and outstanding acceptance boundaries. Never represent old
receipts as fresh tests of another executable. Local packaging is authorized;
publication, installation and merging into main require the user's decision.
