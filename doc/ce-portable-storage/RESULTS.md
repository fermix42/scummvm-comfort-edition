# Windows native portable storage verification — November 6, 2026

Implemented locally on `codex/native-portable-storage`, starting from validated
Native Test CLI commit `80f136b4e8d662225d132041d134d3acf0c079b7`.

- Main implementation: `ae037b606de8f475a208d40b1647fcf5bc1192d1`.
- Final application/probe implementation: `81108692d45f1704a5b0e353a43e45557987d716`.
- Subsequent reporting/test-helper commits do not change production sources.
- Tested Release x64 executable SHA256:
  `c2ab488b51ab30f16a91656e413c35358edfa6e42d05fd76096dfe3465969cdf`.
- Production sources were checked byte-for-byte against the implementation
  commit after runtime verification. The executable hash is its definitive
  identity; this is not a claim about an embedded Git-version string.

The isolated worktree is
`D:\Codex Projects\scummvm-private\scummvm-native-portable-storage`.
Nothing was pushed, published, installed or merged. Accepted installations,
the integration candidate and Native Test CLI worktree were not built or edited.

## Behavior and inventory

The executable directory owns `scummvm.ini`, `saves`, `screenshots`, `logs`,
`cache`, `temp` and managed `data` by default. CE registries, achievement queues,
sidecars, Nox generations and MI4 banks retain their existing save-relative
ownership and serialization. No portable-mode toggle or legacy auto-import exists.

[README.md](README.md) documents path precedence, CLI versus INI relative-path
semantics, writable-root failures, moving the folder and manual legacy import.
[INVENTORY.md](INVENTORY.md) maps original producers to their new locations;
[the source reference index](engine-write-sites.json) includes disabled code and
diagnostics separately from runtime coverage.

All **138 baseline engine-enable defines** remain present, with none removed or
added. No engine or production feature was disabled to obtain passing tests.
The save serializers for MI4, Nox, SCI/SQ5, Cine and MacVenture were not changed.
Additional audited engine writes now use portable preference/cache overlays or
the save manager, preserving bundled files as read-only fallbacks.

## Build and test results

| Verification | Final result |
| --- | --- |
| Normal Windows Release x64, checked-in clean-environment script | PASS; final incremental build: 0 warnings, 0 errors |
| First complete Release build | PASS; 67 inherited diagnostics, including the prior Native CLI shadowing warning; no new storage-code warning |
| Existing CxxTest suite | **442 tests passed**; the build hook and explicit invocation both passed, counted once |
| CE standalone CTest executables | **2/2 passed**: SQ5 battle options and MSVC warning contract |
| Normal-launch portable storage/relocation | **50 named checks passed** |
| Native CLI activation/isolation and protocol | Both renderer runs passed their 4 activation and 8 protocol checks |
| MI4 Windows, TinyGL/headless | PASS: actual gameplay, native-menu A/B saves, ownership, thumbnail metadata, cold A/B restores |
| MI4 PS2, TinyGL/headless | PASS: actual gameplay, native-menu A/B saves, ownership, thumbnail metadata, cold A/B restores |
| MI4 Windows, hidden classic OpenGL | Same gameplay/save/bank/cold-restore checks PASS |
| MI4 PS2, hidden classic OpenGL | Same gameplay/save/bank/cold-restore checks PASS |
| Portable storage in each MI4 scenario | **24 additional storage checks per scenario**, including screenshots, fixed cache/temp probes, nested data directories, auxiliary redirection rejection and read-only config failure |
| Nox resource and offline-session runner | PASS: **7 process invocations**, **74 exact resource files**, **2 sessions**, create/cold metadata checks |
| Nox normal portable Modern gameplay | PASS within the normal-launch relocation fixture: movement, real saves, paired generations and cold continuation |
| Nox Classic gameplay, metadata and host lifecycle | PASS, including **4 independently inspected mode generations**, cold restoration, owner contracts and metadata/UI checks |
| Preservation baseline | **39/39 recorded files unchanged**, including accepted binaries, current legacy INI, previous MI4 test saves, older eXo saves and the reused Nox source image |

The CLI's owned-root enforcement remains intact. Relative JSON game paths are
canonicalized before diagnostic working-directory anchoring. Edited owned
profiles cannot redirect screenshots, downloaded data, cache or temp writes to
an external directory. Probe filenames are fixed, and existing probe files are
rejected. The normal application continues to honor explicit storage overrides.

The read-only configuration test verifies a reported failure, unchanged original
hash and retained `.tmp` recovery file. Its copied fixture uses extended Windows
paths so long MI4 sidecar destinations are exercised rather than shortened.
No ACLs were changed. Two normal processes also raced on startup directory
creation successfully.

## Relocation and filesystem observations

The normal-launch fixture uses a complete installation at `A portable 日本語`,
adds Nox through the normal `--add` command, creates real gameplay saves and CE
metadata, and copies everything to `B relocated Ω`. It then removes the original
A address by parking that disposable directory elsewhere. B launches from an
unrelated working directory, restores actual gameplay and both session owners,
and preserves achievement sidecar owners, popup state and progress. The restored
frame matches the original frame. New default stream writes stay beneath B.
The old A saves remain unchanged. Absolute external game-data references remain
intact, as required.

Explicit per-game/global/CLI save paths, config, logging and temporary paths were
tested. Relative INI save paths follow the executable; relative explicit CLI
config/save paths retain the original caller-directory meaning. Invalid portable
directory components produce an actionable error and preserve the conflicting
file. There is no fallback to a Windows profile directory.

Actual observations combine recursive `ReadDirectoryChangesW` notifications on
the real and controlled profiles with opt-in native stream-write tracing:

- Normal portable launches: **50 checks PASS**, including per-launch default
  destination checks and no ScummVM notifications under the real profile.
- Headless MI4 matrix: **216 successful native stream-write records**, all beneath
  the disposable validation area; no application files in the controlled profile
  and no real-profile ScummVM notifications.
- Offscreen MI4 matrix: **220 successful native stream-write records**, with the
  same destination/profile checks passing.
- NVIDIA-named driver cache activity was observed and retained in the raw
  notifications; the normal fixture recorded **37 system/driver events**. These
  are reported separately, not represented as absent or redirected by CE.
- Native desktop observations recorded zero visible process-owned windows in
  the MI4 runs; offscreen helper windows stayed hidden.

Directory notifications are not process-attributed kernel I/O traces. Background
activity under the real profile is retained in local raw receipts and is not
ascribed to CE. The observations support the tested application paths, not a
universal guarantee about every third-party engine, library or OS component.

## Evidence and optional human acceptance

Committed summaries are in [evidence/verification.json](evidence/verification.json),
[relocation](evidence/relocation.json), [headless](evidence/headless.json),
[offscreen](evidence/offscreen.json), [Nox resources](evidence/nox-resources.json),
[Nox Classic](evidence/nox-classic.json),
[engine coverage](evidence/engine-coverage.json) and
[preservation](evidence/preservation.json).

Raw transcripts, observed filenames, screenshots, saves, failed-pilot receipts
and the fresh acceptance folder remain in this worktree's ignored
`portable-validation-local`. Failed pilot checks were corrected and rerun; only
the final passing scenarios are summarized above.

The complete fresh folder for optional human acceptance is:

```text
D:\Codex Projects\scummvm-private\scummvm-native-portable-storage\portable-validation-local\human-acceptance\scummvm.exe
```

It contains the matching executable, runtime DLLs, trusted engine data, a minimal
relative `extrapath=data` configuration and a hash manifest. It contains no
imported personal saves or link credentials. Copy the complete folder when
relocating it. This folder was prepared, not installed into an accepted location.

The build output itself is
`build-scummvm\Releasex64\scummvm.exe` beneath this worktree.

## Remaining acceptance boundaries

- No pre-repair MI4 seed was available. Real current-format Windows/PS2 saves
  passed cold restoration; older-user-save acceptance remains outstanding.
- HPL1, Harvester, Director and QDEngine storage changes were source-audited and
  compiled with all default engines. Full gameplay acceptance for those paths
  needs representative assets. Generic cache/directory probes are not claimed
  as HPL1 gameplay verification.
- Live achievement synchronization, account linking and a newly earned award
  were not exercised. Durable queues, session ownership and sidecar state were;
  native CLI network isolation remains unchanged.
- A protected Program Files installation and truly read-only media were not
  modified to manufacture a test. Invalid components, read-only config files,
  atomic failure reporting and actual writable-temp probing were exercised.
- Interactive global-options editing and human monitor/DPI/window-layout
  acceptance were not automated. Normal configuration/game registration and
  hidden Windows rendering were exercised.
- Optional WinSparkle is absent from both the validated baseline and this build;
  its persistence is not certified by these runs. OS shell bookkeeping, GPU
  driver caches and system crash infrastructure remain outside CE-owned storage.
- There is no automatic importer or registry merger. Follow the documented
  copy-and-backup procedure and preserve target identities and sidecars.
