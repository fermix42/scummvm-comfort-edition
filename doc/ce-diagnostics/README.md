# F12 diagnostic capture

Press **F12 during a game** to save the current screen and runtime report. CE
briefly pauses the game while capturing, then resumes it. Holding the key makes
one capture; release and press again for another.

Windows CE saves two matching files beside its executable, under
`logs/diagnostics/`:

- `ce-<timestamp>-<millis>-<serial>.png`: the renderer's current screen, captured
  before displaying the new confirmation message.
- The matching `.json`: `ce-diagnostic/1` runtime report. Attach both files when
  reporting a problem. Captures remain local.

The report includes the running executable's path, size and MD5 fingerprint,
build identity, game/target/platform and asset path, graphics mode and screen
dimensions, a fixed selection of graphics/audio settings, and the last 64
keyboard/mouse events. It does not copy the INI or account credentials.

Living Books additionally records the exact page module, mode/subpage/phase,
draw order, item loading/enabling/visibility balance, parent relationships,
playback/timer and sound/sample positions. Animation records contain individual
node script IDs, entry/delay/cel/bitmap, coordinates, bounds and copy modes. The
last 64 visibility changes explain temporary hide/show restoration.
Per-item `uninitialized_draws` counts draws of a visible, running animation
before its first frame executes, preserving evidence of transient restart
blanks after they have passed.
The last 64 redraw records retain timestamps, page/phase and animation IDs
with drawable cells at each draw boundary. These are layer-presence records,
not a video or a history of full screenshots.
Other engines still receive the screenshot and common runtime report; engines without
a semantic diagnostic adapter mark that section unavailable.

Snatcher retains its existing F12 investigation/recovery-save behavior and also
receives the common PNG/JSON capture. Generic F12 diagnostics do not create or
replace gameplay saves.

The on-screen success message is shown only when both screenshot and report
were written. A failure produces an incomplete/failed message. Managed writes
use the existing portable filesystem boundary, including an owned isolated root
when testing with the native CLI. There is no external-profile fallback or new
configurable output destination.

## Validation

`devtools/ce/test_cat_visibility_diagnostics.py` takes `--exe`, `--assets`,
`--extra` and `--root`; `--offscreen` selects hidden OpenGL rendering. It uses
ordinary gameplay input to reproduce the character interaction, verifies the
reaction layer's hidden visibility balance, presses F12, validates PNG/JSON and
executable identity, checks detailed layer records, suppresses held repeats and
tests a second physical-keymapped press. It never controls desktop input.
Add `--idle-seconds 100` to first observe the scene without clicking either
child: both idle sequences must repeat and overlapping poses must not persist
for a second. This also detects lost local enable state after a global pause.
It verifies both children have drawable cells at each recorded idle redraw,
including redraws that happened between inspection requests.

Run the strict portable-storage contract and runtime boundary checks after
changing diagnostic output paths. Existing installs, settings and saves remain
outside the test write roots.
