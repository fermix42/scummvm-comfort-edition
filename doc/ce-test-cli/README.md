# Native gameplay test CLI

The Windows Release executable has an opt-in JSON-lines controller. Normal
launches do not activate its transport, renderer settings, event injection or
inspection. There is no socket, shell command, arbitrary file-write command,
Lua evaluator, memory editor or achievement-award command.

Read [RESULTS.md](RESULTS.md) for tested capabilities and limitations. A capability
being implemented does not imply that its runtime pilot has passed.
The edition-specific production paths are explained in [MI4.md](MI4.md).

## Start and isolate

Create an **empty, disposable, absolute directory**. Run the normal executable
with redirected stdin/stdout (pipes are required):

```text
scummvm.exe --test-cli --test-root=D:\Tests\new-case --extrapath=<engine-data directory>
```

The controller emits `{"ok":true,"event":"ready","protocol":"ce-test-cli/1"}`.
Only its owned directory can be reused for a cold restart. Each directory holds
one asset identity, `profile.ini`, `saves/`, engine log and an ownership marker.
The game data directory is read by the normal engine. Keep it separate from the
test root. Do not point a test at an accepted profile or save directory.
The generated disposable target disables the optional first-run MD5 dialog
(`check_gamedata=false`) and unstable-game warning using existing preferences;
these gameplay pilots are not game-data integrity tests. Explicit test quit
confirms CE's interactive exit prompt without completing/abandoning the session.

The engine-data directory must include the upstream `monkey4-patch.m4b` for MI4.
It must contain trusted engine resources, not replacement game scripts.
DLLs must accompany the executable, as in the normal portable package.

Achievement HTTP transport is blocked while this mode is active, independently
of configuration. No test protocol operation grants achievements. Local game
events still use the normal CE pipeline. There is no live synchronization test.

## Protocol

Send one UTF-8 JSON object per line. Requests can carry an `id`; replies echo it.
Errors have `ok:false` and an error code. Unknown/unsupported commands are
recoverable. Failed assertions and bounded waits make the final exit status 2.
Startup/usage errors return 64; engine errors return a nonzero status. Production
diagnostics go to stderr/the isolated log, never the JSON response pipe.
Maximum request line is 64 KiB; maximum nesting is 32. Requests are serialized.

| Command | Fields and behavior |
| --- | --- |
| `capabilities` | Protocol and command inventory, including the lifecycle boundary |
| `launch` | `engine`, `game`, `platform`, `path`; starts normal detection/engine execution once |
| `status`, `inspect` | Common state, CE state, current keymap actions and optional adapter state |
| `input` | Explicit down/up event; see below; success acknowledges enqueueing |
| `advance` | `frames`: 1–3000; waits for actual MI4 main-loop frame boundaries, maximum 120 s |
| `wait` | `field` dot path, `equals` JSON value, `timeout_ms` 1–120000 (default 10000) |
| `assert` | `field`, `equals`; exact JSON equality, with PASS/FAIL represented by `ok` |
| `session_create` | `title`; calls the existing CE service and flushes isolated configuration |
| `session_select` | `session`; selects an existing active, unarchived local session through CE |
| `saves` | MetaEngine enumeration: chooser IDs, filenames, descriptions and CE owners |
| `load` | MI4 `slot`: zero-based native slot in the active session bank; deferred production restore |
| `save` | Returns `native_save_menu_required`; use the game's actual Save menu via input |
| `quit` | Sends normal quit event and shuts down; closing stdin also quits |

One engine runs per process. Restart means quit and start a new process against
the same owned root. This deliberately exercises cold persistence instead of
trying to re-enter global backend/plugin initialization. There is no same-process
restart command. Waits do not freeze the engine; frames, rendering, audio timers,
scripts and normal event processing continue. A client must impose an outer
deadline as protection against an engine that stops yielding entirely.

### Input

`kind:"key"` accepts numeric ScummVM `key`, optional `ascii` and `flags` from
`common/keyboard.h`; `phase` is `down` or `up`. Native Grim controller pseudo-keys
are exposed through the logical adapter, not common keyboard numeric codes.

`kind:"action"`, `map`, `action`, `phase` uses the **installed enabled game
keymap's Action event**. `inspect.actions` enumerates available IDs. This is the
same production output event the keymapper emits, not a hardcoded game mutation.
Global backend actions are intentionally not exposed through this command.
Axis-valued Action templates require a hardware magnitude and return unsupported
here; use `kind:"axis"` with `map_physical:true` for those bindings. Mouse actions
use the event manager's current virtual pointer position.

Physical `kind:"button"` (`button` 0–31), `kind:"axis"` (`axis` 0–5,
`position` -32768–32767), and keyboard events may set `map_physical:true` to run
through the real keymapper before delivery. Otherwise the raw engine event is
queued. Mouse commands use `kind:"mouse_move"` or `"mouse_button"`, game-space
`x`, `y`, left/right `button` 1/2 and explicit phase. They never warp the OS cursor.

MI4 `kind:"logical"` supports `open_menu`, `confirm`, `cancel`, `skip`, `up`,
`down`, `left`, `right`. Windows uses the native PC keys; PS2 uses the engine's
native PlayStation key codes. `skip` uses Escape's normal movie/scene behavior.
Press and release in separate commands with an intervening `advance`. Do not
assume a down/up pair in a single polling batch is visible to game scripts.

### Save and state evidence

MI4 load calls existing `loadGame`, which defers the normal restore operation to
the engine loop. An immediate reply says `requested`, **not restored**.
Advance frames, check pending flags, enumerate
metadata/ownership, and compare restored scene/actor state. Native slots are
0–159 Windows and 0–3 PS2. Load verifies the active bank's ownership.
MI4's scripts supply required save metadata through the native Save menu. A bare
engine save request bypasses that setup and was experimentally found to produce
an incomplete disposable save. This path is now rejected before writing.
Native Save menu input is the supported save path; no metadata callback or save
payload is synthesized by the controller.

`saves` includes all chooser entries and their owners; chooser IDs are distinct
from native slot numbers. No test command deletes saves or modifies sidecars.
The existing CE routing, collision preservation and sidecar hooks remain owners
of all bank/persistence behavior. Direct API saves do not prove native menu use.

MI4 adapter fields include actual current set name, engine mode, selected actor
position, pending save/load flags, renderer and native slot count. Native menu
inspection compares the current Lua `system.buttonHandler` function against
known game menu handlers using raw reads, including table-valued handlers used
by LuaBase's production callback dispatch. `unsupported` means no known handler
matched, not that no menu exists. Cutscene level, handler kind and enabled/held
controls are separate source values. Normal mode and a selected actor alone
do not establish gameplay: the credits also have both. Bounded raw diagnostics
for fixed menu tables expose scalar fields, choices and actual text-object IDs.
The active native dialog also exposes bounded raw choices/current selection.
No semantic fields are inferred from screenshots or elapsed time.

Common CE inspection uses the existing service for session records/status,
pending queue entries and owners, native unlocked IDs, popup-seen keys, progress,
current sidecar, challenge mode, enabled cheats and permanent assistance flags.
Seen-popup keys and achievement credit are separate fields. Unsupported adapters
return an explicit `unsupported` result.

## Rendering and ownership decisions

The existing Event Recorder is useful for recorded input/playback, but does not
provide a semantic request/reply protocol or CE session/save inspection. The null
unit-test backend does not render MI4's 3D world. MI4 already has TinyGL; the
default mode selects it with SDL's dummy video driver and software presentation.
That path has no native desktop window. A second explicit
`--test-render=offscreen` mode selects classic OpenGL and creates the SDL window
hidden from its first creation. It is an offscreen graphics environment, **not**
an assertion that OpenGL works without a Windows desktop/driver/session.
The PS2 shader workaround remains classic OpenGL in this second mode.

The SDL window layer prevents test-mode cursor warping, grabbing, relative
mouse mode, display layout changes, maximizing and visible creation. SDL physical
input events are drained without delivery, so user keyboard/controller activity
cannot influence a test. OS message boxes and the engine fatal debugger are
replaced by structured test errors. Normal execution retains all these paths.
External browser/log-viewer and native file-dialog features are disabled in test
mode, and Windows process error boxes are suppressed without changing system
settings.

There is no worker thread touching game objects. The Windows entry point installs
a nonblocking inherited-pipe transport only after explicit activation. Commands
are drained by DefaultEventManager on the engine thread; the MI4 adapter adds an
actual main-loop frame boundary. Native menus still run their original Lua
handlers. Save serialization remains in the original deferred frame path.
Backend work uses OSystem; MetaEngine owns save enumeration; CE owns persistence.

## Python and PowerShell

From Python, use the bounded client in `devtools/ce/test_cli_client.py`:
run from that directory or add it to `PYTHONPATH`.

```python
from test_cli_client import Client
c = Client(exe, disposable_root, trusted_engine_data)
c.launch(windows_game_directory, 'windows')
c.command('advance', frames=2)
c.press('skip')
state = c.command('inspect')
c.close()
```

From PowerShell, run the Python regression driver with absolute paths:

```powershell
& $python devtools/ce/test_native_cli.py --exe $exe --extra $engineData `
    --artifacts $newDisposableDirectory --windows $windowsGame --ps2 $ps2Game
```

`System.Diagnostics.ProcessStartInfo` with `UseShellExecute=false`,
`CreateNoWindow=true`, `RedirectStandardInput=true` and
`RedirectStandardOutput=true` can also exchange JSON lines directly. Redirect
stderr to a separate log. Read each response before issuing a dependent command.
Every runner transcript records process identity, executable SHA256, requests,
responses and exit status. No sleeps serve as gameplay condition assertions.

Build with this worktree's checked-in `run-build-release-cleanenv.ps1`.
Run it with `-UnitTests` for existing CxxTest/native checks. The native CLI is
compiled into the same normal Release executable by engines/module.mk and the
existing Windows build/project generator; there is no separate production binary.

## Adding an adapter

Override `Engine::testCLIState()` and, only where required,
`Engine::testCLICommand(JSONValue&)`. Defaults explicitly report unsupported.
Read state only on the engine thread and publish source-defined semantics.
Use installed keymap actions and normal save APIs. Add a safe frame boundary
through `CETestCLI::frame()` if actual frame advancement is exposed. Do not expose
script execution, memory writes, arbitrary filenames or synthetic achievement
awards. Add a legitimate-data runtime scenario and cold-process persistence
evidence before documenting an engine capability as verified.
