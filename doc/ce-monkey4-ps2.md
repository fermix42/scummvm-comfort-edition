# Escape from Monkey Island PS2 startup in Comfort Edition

## Build enablement

Escape from Monkey Island uses the Grim engine's optional `monkey4`
subengine. Detection can recognize the game even when that subengine is
disabled, in which case startup reports:

> Escape from Monkey Island support is not compiled in

The CE Windows release and layout-test workflows, and the local
`run-build-release-cleanenv.ps1` script, now pass
`--enable-engine=monkey4` to `create_project`. The local script also
regenerates an existing project when its x64 properties lack
`ENABLE_MONKEY4`, so an older generated solution picks up the change.

The PS2 version additionally requires MPEG2 support. The verified Windows
build already enabled `USE_MPEG2` and `USE_BINK`; no additional codec
dependency was needed.

## Renderer workaround

The English PS2 version was detected with `artAll.m4b` hash
`0dc9a4df0d8553f277d8dc8e23b6249d` and size 34,593,974 bytes.
After enabling the subengine, starting it with OpenGL with shaders caused
an access violation (`0xc0000005`) that terminated ScummVM.

Two local crash dumps showed NVIDIA OpenGL driver addresses on the
crashing thread. In the first dump, the main game thread was in
`Grim::GfxOpenGLS::createFont`. An isolated shader-renderer launch reproduced
the crash. Two isolated classic OpenGL launches remained responsive past
startup and loaded game resources.

For affected installations, select **OpenGL**, rather than **OpenGL with
shaders**, in the game's graphics settings. The equivalent target setting
is:

```ini
[monkey4-ps2]
renderer=opengl
```

This was saved for the tested local target. It is a per-game configuration
workaround, not an engine or NVIDIA driver fix, and is not automatically
applied by the build changes.

## Validation and limits

Validation on October 12, 2026:

- Windows Release x64 build succeeded with zero errors.
- The executable's `--list-games` output included `grim:monkey4`.
- Classic OpenGL passed the two startup process/log checks described above.
- Visible graphics, save/load reliability, and a complete PS2 playthrough
  were not established by those checks.

The game remains marked unstable in detection, and upstream lists PS2 as
unsupported. Enabling compilation does not establish complete game support.
Controller mapping changes are a separate concern from these startup fixes.

## PlayStation controller profile

For the PS2 game target, a controller whose backend-reported name identifies
it as a PS2, PS4, PlayStation 2/4, or DualShock 2/4 selects the console
keymap when the game starts. Other controllers and other game platforms
retain the existing MI4 defaults. No USB-adapter IDs are guessed.

The console keymap forwards each face button, D-pad direction, shoulder,
trigger, stick click, Start, and Select to the PS2 scripts' corresponding
native controller key. Options corresponds to Start; Share corresponds to
Select. Both sticks feed the native joystick axes rather than emulating
mouse movement or sending PC keyboard shortcuts.

PS4 touchpad click opens ScummVM's menu. Ctrl+F5 remains available on all
controllers, and F1 opens the game menu. The profile does not rely on the
PS/Guide button, which other applications can intercept.

All actions can be changed through the game's keymap settings. Console
bindings use the separate `monkey4_ps2_console` keymap ID, so existing
`monkey4` PC-style overrides do not inject overlapping keyboard actions
into the console profile. Connect the controller before launching MI4;
if changing controller families during play, relaunch to select the
corresponding profile.

Physical PS2 adapters must report a recognized PlayStation controller
name and standard SDL button/axis mappings. Devices reported as generic
joysticks use the existing defaults and remain manually configurable.

Validation: the Windows Release x64 build completed with zero errors.
An isolated launch with the connected PS4 Controller logged selection of
the console profile and remained responsive. A launch with controller
input disabled retained the fallback profile and remained responsive.
The user confirmed the controller profile worked in gameplay on October
13, 2026. A physical PS2 controller was not tested locally.
