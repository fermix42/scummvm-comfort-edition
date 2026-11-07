# Native portable storage (Windows CE)

Windows Comfort Edition stores application-owned state beside the actual
executable by default. No adjacent INI or mode toggle is required. No normal
startup path searches AppData, Documents, Windows, the registry or the home
directory for a storage root. Explicit user storage overrides remain supported.

```text
ScummVM CE/
  scummvm.exe
  scummvm.ini
  data/                 downloaded icons/DLCs; bundled resources may also live here
  saves/                saves, autosaves, sessions, achievements, queues, sidecars
  screenshots/
  logs/scummvm.log
  cache/                generated engine caches, created only when needed
  temp/                 process-local TEMP/TMP, prepared before SDL/library startup
```

The Windows entry point discovers the module directory with a growing
`GetModuleFileNameW` buffer. Defaults are resolved independently of the caller's
working directory. Legacy relative engine diagnostics are additionally anchored
by setting the process working directory to this root. The Windows build uses
Unicode APIs; filesystem conversion buffers grow rather than truncate paths.
Long-path awareness is declared in the executable manifest; Windows policy and
individual third-party libraries can still impose path-length restrictions.

Relative explicit CLI path arguments retain their original caller-working-directory
meaning: they are converted to absolute paths before legacy diagnostic anchoring.
Storage settings in the INI (`savepath`, `screenshotpath`, `logfile`, `iconspath`, `dlcspath`,
`cachepath`, `temppath`) resolve relative to the executable directory. Internal
paths passed through ConfigManager's path setter serialize relatively. Absolute
external overrides are preserved, including game-specific save paths and CLI
`--config`/`--savepath`/`--logfile`. Existing absolute settings are not guessed to
be defaults or rewritten on read. Game-data `path`, resource `extrapath`, ROMs,
soundfonts and other read-only asset references are separate from storage.

Absent defaults are registered in the defaults domain, not copied into the INI
as build-machine absolute paths. An empty savepath uses `saves`. CLI isolation
is unchanged: `--test-cli` still requires pipes and an empty or previously owned
absolute test root. After ownership validation that root also owns screenshots,
downloaded data and temporary files, preventing test writes into the executable
installation. No arbitrary file-writing or achievement-grant command was added.
`storage` inspects effective paths; `screenshot` requests the existing renderer.
`storage_probe` tests actual cache/temp streams and nested managed-data creation
using fixed diagnostic names only; it rejects existing files and accepts no
caller-selected destination. These diagnostic probes are separate from the
actual native-menu gameplay-save tests.

Directories are created when needed, tolerating concurrent creators and rejecting
files used as directory components. `temp` is prepared early for native library
temporary APIs. Explicit custom temppath replaces the process-local TEMP/TMP
after configuration loads. HPL1 generated XML/AI caches use `cache/hpl1/<target>`. Its named `.cfg`
preferences use `data/hpl1/<target>` instead. Cache reads fall back to bundled
source XML without overwriting it. Harvester startup preferences use a portable
engine-preferences overlay, QDEngine scores use the save manager, and Director
script trace logs default to the portable log directory. An unused `cache` directory is not created.

## Move an installation

Exit CE completely. Copy the **whole folder**, including `scummvm.ini`, `saves`
and associated metadata, to the new location. Launch its executable. Internal
relative storage settings follow the folder. Absolute external storage overrides
and absolute game-data paths continue to refer to their selected locations;
change those deliberately if their destinations also move. DLLs must accompany
the executable. Keep trusted engine data accessible (for example `data` through
an explicit relative `extrapath=data`). Bundled read-only archives are not
overwritten or shadowed with generated save data.

Install in a writable folder such as `D:\Games\ScummVM CE`. A protected or
read-only destination produces a path-specific error; CE does not elevate,
change ACLs or fall back to a profile directory. Atomic Windows writes finalize
their replacement before reporting success. Failed replacements preserve the
original and retain the `.tmp` file for diagnosis/recovery. A permission error
must be resolved before retrying a save or configuration change.

## Import legacy data manually

Nothing is imported, migrated or deleted automatically. With CE closed, inspect
the old INI's global and game-specific `savepath` values and any launcher's
`--savepath`. Historical Windows defaults were
`%APPDATA%\ScummVM\scummvm.ini` and `%APPDATA%\ScummVM\Saved games`; older
upstream portable mode used `Saved games` beside its executable. Custom paths
may be on any drive. Inspect those locations before assuming data is missing.

Back up both source and destination. For an empty destination, copy the entire
old save directory, including `ce-playthroughs.json`,
`ce-achievements-queue.json`, achievement `.dat` files, sidecars, timestamps and
all game-specific generations. Preserve original filenames and target-domain
names: MI4 bank hashes and CE ownership depend on identity. Copy, never move, the
originals. Review legacy config storage paths before using the copied INI.

If the destination already contains saves or CE metadata, **do not overwrite or
blindly combine registries**. Use a separate portable installation/save directory
to inspect the legacy collection, or deliberately select its existing savepath.
Filename collisions and session ownership require case-specific reconciliation;
this change does not implement an importer. Keep external game-data references
unless you intentionally relocate the assets.

## Verification and boundaries

See [INVENTORY.md](INVENTORY.md) for audited producers and
[RESULTS.md](RESULTS.md) for actual build/runtime evidence and limitations.
`devtools/ce/test_portable_storage.py` uses normal launches in disposable Unicode
installation copies, adds a target with the normal CLI, exercises the existing
Nox gameplay/save/session fixture, then restores after relocation with the old
address absent and a different caller working directory. It combines native
recursive directory notifications with explicit Windows stream-write tracing.
Set `SCUMMVM_CE_WRITE_TRACE` to an explicit diagnostic output file only when
tracing is wanted. Trace output itself is an intentional external override.

Representative runs cannot prove universal behavior across every optional engine
and third-party library. OS-managed graphics caches, Windows shell preferences
and crash-report infrastructure are outside CE storage ownership. They must not
be represented as CE application-data guarantees. No engine or existing feature
is disabled to obtain a passing storage test.
