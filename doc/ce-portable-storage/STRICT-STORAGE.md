# Permanent Windows CE storage boundary

The previous executable-relative implementation already covered default config,
saves, CE sessions/achievements, screenshots, downloads, temporary files and
engine preference/cache overlays. Its remaining policy gap was intentional
support for external managed-storage overrides. Earlier results in RESULTS.md
describe that older policy; they are historical evidence, not fresh results of
this revision.

Windows CE now confines managed storage to its executable directory tree.
CLI requests for external configuration or managed output are errors. Legacy
INI storage keys resolve to their local defaults and normalize on the next
normal config flush. Internal custom locations remain supported. Game `path`,
resource `extrapath`, soundfonts and other source assets remain external/read-only
references. Nothing imports or changes old external settings or saves. If a
legacy INI selected external saves, those saves must be copied manually with CE
closed; do not merge colliding session registries.

The existing layout is preserved:

| Path beneath the executable | Contents |
| --- | --- |
| `scummvm.ini` | Main settings and game targets |
| `saves/` | Saves, autosaves, CE sessions, achievement files/queues and sidecars |
| `screenshots/` | Screenshots |
| `logs/` | Application and per-target engine logs |
| `data/` | Downloaded content/icons and generated engine preferences |
| `cache/` | Generated engine caches, created as needed |
| `temp/` | Process-local temporary working files |
| `ce-write-trace.tsv` | Optional write instrumentation, enabled only by environment switch |

Managed stream creation (including atomic temporary files), directory creation,
save removal and replacement validate the boundary. Win32 canonicalization
collapses dot segments and checks a directory separator boundary. Managed paths
through descendant junctions/symlinks, alternate data streams and ambiguous
Win32 components are rejected. This is an application invariant, not an OS
security sandbox: it does not defend against a concurrent hostile process
replacing directories or introducing hard links between validation and opening.

Unwritable installations fail with the path and instructions to move the whole
folder to writable storage. There is no AppData/Documents fallback. Separate
installations own separate state. Upgrade by closing CE and replacing program
files while preserving INI, saves and writable subdirectories; never overlay a
personalized installation with someone else's config/save data.

The opt-in native test controller retains its pre-existing explicitly owned
disposable test root. It is a diagnostic isolation facility, not a selectable
normal storage mode. No normal launch can select another installation root.
External exports through the common write API are currently rejected too;
there is no blanket exemption allowing engine-generated files outside the tree.

## Audit and regression checks

The existing [inventory](INVENTORY.md) and source index cover common saves and
independent engine writers. AGS virtualizes its application-data tokens and
routes create/delete/rename/copy through SaveFileManager; SCUMM and Grim/EMI
saves and CE banks/sidecars use that manager too. HPL1 generated assets,
Harvester preferences, QDEngine scores and Director logs retain their existing
portable adaptations. ImGui persistence is disabled by the existing backend
(`IniFilename=nullptr`). Plugin/resource asset search remains read-only;
downloads use guarded filesystem streams. No engine or gameplay feature was
removed for these checks.

`devtools/ce/test_strict_storage.py` checks boundary hooks and compares raw
file/profile/environment API sites throughout tracked engines and relevant
subsystems against `strict-write-sites.json`. The inventory intentionally
includes disabled/vendor/read-only sites; it detects changes requiring review,
and is not evidence that each such line is an active writer. Review inventory
diffs on upstream updates. CI runs these checks and the observer classifier.

Use `--exe <exe> --artifacts <fresh-directory>` for actual portable-copy launch
checks without commercial game assets. `test_portable_storage.py` adds real Nox
saves/cold restores, metadata, multiple installations, upgrade preservation,
legacy normalization, junction rejection and read-only ACL checks. Native CLI
MI4 tests exercise actual Windows/PS2 native saves and cold restores separately.

Windows/driver activity is outside this invariant. Prior observed categories
include NVIDIA caches, Direct3D databases/journals and shared SAPI lexicon
activity. Directory notifications are not process-attributed kernel tracing;
unknown controlled-profile files and any CE-traced profile write fail the tests.
No representative matrix proves every optional third-party engine/library.
Fresh results and remaining coverage limits are recorded separately.
