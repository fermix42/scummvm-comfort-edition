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
