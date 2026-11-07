# Storage inventory

Scope: all Windows CE default-path hooks; shared configuration/filesystem/save
layers; engine-wide searches for raw C/C++/Win32 file operations, profile lookup,
temporary APIs and write streams; focused persistence tracing for CE, Grim/MI4,
Nox, SCI/SQ5, Cine, MacVenture, AGS, Director, Wintermute, Glk, HPL1 and Crab.
Read-only assets and OS-managed resources are distinguished below. An audited
source path is not a runtime claim for every engine.

| Producer / mechanism | Original location | New default / treatment |
| --- | --- | --- |
| Windows SDL main config | Roaming ScummVM, with Windows-directory legacy fallback; executable directory only when portable detection passed | `<exe>/scummvm.ini`; no profile discovery or legacy fallback |
| Windows SaveFileManager | Roaming ScummVM `Saved games`, or old portable `Saved games` | `<exe>/saves`; per-game/global/CLI overrides retained |
| Ordinary saves, autosaves, native achievement `.dat` files, cloud `timestamps` | Effective SaveFileManager directory | Same manager, portable default; no serialization changes |
| CE sessions, status, owner maps | `ce-playthroughs.json` in effective save directory | Same file and ownership architecture under portable saves |
| CE event synchronization queue | `ce-achievements-queue.json` in effective save directory | Same; no network enablement or account changes |
| Popup progress/achievement sidecars | `<save>.ce-achievements.json` and existing CE persistence | Same save-relative names and ownership |
| MI4 Windows/PS2 native banks and galleries | Effective save directory; `efmi*.gsv` / `efmi*.ps2`, including CE hashed banks | Same files and embedded thumbnails; no separate session directory |
| Nox paired generations / metadata | Effective save directory, `.g0`/`.g1` and existing metadata | Same paths, formats, owner rules and previews |
| SCI/SQ5, Cine, MacVenture saves/session data | SaveFileManager / CE services | Inherit portable saves; serializers untouched |
| Windows screenshots | My Pictures/ScummVM Screenshots, or old portable Screenshots | `<exe>/screenshots`; explicit screenshotpath retained |
| SDL application log | Roaming ScummVM/Logs/scummvm.log or old portable root | `<exe>/logs/scummvm.log`; explicit logfile retained |
| Downloaded launcher icon packs | Roaming ScummVM/Icons or portable Icons | `<exe>/data/icons`; lazy parent creation on write |
| Downloaded DLCs | Roaming ScummVM/DLCs or portable DLCs | `<exe>/data/dlcs`; configured destinations retained |
| Application caches | Many are memory-only; HPL1 generated XML/AI data could be written beside game assets | `cache/hpl1/<target>/<source-hash>-<name>` for generated HPL1 XML; lazy creation, explicit cachepath retained |
| Native temporary APIs | Inherited Windows TEMP/TMP, potentially profile Local/Temp | Process-local `<exe>/temp`, or explicit temppath after config load |
| Legacy relative diagnostic dumps, debugger outputs, recordings | Caller working directory | Executable-root anchored CWD; explicit output paths remain explicit |
| Native test CLI profile/saves/log and auxiliary data | Explicit owned test root | Same enforcement; auxiliary defaults/CWD/TEMP also use that owned root |
| Harvester startup preferences | `CONFIG.INI` in game-data directory | Portable `data/engine-preferences/<target>/<source-hash>-CONFIG.INI`; overlay read first, original assets preserved |
| QDEngine hall of fame | Relative `Resource/hof.dat`, with game-asset reads | SaveFileManager `<target>-hof.dat`, original plaintext format and bundled read-only fallback |
| Director script trace log | Game-data-relative `traceLogFile` | Portable `logs/<target>` or selected logfile directory; explicit absolute diagnostic destinations retained |
| Backend preferences/keymaps/window settings | Shared main config | Portable config; no independent preferences file introduced |
| AGS file API | Adapted ScummVM SaveFileManager; original application-data tokens are virtualized | Inherits selected save directory; original game assets remain read-only |
| Director XLib FileIO, Ednox, widget, MMOVIE, MSFile writes | Adapted SaveFileManager | Inherits selected save directory; diagnostic explicit paths unchanged |
| Wintermute script file writes | Adapted save-manager files; original `c:\val.log` code is disabled | Inherits selected save directory |
| Glk interpreter saves/transcripts | Glk save/file abstractions; explicit user filename/environment selections in some interpreters | Save manager where adapted; explicit selections respected; relative diagnostics anchored |
| Glk TADS temporary swap files | ScummVM memory streams | Memory-only, no Windows temp file |
| HPL1 named `.cfg` preferences | Original XML filename, potentially asset-relative | `data/hpl1/<target>/<source-hash>-<name>`; generated overlay read first, bundled source read-only fallback |
| HPL1 Personal/SHGetFolderPath and file-copy code | Original code disabled with `#if 0`; special path returns empty | No enabled Windows profile producer there; enabled save/stream abstractions audited |
| Crab original `std::ofstream settings.xml` | Disabled original implementation | ScummVM adaptations retained; no active profile write from that block |
| Raw `fopen` search matches in Lure, Gob, Cruise, Chamber, Watchmaker, Made, Bagel and vendored libraries | Many commented, disabled or developer diagnostics; enabled relative diagnostics vary by engine | Classified in source audit; anchored relative output, no blanket claim that every diagnostic is save-manager based |
| HTTP/cloud downloads, local web uploads | Explicit request-selected destination streams | Respect explicit destinations; default DLC/icon locations portable |
| ROMs, soundfonts, themes, engine `.dat`/`.m4b`, game directories | Read-only search paths / explicit asset references | Preserved; not persistent application state and not migrated |
| Optional WinSparkle updater | Not enabled in validated baseline or this Release configuration | Not enabled/disabled by this storage work; optional updater persistence not runtime verified |
| GPU caches, OS shell bookkeeping, crash-report infrastructure | OS/third-party managed | Excluded from application-owned storage, not redirected or claimed absent |

There is no automatic session-tree migration, new save format, new portable-mode
toggle, engine exclusion or legacy configuration import. The unused Windows
AppData storage helper was removed to prevent accidental reuse as a fallback.

The broad engine scan found 160 `DumpFile`/`createWriteStream` references (including
comments, declarations and test support), not 160 independent active producers.
Direct asset-derived writes found during review were addressed above. Most other
matches are developer dump/export commands or relative diagnostic output, not
normal save formats. Ultima's default `nuvie.cfg` is relative and therefore stays
under the anchored installation root; explicit XML/export filenames remain
explicit. Windows profile directory access in the disabled original HPL1 code
was not counted as an active producer. HPL1, Harvester, QDEngine and Director
runtime behavior requires representative assets for full engine-specific acceptance.

[Engine source reference index](engine-write-sites.json) records exact files and
lines for the broad raw-write/profile-path scan, including disabled code.
