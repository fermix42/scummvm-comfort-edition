# Strict portable storage verification — November 8, 2026

This revision preserves the existing executable-relative defaults and adds a
permanent boundary for managed overrides and common filesystem mutations.
The implementation audit and directory structure are in [STRICT-STORAGE.md](STRICT-STORAGE.md).
Earlier RESULTS.md receipts describe the former external-override policy and
are not counted as fresh tests here.

## Build identity

- Development branch: `codex/ce-2026.4.0-consolidated`.
- Build: this worktree's checked-in `run-build-release-cleanenv.ps1`, Windows
  Release x64 solution; final incremental compile/link passed, 0 warnings and
  0 errors. Complete executable and enabled engine set retained.
- Executable SHA256: `c1de17d5ecacc75d31b7fcc8a0d1f45370948944ce14843674d6a28c65d6d8c0`.
- Local build includes the pre-existing uncommitted Gooka/build changes. The
  storage commit deliberately excludes those unrelated edits; this executable
  is a validated local working-tree build, not a reproducible binary of that
  commit alone and not a published GitHub release.
- Only the eight reviewed runtime files change for strict storage. Their exact
  Git blobs are recorded in `strict-runtime-blobs.json`; preservation checks
  retain the previous immutable baselines everywhere else.

## Fresh automated coverage

| Requirement | Actual verification |
| --- | --- |
| Fresh launch without INI/marker | Concurrent normal startup and executable-copy boundary suite |
| Configuration creation/modification | Normal `--add`, game settings, all seven legacy storage keys and per-game savepath normalized |
| Save/load | Actual Nox saves and relocated cold restores; MI4 Windows/PS2 native banks and cold restores; AGS game checkpoint and cold restore |
| Autosaves | Primordia's own `autosave` slot 2; standard AGS timer autosave observed where scene availability permits; Nox's existing no-autosave policy preserved |
| Screenshots/logging/cache/temp/download directories | Normal log/temp plus native storage/screenshot/probe checks |
| Achievements/sessions | Real Nox registry, queue and sidecars; MI4 session/bank ownership and cold continuity |
| Independent installations | Separate normal configs, original save hashes unchanged |
| Relocation/unrelated CWD | Unicode/space directory copies, original address parked, actual cold continuation; executable-root custom path serializes as `.` |
| Read-only installation | Disposable ACL denies only writes, executable remains runnable; clear error and no fallback; ACL restored |
| Official installations | External sentinel config preserved; controlled and real profile observation; no migration/import |
| Upgrade | Executable replacement preserves local config and exact save bytes |
| Escape boundaries | External CLI config/save/log/screenshot/icon/DLC destinations, dot traversal, sibling prefixes and directory junction rejected |
| Future merges/packages | Raw API inventory/source contract, pinned preservation checks and both Windows build/release workflow runtime checks |

The normal suite passed **92 named checks** on the executable hash above.
The standalone boundary suite passed **5 source checks and 13 actual launches**.
CLI smoke passed **3 checks** (version, engines, detection catalog). Observer
classification passed **5 tests**. MI4 activation/protocol passed **4 + 8 checks**;
both Windows and PS2 gameplay cases passed, each including **24 portable storage
checks**, actual native saves, separate session banks and cold restores.
Fresh AGS results and preservation-audit receipts accompany this report.
The inherited source-preservation suite passed **10 tests** and the ancestry/
runtime audit passed **9 checks**, covering **22,139** inherited runtime/resource
paths against their original or explicitly pinned strict-storage blobs.
See [verification summary](strict-evidence/verification.json),
[normal](strict-evidence/normal.json), [boundaries](strict-evidence/boundaries.json),
[MI4](strict-evidence/mi4.json), [AGS](strict-evidence/ags.json) and
[filesystem observations](strict-evidence/filesystem.json).
The complete prepared candidate (189 files, including DLLs, resources and
licenses) also passed the same **92 checks**, including copying and relocating
the entire installation; see [complete package](strict-evidence/package.json).

## Filesystem evidence and external activity

Normal launches use executable-relative storage with no `--test-cli` flag.
Gameplay-controller cases retain their pre-existing explicitly owned disposable
test roots. Recursive native directory notifications observe both controlled
profiles and the real Windows profile. CE stream tracing stays in the active
portable root; the Python observer collects it after execution. Any CE-traced
profile write or unknown controlled-profile application artifact fails checking.

Observed tests found no CE-owned controlled-profile files and no ScummVM
notifications beneath the real profile. The normal suite separately recorded
NVIDIA driver-cache activity. Native gameplay observation recorded Windows SAPI
shared user-lexicon files. These are retained as external system resources;
they are not claimed absent or redirected by CE. Prior Direct3D cache/journal
categories remain classified but should not be counted as newly observed unless
present in the fresh receipt.

Directory notifications are not process-attributed kernel I/O traces. Trace
record counts can include histories copied with disposable installations and
are not counts of distinct OS write calls. Raw notifications stay in local
`strict-validation-local` receipts; committed summaries omit unrelated real-user
profile event details.

## Limits

Representative games do not prove every optional engine/vendor/debug code path.
Raw C/vendor diagnostics remain source-audited rather than universally hooked;
the inventory detects new/changed sites requiring review. Optional WinSparkle
configurations are not enabled or runtime-validated in this CE build. No engine
or existing gameplay function was disabled to obtain these results.

External legacy save selections now use the local default; original files are
untouched and require deliberate manual copying if wanted. Blind registry/sidecar
merges are unsafe. External exports through the common write API are rejected.
The path checks are application safeguards, not an adversarial security sandbox
against concurrent directory replacement or hard links. Live cloud synchronization,
all-game playthroughs and physical desktop/DPI/controller acceptance are outside
this storage verification. No publication, main-branch merge, live installation
or modification to official ScummVM data was performed.

A complete-package run at a deeply nested workspace path hit the existing
Windows/C-runtime path-length limit when writing a long Nox achievement sidecar
under an additional custom save subdirectory. CE reported the failed save and
did not use a profile fallback. Use a reasonably short writable installation
root (for example `D:\Games\ScummVM CE`); long-path awareness alone does not
guarantee every C-runtime/library call accepts arbitrarily deep filenames.
