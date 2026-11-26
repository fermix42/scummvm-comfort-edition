# The Cat in the Hat: Windows Living Books v3

CE issue [#59](https://github.com/fermix42/scummvm-comfort-edition/issues/59)
originally stopped at `setZOrder` when entering the first interactive scene.
Supporting this edition also requires its motion and drawing extensions,
dragging, parent/child movement, animation queries and event registration.
Skipping the failing commands leaves its activities incomplete.

## Current checkpoint: owner playtest accepted

On November 25, 2026, the owner confirmed completing the Windows edition from
start to finish in both Read To Me and Let Me Play after the fixes below. This
is human end-to-end playtest acceptance, in addition to the recorded automated
traversals and focused regressions. Historical references below to uncommitted
source or pending owner acceptance describe earlier checkpoints.

The inherited CE issue #59 can be closed for its implemented startup/support
fixes and this accepted Windows playthrough. Full original-interpreter
visual/audio comparison, exhaustive random/alternate-branch coverage, the Mac
edition and the unavailable Tortoise v3 regression are not claimed by that
acceptance. Following the owner's completed playtests and explicit request,
the Windows detection entry no longer carries `ADGF_UNSTABLE`. The unqualified
Macintosh entry retains that flag. Earlier flag-retention statements below
describe historical checkpoints.

The final CH13 background item (`SND1`, resource 1047) uses the engine-wide
ambient handling, but its authored interaction scripts explicitly stop it at
the start of Mouse, Tommy, Sally, Bird, Book, Mom and Fish gags and restart it
afterward. Those explicit commands remain honored. The CH12 fix removes
unintended foreground interruptions and enables eligible continuous loops; it
does not override deliberate page scripts. No CH13 behavior override was added.

The installed CE Test executable remains the validated cleaning-audio build
`8a388c8fd055321c5e2615c5dae8a72b40f5b615a9f93ded8c5c8c8c471d19be`.
Recording acceptance and committing the source does not refresh that executable
or alter the owner's settings/saves. Private assets, dumps, binaries, profiles
and validation backups remain outside the source commit.

## Scope and inventory

The investigated Windows edition has a 2,428-byte `OUTLINE`, MD5
`0b5ab6dd7c08cf23066efa709fa48bbc`. The implementation starts from accepted CE
commit `9020e444f62`; it does not include the separate unfinished ThinkQuick v5
prototype. The Mac edition has not been qualified by this work.

The bounded inventory covers all 50 MHK files: 2,862 BITL items, 20,165
event/action records, 7,526 SCRP animation resources and 2,166 animation
containers. SCRP resources are animation instructions, not text VM bytecode.
Targets, conditions, nested RunData payloads, notification parameters, sprite
nodes and shape references were decoded separately.

## Implemented behavior

- Z-order changes update drawing and picking together. Parenting preserves an
  item's position when attached, propagates later movement and detaches safely
  during deletion. Item name lookup follows the interpreter's case handling.
- Dragging retains the pointer offset, checks each movement step against the
  configured rectangle or item mask, and dispatches start/move/end events.
  Rectangle drag limits include their right/bottom edges; ordinary hit tests
  and motion bounds are half-open.
- Key records register on their specified script event. Later registrations
  replace matching keys. Rollover tracks one frontmost eligible item, supports
  both rectangle and bitmap tests, and sends leave events when disabled.
  A zero-length rollover payload disables it; a two-byte zero payload enables
  rectangle tracking. Focused mouse tracking also distinguishes enter, leave,
  inside movement and outside movement.
- Runtime PlayInfo changes start idle timers. This v3 interpreter uses
  milliseconds, while earlier supported interpreters retain their tick clock.
  Animation rate division and position retain 16.16 fractions. Queries and
  playback use the animation timeline, and moved animations update their
  drawing bounds. Paused animations resume when re-enabled.
- RunData applies to its selected target. Sequential/random action selection
  visits the selected entry correctly, and custom events reach every target.
  Binary RunData also accepts trailing conditions, including the tossing
  scene's 24 conditional rollover updates. Implicit function calls require an
  argument list, so variable names such as `xPos` remain valid assignments.
  Geometry helpers preserve x/y ordering; substring uses the original
  one-based inclusive range and clips an overlong ending index.
- Item 4000 implements the interpreter's motion state and script-driven
  collision callbacks. Item 5000 captures the selected drawing tool on mouse
  down, paints during mouse tracking, and resets tracking on mouse up. Its
  canvas persists across redraws, supports the native solid pen and eraser,
  and is discarded with the page.
- Picture and animation drawing honor opaque versus transparent copy modes.
  An empty cursor name restores the default cursor.

The native test CLI adds read-only Living Books page, item, focus, rollover and
selected game-variable inspection. It introduces no page-jump, arbitrary
script-evaluation or game-state mutation command. Gameplay tests use normal
pointer and keyboard events.

## Original-interpreter references

Private validation scripts execute original CAT32.EXE routines under a 32-bit
x86 emulator. Their receipts state every intercepted external operation.
They cover 203 motion inputs, 202 constrained drags, 5,000 animation timing
steps, runtime idle scheduling, and Z-order/parent-link cases. Other semantics,
including canvas mouse dispatch, pen settings, picture modes and key
registration, were checked in the original dispatcher.

These are routine-level references, not a full original-game playthrough or a
claim of pixel-identical GDI rasterization. Original executables and game
assets are not distributed with the source.

## Qualification: November 25, 2026

Private receipts are under the existing workspace validation directory
`validation/cat59-20261125`. Each run records its executable hash. Earlier
candidate results must not be represented as fresh tests of a later binary.

The repeatable normal-input driver is
`devtools/ce/test_livingbooks_cat.py`. Supply `--exe`, `--assets`, `--extra`
(engine data), an owned `--root`, and `--mode play`, `--mode read`, or
`--mode dialogue` for the focused missing-speech regression.
Play-mode image assertions require Pillow. Use different roots for concurrent
runs. It records the executable hash, visited pages, assertions, screenshots
and result; failed assertions return a nonzero exit status. The private
qualification receipts below use the original workspace drivers from which
this parameterized driver was extracted.

- [x] Complete structural inventory and original-routine references.
- [x] Final executable: natural Read To Me through all 13 pages/subpages and
  return to controls; Let Me Play traversal through all 13 pages/subpages.
- [x] Final executable: ball dragging, release physics and successful basket
  drop; persistent pencil drawing and erasing; pickup/toss of milk, fish and
  book. The initial candidate also passed clock tracking/release and
  picture-frame/brush rollover/move/click.
- [x] Initial candidate: Options selection, backward navigation, panel return,
  Play/Read mode change and cold restart with hidden OpenGL rendering.
- [x] Initial candidate: DISCO letter-key sequence, all five credits pages and
  automatic return to Options.
- [x] Final executable: cold launch, controls, Play scene and forward navigation
  in Green Eggs and Ham (v3), Bears in the Dark (v3), Tortoise and the Hare
  (v1 CD) and Arthur's Teacher Trouble (v1).
- [x] Existing CE Test updated in place after qualification. Only its executable
  changed; all 325 other files, including settings, saves and runtime
  dependencies, retained identical hashes. The prior executable is backed up
  under the existing validation directory. Release was not updated.

Initial qualified executable SHA256:
`ba673776b0c9996496307fe26c4307fd524e9c615c03c426d8054f7e112aafc3`.
The retained `installation.json` links each receipt to this hash. Production
sources were checked against the build's saved patch before installation.
Cat logs contain no unimplemented/ignored required command or unknown item.
Headless TTS initialization warns; Bears retains its pre-existing panel-load
and key-notify warnings, also present in the accepted baseline.

These tests cover normal traversal and the named activity paths, not every
hotspot/random branch or subjective audio quality. Full original-game visual
and audio comparison remains a separate acceptance task; routine-level oracles
must not be described as that comparison. Tortoise v3 data was unavailable, and
the Mac Cat edition was not tested. The unstable detection flag is retained,
and issue #59 stays open for those broader acceptance boundaries and source
review. The implementation is local and uncommitted at this checkpoint.

## Follow-up: missing opening dialogue

The owner reported silence on Read To Me page 3.2, "Now! Now! Have no fear",
until the following section. A trace of the initial qualified binary confirmed
that CH03T sound 1000 never started, although 1001 through 1005 did. Sound 1000
contains 189,502 PCM sample frames at 22,050 Hz (8.594 seconds). Its sample
values confirm that the resource contains sound rather than silence.

Animation restart used `seek(1)`, whose preview execution suppresses sound and
notifications. This consumed the first frame before playback. Restart now
rewinds with `seek(0)` so the real update executes that frame's effects. Explicit
seek previews retain their existing behavior.

Read-only test inspection now reports each animation's active sound resource
and elapsed source sample frames. The focused regression verifies all six
CH03T clips and advancing samples, including resource 1000. The asset-derived
full-read audio check also covers long first-frame narration on 3.2, 5.1, 7.1,
11.3 and 13.1. These checks distinguish audible-stream progression from merely
visiting a page; they do not claim a subjective listening comparison.

The corrected executable has SHA256
`f8ad724f38b7de93fd173f80fc870a37c33d325a1acd379f4e79fb81c1dcb74e`.
It passed the focused dialogue regression, all five opening-clip sample checks,
both complete 13-page/subpage traversals, the Play activity assertions and the
same four Living Books regression cases. `audio-installation.json` records the
in-place CE Test replacement and its prior-executable backup. All 325 other
installation files retained identical hashes, including current settings and
saves. This replaces the initial qualified executable above; the initial
candidate's separate navigation/credits receipts remain historical evidence.

## Follow-up: overlapping idle poses and transient blanks

The owner reported Sally doubling for several seconds on EP/CH05 in Let Me
Play, during automatic idle narration without clicking either child. The owner
subsequently confirmed no doubling with diagnostic17 installed, then reported
brief blinking on the same page. These observations are distinct from the
interaction reproduction below.

The original CAT32 item dispatcher uses a signed local visibility balance
(visible at zero or above) and a separate global visibility boolean. CE had
treated local visibility as a boolean: restoring a temporarily hidden list
could reveal a reaction layer that was already hidden before the interaction.
Executing original dispatcher message 0x41 with virtual redraw callbacks stubbed
verified 280 visibility operations against the signed-balance model. A normal
Tommy interaction reproduced the old persistent Sally overlap and verified
restoration of the negative reaction-layer balance with the correction.

Automatic idle sequencing also exposed a separate global-enable error: CE's
global setter called the local setter, losing the local enabled state. Global
pause/resume now preserves that state and notifies animation playback only when
effective enablement changes. Sixty original CAT32 message 0x13 operations,
with virtual callback calls recorded and stubbed, verified local-state
preservation and the effective-state callback transitions. The idle17 baseline
failed the hands-off test with only one child's idle sequence repeating;
idle18 repeated both sequences in Surface and OpenGL. These routine checks do
not constitute a full original-interpreter visual comparison.

Animation restart also reset all sprite cells and deferred real first-frame
execution until a later engine loop, allowing a redraw with an empty pose.
Restart now executes the first real frame immediately, retaining sound and
notifications. This follows the original reset routine at 0x416704, which
resets its nodes and immediately calls its step routine at 0x415a56. Explicit
seek previews remain silent. Per-item `uninitialized_draws` records visible,
running animations drawn before their first frame has executed; F12 retains
this evidence even when a screenshot misses a transient reset.

F12 now saves a local PNG plus a runtime JSON report, including page module,
layer order, enable/visibility states and history, individual animation nodes,
timers, sounds/sample positions, recent input, renderer and executable identity.
See [the diagnostic guide](../ce-diagnostics/README.md) for usage and boundaries.
Reports do not copy the owner's INI or replace gameplay saves.

### Visibility/diagnostic validation and installation

The installed diagnostic build has SHA256
`43a5d292bf84215bc55978e4924bea60eac0bbd9e70e18691a9d44cf7717aa0b`
(blink20). The clean Release build passed with zero warnings/errors. Fresh
100-second idle checks in SDL Surface and hidden OpenGL verified both children
repeat, no persistent doubled poses, zero uninitialized idle/reaction draws,
and both children's drawable cells at all 966 recorded idle redraw boundaries
(462 Surface, 504 OpenGL). The ordinary Tommy interaction, held/second mapped
F12 presses, PNG/JSON/executable identity and render-history records passed in
both renderers. A blocked diagnostic directory preserves the existing file and
resumes gameplay. Four other Living Books gameplay regressions and the five
static storage contracts passed; the focused six-clip dialogue check passed
fresh on blink20 as well.

Complete Read To Me and Let Me Play traversals, Play activity assertions,
all six dialogue clips and the five long opening-clip checks passed on blink19
(`78353b25905a67d9b1a868c256f4dae9623a3e3762c1114cc753a2063d7f9070`).
The subsequent blink20 changes add only read-only drawable-node queries,
bounded render-presence history and JSON serialization; playback, visibility
and enablement logic are unchanged. These full traversals are prior-build
evidence, not fresh full-book runs of blink20. Earlier diagnostic17 receipts
cover Unicode executable relocation, portable runtime boundaries and Snatcher
F12 recovery compatibility; the common diagnostic/storage implementation is
unchanged by the Living Books follow-up.

`blink20-installation.json` records the in-place CE Test replacement. Only the
executable changed; all 327 other files, including current settings, saves and
the owner's diagnostic capture, retained identical hashes. The prior
diagnostic17 executable is preserved as `accepted-before-blink20.exe` under
the existing validation directory. Release is unchanged. The source remains
local/uncommitted, and the broader original-reference/acceptance boundaries
above remain open.

## Follow-up: Horton dynamic assignment

The owner's F12 report on EP/CH07 showed Horton (1270) selected and the debugger
error `missing EOS (got 22)`. The same ordinary click reproduced the error on
the installed blink20 executable. The shuffle assigns generated variable names
with `@eval(curQD)=curP; @eval(curP)=curQD`; drag/drop similarly updates four
generated destinations. Token 0x22 is assignment. Eval previously returned the
value and left the assignment token unconsumed.

Eval now recognizes assignment destinations, parses a single variable reference
including optional list indices, and uses the shared ordinary variable-operation
handler. The name and indices are computed once before the RHS, and the
destination is resolved after RHS evaluation so nested assignments cannot
invalidate a retained variable-map pointer. Invalid expressions and `self` are
rejected as destinations. Existing eval expression reads are preserved. The
ordinary assignment/read/compound/postincrement body was extracted unchanged;
`horton21-assignment-refactor.json` verifies that mechanical equivalence.

The clean Release build passed with zero warnings/errors. Executable SHA256
`50bf328fa4ab5b7416bd4f29b8750171c12a74b758f15d8de8a993fe3a05cf38`
passed three complete Horton shuffle-and-solve rounds in SDL Surface and three
in hidden OpenGL. The focused normal-input runner verifies bidirectional
six-piece mappings, 22 real drag/drop swaps, all-six-matched completion,
Horton's win animation, repeatability and an F12 PNG/JSON capture. Recorded
inspection states independently verify that each shuffled/swapped piece's
actual position matches its assigned slot, not merely that variables changed.
Read-only inspection/F12 now includes the puzzle's mapping and progress values.
See `devtools/ce/test_cat_horton.py` and `horton21-puzzle-geometry.json`.

The first candidate test's wait condition incorrectly expected Horton visible
while the shuffled puzzle was active; it timed out after a successful shuffle.
The corrected runner waits for shuffle completion, then performs the full
puzzle interactions. Those initial failures are retained as test-harness
evidence, not reported as successful complete runs.

At the owner's scope correction, unrelated full-book, dialogue and idle reruns
were stopped and marked cancelled. Four-title gameplay checks had already
completed; they were not a required installation gate. No fresh full-book
acceptance claim is made for this candidate. Dynamic indexed/compound forms
share the existing variable-operation code but are not separately exercised by
this game's focused puzzle regression.

`horton21-installation.json` records the CE Test update. Only the executable
changed; all 329 other files retained identical hashes, including settings,
saves and both owner diagnostic captures. The previous blink20 executable is
preserved as `accepted-before-horton21.exe` under validation. Release is
unchanged. Source remains local and uncommitted; the broader acceptance issue
remains open.

## Follow-up: clock controls and hourly sequences

The owner's CH08 dump prompted clock-only validation. On the installed Horton
build, entering the dial positioned the scripted hand pointer only after a
second mouse movement. An immediate click selected the clock background.
Priming that pointer with extra movement allowed capture, but winding still
left the hands unchanged and never triggered an hour: `ZoneNum` comes from
substring as a string, while numeric comparison used uninitialized integer
storage or rejected mixed string/integer equality.

Scripted drag startup now positions the item immediately. Integer-form numeric
strings convert explicitly for clock arithmetic, predicates and comparisons;
integer/real conversions also read the correct value member. Same-type string
equality remains string equality. Read-only clock variables are included in
native inspection and F12 reports.

Testing additionally reproduced a stuck `HandDown` after holding through an
hourly sequence. Re-arming the hidden captured item as a hover pointer put it
under the mouse before release, changing the old press's outside-release event
into an inside-release event without the authored cleanup. CE now records this
specific re-arm and preserves outside-release cleanup for that press while
keeping the new pointer positioned normally. Other hit-based releases retain
their existing routing.

The final clean Release build passed with zero warnings/errors, SHA256
`0b6ead7327405e6175a5952e77193fe3b84ed188f580829b154ba0ec287be63a`.
`devtools/ce/test_cat_clock.py` uses only ordinary clock inputs. On this exact
executable, Surface and OpenGL each passed twelve successive hourly sequences,
both hour-wrap directions, all 22 authored sound resources with advancing
sample positions, and drawable graphics for the five hours that contain visual
sequences (2, 3, 5, 8, 12). Holding through completion and releasing stationary
or after movement passed. The control follow-ups passed immediate capture,
stationary re-grab, release inside/outside, and leaving/re-entering while held.
F12 PNG/JSON captures retain clock state.

The initial complete-hour runs reached their later control check and failed
because the harness jumped directly from sector 12 to 7. Authored scripts
accept adjacent sectors after that boundary and intentionally ignore that
jump. The corrected control-only follow-ups use 12 to 1 and passed on the same
unchanged executable; the already-passing hourly sequences were not rerun.
`clock24-qualification.json` combines those receipts without relabeling failed
files. An earlier candidate test also incorrectly required nonzero animation
position for audio-only hour 11; the corrected test uses audio progression.
No unrelated game, full-book or other activity regressions were run for this
clock task.

For normal use, hover over the dial, hold the left button and wind continuously
through neighboring numbers; the hour hand follows the minute hand. Resume near
the current minute-hand position after a chime or re-entry. Abrupt jumps across
the face are not a supported authored gesture.

`clock24-installation.json` records the CE Test update. Only its executable
changed; all 331 other files retained identical hashes, including current
settings, saves and owner diagnostics. The prior executable is preserved as
`accepted-before-clock24.exe` under validation. Release remains unchanged.
Source is local/uncommitted and broader title acceptance remains open.

## Follow-up: cleaning background audio

The CH12 owner dump identified the callback-free ambient sound item `AmbSND`
(2090), resource 1073, configured as an indefinite repeat. Its raw source is
300,866 mono 8-bit frames at 22,050 Hz (about 13.64 seconds), without an embedded
loop. A focused baseline trace recorded repeated actual resource starts and
rejected/retried starts around cleaning actions. The old ambient path wrote
the foreground sound ID, allowing subsequent interaction locks to stop it;
item-level repetition then replayed it from the beginning.

Ambient sound items now play independently of foreground ownership/priority.
Callback-free, delay-free ambient forever loops use the mixer's continuous
whole-stream loop. Other scripted completion/loop scheduling remains intact.
Interaction sound cues retain the existing foreground behavior. Standalone
sound items now expose active resource, elapsed source samples and successful
start count in native inspection and F12.

The clean Release build passed with zero warnings/errors, SHA256
`8a388c8fd055321c5e2615c5dae8a72b40f5b615a9f93ded8c5c8c8c471d19be`.
`devtools/ce/test_cat_cleaning_audio.py` tested only this scene in Surface and
OpenGL. Both runs cleaned BookA, Chair and Phone, verified advancing pickup
sound samples, and retained exactly one ambient start with monotonically
advancing background samples across more than two complete source-loop lengths
(761,254 and 761,055 frames of progress). F12 records the running background.
The exit check advances the page, confirms ambient-owner removal and exercises
normal page sound cleanup; it does not independently record output-device
silence. No original-interpreter listening comparison or unrelated gameplay
regressions are claimed. The baseline Rake click did not select its pickup and
is not counted as a validated cleaning interaction.

`clean25-installation.json` records the CE Test replacement. Only its executable
changed; all 337 other files retained identical hashes, including settings,
saves and owner captures. The previous executable is preserved under validation
as `accepted-before-clean25.exe`. Release remains unchanged. Source is still
local/uncommitted and broader title acceptance remains open.
