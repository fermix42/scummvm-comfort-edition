# Codex Notes

## Building ScummVM

This local checkout is intentionally not built from the ambient shell `PATH`.
Use the checked-in clean environment build script from this repository root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\Codex Projects\scummvm-private\scummvm-native-test-cli\run-build-release-cleanenv.ps1"
```

The script sets up the expected Windows build environment:

- creates `S:` as `subst S: "D:\Codex Projects\scummvm-private"` when needed
- points `PATH` at the Visual Studio 2022 BuildTools MSBuild/CMake/Ninja locations
- sets `VCPKG_ROOT`, `VCPKG_DEFAULT_TRIPLET`, `VCPKG_INSTALLED_DIR`, `VCPKG_OVERLAY_TRIPLETS`, and `VCPKG_OVERLAY_PORTS`
- reuses the populated `vcpkg_installed` tree from the sibling non-Git checkout unless this Git clone has its own
- generates `build-scummvm\scummvm.sln` if it does not already exist
- builds the release x64 solution
- writes the detailed build log to `D:\Codex Projects\scummvm-private\scummvm-native-test-cli\msbuild-release.log`

The expected executable is:

```text
D:\Codex Projects\scummvm-private\scummvm-native-test-cli\build-scummvm\Releasex64\scummvm.exe
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
