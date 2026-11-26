# ThinkQuick / Living Books v5 unfinished prototype

Tracking issue: [CE #1](https://github.com/fermix42/scummvm-comfort-edition/issues/1).

Resumed November 26: see [the baseline and initial coverage report](THINKQUICK-COVERAGE.md)
for the integrated source identity, fresh validation and explicit unknowns.
The checkpoint evidence below remains historical.

## Status on November 25, 2026

This is an unfinished implementation checkpoint, not a game-support claim.
ThinkQuick (`mohawk:carmentq`, Windows) completes Startup.MHK setup and loads
Main.MHK, then exits with:

```text
Unsupported V5 bytecode token 67 at 22
```

Inspection of the supplied original executable identifies token 0x67 as unary
numeric negation. It is not implemented here. Further missing behavior remains;
fixing the next opcode alone will not establish playability. No title, menu or
gameplay validation has succeeded. CE Test and release were not updated.

## Scope of this checkpoint

The prototype explores archive-local page/proxy resource bases, v5 script
entry parsing, a postfix evaluator, references and lists, named events and
local/argument frames, script linking and deferred initialization, supplied
FON fonts, draw order/text assignment, immediate page transitions, ACME path
setup and script-assigned proxy filenames. These need systematic review against
original behavior. Font loading and text assignment do not establish complete
text rendering/editing support. Unsupported operations remain explicit errors.

## Existing validation

The last clean-environment Release x64 build passed with zero warnings/errors.
Executable SHA-256:

```text
0bba41311535f811cab9e4d2c9009a3ef1013538ce21e970ca9bab69e5df4b9c
```

All 18 generated native regression cases passed. They exercise the prototype,
including failure paths, but are not differential tests against the original
interpreter and do not prove v5 compatibility. The runner is
`devtools/ce/test_mohawk_v5_startup.py`; use its `--help` for required paths.
It requires an owned ThinkQuick data directory for detector metadata and a new
validation root for generated assets and isolated profiles.

Workspace evidence is retained under `validation/thinkquick-20261125` outside
this checkout: `CURRENT-STATUS.md`, `vm-regressions-links/results.json`,
`v5-links/engine.log`, `v5-links-result.json`, structural inventories and
`owner-after-v5-work-verification.json`. Those local artifacts and game assets
are not included in this commit. Preservation checks matched all 114 original
hashes: CE Test executable, profile and 112 save files, with no added saves.

## Resume plan

Follow issue #1: inventory ThinkQuick's required features; document the original
VM, values, calling conventions and lifecycle; establish original-interpreter
comparison probes; then implement and validate coherent subsystems. Audit this
prototype against that evidence before treating its behavior as established.
Include representative older Living Books regression coverage.

Validate startup, menus, a real question round and persistence before updating
the existing CE Test installation, preserving its settings and saves. Keep
release promotion separate. Record exact source/build identities, original
reference evidence, passed/failed tests and remaining gaps in progress updates.
