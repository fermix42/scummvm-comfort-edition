# Native gameplay CLI verification — November 6, 2026

The normal Windows Release x64 executable contains the opt-in native controller.
Both MI4 editions passed genuine headless gameplay/save/session/cold-restore
scenarios and separate hidden classic OpenGL scenarios. This is a locally
committed testing feature, not a release approval or an installation.

## Identity and preservation

- Branch: `codex/native-test-cli`.
- Production source/test implementation commit:
  `7d954c8e36d98f00d58e49d913c3ed4efe6092d9`.
- Parent integration candidate: `2d0de89e8b875852a0aca2c69a2de9b3c8b445b9`.
- Upstream baseline: `v2026.4.0`, `7d2709da5136866589f7602f5f5a7b17de03420e`.
- Worktree: `D:\Codex Projects\scummvm-private\scummvm-native-test-cli`.
- Executable: `build-scummvm\Releasex64\scummvm.exe` in that worktree.
- Executable SHA256:
  `10cf1b4cd11d4f6f9912439ac5b991adf1a1155e386601eab1ccf59241f26bdc`.

The subsequent reporting commit adds these receipts without changing production
source or the executable; its final SHA is supplied with the delivery. The build
used production files identical to the implementation commit above. The original
ten dirty source files still match their preserved snapshot, both accepted CE
executables retain their original hashes, and the integration candidate remains
clean at its original HEAD with executable hash `7ed5bee7...52e8d2f`.
Nothing was pushed, published, installed, merged into the integration candidate,
or copied over an accepted executable. Original game assets/profiles/saves were
not used as test write destinations.

## Implemented architecture and interface

[README.md](README.md) gives the architecture decision, complete protocol,
PowerShell/Python examples, isolation requirements and adapter instructions.
[MI4.md](MI4.md) explains the edition-specific production paths.

The Windows entry point installs inherited stdin/stdout pipes only for
`--test-cli` with an explicit disposable root. It calls the normal ScummVM
configuration/detection/engine entry point. DefaultEventManager drains commands
on the engine thread; Grim adds a real frame boundary. Input uses the existing
event queue, installed keymap Action outputs or real physical-event mapping.
OSystem/SDL, TinyGL/classic OpenGL, MetaEngine, Grim's original serializer and
CE's existing persistence services retain ownership of their respective work.
No worker thread reads game objects; no arbitrary Lua evaluator, memory writer,
shell command or network endpoint is added to the protocol.

Supported commands: `launch`, `capabilities`, `inspect`, `status`, `input`,
`advance`, `wait`, `assert`, `session_create`, `session_select`, `saves`, `load`
and `quit`. The `save` command explicitly returns `native_save_menu_required`:
MI4's native menus supply metadata that its bare engine save request omits.
The supported save workflow navigates those menus through normal input.
One game runs per process; restart uses a fresh process and the same owned root.

The MI4 adapter exposes real scene, actor position/orientation, cutscene level,
enabled/held controls, handler kind, known menu identity, bounded native menu/
dialog choices, native slot count and pending save/load requests. CE inspection
exposes session records/status/owners, queue ownership, native unlocked IDs,
popup keys/progress/sidecar, challenge and assistance state. Missing adapters
return unsupported. Cine has shared-layer runtime verification without a new
semantic adapter. Nox, SCI, MacVenture, SCUMM and other adapters were not added.

## Runtime matrix

| Edition / renderer | Real gameplay input | Native Save menu and metadata | Two CE banks | Fresh-process A/B production loads | Clean exits |
| --- | --- | --- | --- | --- | --- |
| Windows / TinyGL + SDL dummy | PASS | PASS | PASS | PASS | PASS |
| PS2 / TinyGL + SDL dummy | PASS | PASS | PASS | PASS | PASS |
| Windows / hidden classic OpenGL | PASS | PASS | PASS | PASS | PASS |
| PS2 / hidden classic OpenGL | PASS | PASS | PASS | PASS | PASS |

All four scenarios reached actual Wedding Ship gameplay (`wed.set`), observed
Guybrush's orientation change through normal controls, opened the native menu,
created independent sessions A/B, made native saves, decoded descriptions and
160×120 gallery thumbnails, preserved A's payload hash while saving B, and
restored the exact inspected actor record, scene and A's ownership in a new
process. B then restored into its own session. Save-event queue owners and active
session status survived restart. Negative load-boundary/ownership cases and the
rejected bare save path also behaved as specified. No synthetic save payload,
replacement script, game-state setup write or achievement grant was used.

Windows used typed native descriptions. PS2 used its retail profile selector,
analog menu axes, native Save/Cancel choice and automatic `Slot 1 - Wedding Ship`
metadata. Both wait for the original game handler to return after saving;
normal mode alone precedes the end of PS2's save notification.

### Desktop and rendering evidence

- Genuine headless mode reported the actual SDL video driver `dummy`, rendered
  with TinyGL and owned **zero native desktop windows** in 404 samples.
- Classic OpenGL reported driver `windows`; its SDL/probe/helper windows stayed
  hidden. All 392 samples reported zero visible process-owned windows. This
  mode requires the Windows graphics environment and is labeled offscreen,
  rather than claimed to be independent of a desktop/GPU session.
- Foreground-window and cursor-position samples remained identical across both
  final runs. The client performs read-only observations, never OS input.
- Native SIMG thumbnails from both headless saves were decoded and visually
  inspected: they show actual Wedding Ship gameplay, not blank/null rendering.
  The PNGs remain with local artifacts; they are not desktop captures.
- No visible-game-window test, monitor rearrangement, resolution/DPI change or
  desktop keyboard/mouse automation was performed.

The previously unsuccessful desktop F1/Ctrl+F5 attempts did not record engine
delivery/state, so their exact failure cannot be reconstructed. Native F1 via
the installed Windows keymap and native PS2 Start now demonstrably reach the
production handlers. The investigation established real cutscene, press/release,
table-handler and edition-specific menu boundaries; it did not establish a
defect in ordinary human input or claim to fix an external injection tool.

## Build and regression results

The established checkout-relative clean-environment script built the normal
Release x64 solution successfully: **0 errors, 0 warnings in the final incremental
build**, elapsed `00:02:46.86`. All default upstream engines and CE engines remain
enabled. This does not relabel inherited full-build compiler warnings as fixed;
the previous integration warning classification remains applicable.

| Verification group | Passed | Failed |
| --- | ---: | ---: |
| Existing CxxTest methods | 442 | 0 |
| Existing native CTest targets | 2 | 0 |
| SQ5 importer tests | 5 | 0 |
| Integration/source/default-engine audit | 8 | 0 |
| Ordinary version/engine/detection CLI checks | 3 | 0 |
| New activation/isolation, protocol and generic Cine CLI checks | 23 | 0 |
| MI4 gameplay scenarios, counted separately | 4 | 0 |

The first five rows retain the previous 460 counted checks; the new common CLI
groups contain 4 activation/isolation, 8 protocol and 11 Cine checks. Gameplay
assertions/frames are not inflated into additional unit-test totals. The two CE
cold-achievement methods also passed in separate processes and are already
included in 442. Repeated post-build/explicit CxxTest invocations are counted
once. The inherited default build compiles out the slow blending test body unless
`SLOW_TESTS` is selected, as documented in the integration acceptance audit.

Unknown/malformed commands, unsupported adapters/actions, timeout/recovery,
unsafe save-path overrides, occupied roots, raw-axis boundaries, fresh processes
and clean shutdown are covered. The Cine timeout fixture intentionally exits 2;
the test expects it. All final MI4 processes exit 0. The source audit explicitly
lists the reviewed opt-in test hooks and retains exact upstream/CE blob checks
outside them, plus checks real compile inputs for both new C++ modules.

## Evidence and earlier corrections

[verification.json](evidence/verification.json) pins the executable/source,
per-mode results, transcript hashes/process counts, desktop observations, log
hashes and preservation audit. [Headless results](evidence/headless-results.json)
and [offscreen results](evidence/offscreen-results.json) are compact receipts.

Full command/result/state JSONL transcripts and all disposable native saves live
under `D:\Codex Projects\scummvm-private\native-cli-validation-local\verified-headless`
and `verified-offscreen`. Normal CLI receipts are in `verified-normal-cli`.
Detailed build/unit logs are in the isolated worktree; their exact names and
hashes are in the verification receipt. Earlier failed diagnostic runs remain
under the same local artifact root rather than being presented as final passes.

Corrections established by those runs: explicit SDL surface selection was needed
because Windows defaults to an OpenGL 2D backend; MI4 launch must wait for its
main loop; normal mode/actor presence during credits was insufficient evidence;
native callbacks can be tables; bare saves omit retail metadata; CE's interactive
exit confirmation needed an explicit test-mode confirmation; PS2 startup/profile,
axis navigation, horizontal Save confirmation and post-save handler readiness
needed their own production-state synchronization. The new-session command clears
prior transient popup context using CE's existing reset API. Axis Action
templates without a hardware magnitude are rejected rather than delivered with
undefined data. No game behavior or save metadata was fabricated to pass.

## Limits and future adapters

Native Save menus and production-API restores are verified. Native Load-menu
navigation, every individual native slot, full gallery visual/layout acceptance,
historical missing save seeds, other hardware/DPI combinations and full
playthroughs are not claimed. Existing helper tests cover the 160/four-slot
namespace boundaries; the real pilot saves native slot 0 in each bank.

Live achievement synchronization remains blocked in test mode and was not run.
MI4 gains no achievement hooks from this change. Existing offline achievement
tests verify unlock/progress/ownership persistence; actual earned hooks in other
engines remain separate gameplay acceptance. The normal service remains intact.

There is no same-process engine restart, arbitrary inventory/state edit or
unrestricted script endpoint. The Windows transport is currently SDL2-only;
SDL1/SDL3 explicitly reject activation and other platforms have no activation
entry point yet. An engine that stops yielding entirely requires the client's
outer deadline/cleanup, in addition to native bounded waits. Offscreen OpenGL
support on this machine is not a promise about other drivers.

Add future adapters by overriding the optional engine state/command methods,
declaring narrow capabilities, using production events/saves and adding safe
frame boundaries only where meaningful. Follow the instructions in README and
prove an actual legitimate-data/cold-process scenario before claiming support.
No unrelated adapter or Nox M4 migration work was reopened in this phase.
