# Static command menu during flashlight examination

The November 22, 2026 owner capture exposed horizontal command-menu movement
during mouse flashlight examination. The primary command-window split retained
its font-map Y position, but final row composition overwrote its fixed horizontal
origin with the flashlight plane's scrolling origin. Text wrapped across the
screen as the pointer moved.

Primary command and dialogue windows now retain the font plane's fixed horizontal
origin below their authored split. Ordinary windows use both font planes;
alternate captions retain the independent background plane's scrolling. The
upper scene continues using the original scrolling/line sampling. Pointer
projection, hotspot lookup, keyboard controls, phone-window ownership and
saved-state formats are unchanged.

The scene-runtime component regression publishes several source-plane positions
and checks that the upper plane moves while the lower command window stays at
X zero and its font-map Y origin. `devtools/ce/test_snatcher_flashlight.py` restores
a privately supplied owner snapshot under an isolated native CLI root, sends
ordinary mouse moves and compares the rendered lower-window RGB bytes. It also
checks upper-scene movement, unchanged story state and preservation of the input
snapshot. It then clicks Look at an off-center flashlight position, verifies
that caption text does not wrap into the source's sixteen-pixel left margin,
acknowledges the dialogue and verifies the returned menu stays fixed. The first
correction covered only the open command menu and missed this dialogue path;
the expanded regression reproduces that omission in the previous executable.
Pass `--expect-static` for candidate validation. Without that switch, the receipt
records which faults a baseline reproduces.

During root flashlight examination, wheel up/down cycles the selected command,
including across paged menus, without changing the source aim or hit region.
A left click in the upper examination view confirms that selected command at
the already positioned beam; button coordinates do not retarget it. Confirmation
can wait for the same menu owner's repaint and cannot repeat while held. Direct
menu clicks and nested-menu behavior retain their existing handling.

`--wheel-commands` extends the native regression with both wheel directions,
wraparound, unchanged source aim, immediate click during selection repaint,
one confirmation despite a repeated down event, and the resulting Look dialogue
and menu return. It uses ordinary mouse events and the normal command consumer.
