# MI4 native regression workflows

Use `test_native_cli.py` with legitimate installed game data, the normal Release
executable and a new disposable artifact directory. [README.md](README.md) defines
the protocol, isolation and renderer options. [RESULTS.md](RESULTS.md) records
the actual evidence and its executable identity.

The automated pilot follows these production paths:

| Operation | Windows | PlayStation 2 |
| --- | --- | --- |
| Startup | Skip ordinary intro/credits using Escape, then observe actual scene and cutscene state | Observe the game's Press Start text, press native Start, confirm an empty slot in the retail four-slot New Game selector, then follow intro state |
| Gameplay evidence | Read Guybrush's position/orientation before and after native directional input | Read the same engine actor state before and after native PlayStation directional input |
| Open native menu | Installed `monkey4` keymap `GMNU` action emits F1 | Native PlayStation Start key |
| Select Save | Read `main_menu.save_index` and `menu.cur_item`; navigate using keyboard arrows | Read the same native indices; pulse joystick Y through the real axis handler |
| Commit save | Choose an empty slot, wait for native name entry, type the description and confirm | Native Save/Cancel memory-card dialog; read its choices and current selection, pulse joystick X to Save, then confirm with Cross |
| Completion | Wait for normal engine mode and the normal game button handler | Both waits are necessary: the Game saved notification briefly disables PS2 input after normal mode resumes |
| Metadata | MetaEngine description, 160×120 thumbnail, actual filename and CE owner | Native `Slot 1 - Wedding Ship` description, thumbnail, filename and CE owner |
| Session isolation | Save the same native slot in A and B; retain distinct bank files and an unchanged hash for A | Same operation in the active retail profile's native slot, with CE's four-slot namespace |
| Cold restore | Quit, launch a new process, select A and load native slot 0 through the production deferred load API | Same independent-process procedure, using the PS2 save implementation |

The actor is initially tied aboard the Wedding Ship, so the pilot reports an
observed orientation change rather than claiming that Guybrush walked elsewhere.
Credits can contain a selected actor in normal engine mode; that alone does not
establish gameplay. The pilot waits for actual scene/cutscene/handler conditions
and then requires an observed actor change.

The runner compares the restored scene, full inspected actor record and session,
checks that A/B sessions remain active across quit/restart, restores B separately,
and checks queued save-event ownership through the real CE service. It validates
out-of-range loads, missing ownership and rejection of the unsafe bare save API.
Starting B clears the previous save's transient popup/progress context through
CE's existing `resetPopupState()` operation.

No game script is replaced. No external command changes a room, actor, inventory,
script variable, save payload or achievement credit. Native menus build their own
save metadata and call their own serializers. The protocol's direct `load`
command is tested separately from native menu navigation: it is not proof that
the native Load menu was navigated.

These are distinct limits:

- Native menus, two real saves/banks, metadata and cold production-API restores
  are gameplay evidence. Pure naming-helper tests are counted separately.
- The pilot saves native slot 0 in each session. Existing helper tests cover all
  160 Windows boundaries and all four PS2 boundaries; the pilot does not claim
  that every native UI slot was individually populated.
- Gallery thumbnail decoding and native menu state are verified. This is not a
  visual acceptance of every ScummVM gallery layout, monitor or DPI setting.
- Newly generated valid saves are now available in disposable artifacts.
  Compatibility with missing historical save seeds remains a separate test.
- MI4 does not gain CE achievement hooks from this controller. Actual CE session
  save events are inspected; no synthetic unlock is sent. Earned achievements
  in other engines and live synchronization are separate acceptance work.

For a bounded terminal investigation, `probe_test_cli.py` forwards JSON requests
to the same native executable and keeps a transcript. Its `press`, `axis` and
`text` helpers expand into ordinary protocol input/frame commands. The optional
`--prepare-save-dialog` follows the PS2 production menus to their confirmation.
It never sends Windows desktop keyboard or mouse input.
