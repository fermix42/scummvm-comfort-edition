# F12 slowdown investigation capture

During normal Snatcher play, press **F12 once** when the slowdown starts.
Wait for the persistent completion dialog:

> Capture complete - safe to restart.

That success message appears only after the diagnostic report, recovery save
and completion record have been finalized, and their stored bytes checked.
The dialog stays open until dismissed. If the recovery save fails, the dialog
explicitly says **SAVE FAILED**; a diagnostic report can still be available.
Do not interpret a progress message or an incomplete/error message as success.

Evidence goes into the installation's managed `saves` folder, with a shared
`<target>-snatcher-investigation-<date>-<time>-<counter>` prefix:

- `.json`: engine state, recent semantic trace, selected graphics/audio settings,
  event counts and recent performance history, written before attempting a save.
- `.save`: full native recovery snapshot, separate from numbered gameplay saves.
- `.result.json`: whether snapshot finalization and read-back validation succeeded.

Each press creates a new capture. Existing manual saves, autosaves, prior
captures and Play Session ownership are preserved. The snapshot can be copied
to an unused manual slot for investigation; it is not automatically inserted
into or loaded through the player's save list.

Normal play retains at most 1,800 performance samples in memory, generally one
per second plus scene-resource changes, and 256 semantic trace entries. Samples
include polling/update/draw/present timing maxima, input/mouse counts, narrative
location and pending events. Windows samples also include this process's working
set, private bytes, handle count, cumulative CPU time and available physical RAM.
There is no network upload, other-process enumeration or full configuration dump.

F12 runs on the game's main thread. It can capture a responsive slowdown, but
cannot execute while that thread is completely stuck inside a driver or other
blocking call. This is an investigation tool, not a fix for the reported slowdown.
An abrupt process-termination test checks that completed evidence survives loss
of the process; it does not simulate sudden power loss or failing storage.

## Detailed graphics capture (format 2)

Each history sample now includes `graphics`: backend and renderer identity,
video driver, drawable dimensions, and `backend_reported_vsync` (`-2` means
unknown; OpenGL `-1` means adaptive). SDL2 surface rendering reports the renderer
flags; OpenGL reports the current swap interval. Driver-level overrides are not
observable here. SDL2 OpenGL captures also record GL vendor and version.

The `lock`, `prepare`, `scale`, `upload`, `render`, `present`, `metadata` and
`other` objects contain `max_ms`, `total_ms` and `calls`, with `frames` giving the
number of host screen-update calls in the interval. Maxima are per-frame stage
totals; calls count timed segments (preparation and locks may have more than one
segment per frame). Zero calls means the stage was not reached, rather than a
measured zero-duration operation. The stages partition CPU wall time inside the
screen update with millisecond resolution; they are not GPU execution timings.
`prepare` includes dirty-region conversion and cursor/OSD preparation; `scale`
covers the surface scaler/aspect loop. OpenGL shader scaling belongs to `render`.
`other` includes early-return paths and backend teardown after presentation.
Metadata queries are separate and sampled about once per interval. Unsupported
backends retain an empty identity and attribute their time to `other`.

Recording is enabled only around Snatcher's main-loop `updateScreen()` call.
It adds no GPU fences, extra presents, renderer resets or settings changes.
The existing 1,800-sample bound and F12 completion/recovery behavior are retained.
Press F12 once when the slowdown returns and wait for the completion dialog.

## Per-texture OpenGL capture (format 3)

`graphics.textures` separates `game`, `cursor`, `cursor_mask`, `overlay`,
`osd_message`, `osd_icon` and `unattributed` work. Each role records operation
`max_ms`, `total_ms` and `calls` for the whole `update`, CPU `convert_scale`,
`edge_prepare`, `gl_bind_texture`, `gl_pixel_store`, `gl_tex_sub_image`,
`gl_tex_image`, `gl_error_clear` and `gl_error_check`. These are per-operation
call maxima across the interval. Whole-update and nested operation times overlap;
do not add them together. Clean texture-update calls may have zero upload calls.
Surface SDL leaves OpenGL metrics at zero. `unattributed` includes instrumented
GL calls outside a named texture-update scope, such as texture binds during
rendering; it is not exclusively upload work.

The GL error checks execute in the same order and quantity as before. Their
latency is measured separately from the actual GL call. Allocation calls use
`gl_tex_image`; ordinary texture transfers use `gl_tex_sub_image`. Other GL work
inside a texture update remains included in the whole-update time.

For ordinary transfers, each role records upload counts, total uploaded bytes,
and `last_upload`/`slowest_upload` metadata. The latter preserves the slowest
complete updateArea call (bind, error checks, pixel-store and transfer), not just
the latest fast transfer. Metadata includes texture ID, GL internal/input formats
and type as numeric enums, allocated/source dimensions, pitch, bytes per pixel,
dirty rectangle, actual uploaded width/height, byte count and monotonic timestamp.
The existing transfer uploads whole rows, so its byte count may exceed the dirty
rectangle's width. Counts/metadata describe attempted API transfers, not proof
that the GPU accepted or completed them. Recorded IDs are GL object names, not
memory addresses. At most one last and one slowest transfer per role per sample
are retained, with the existing 1,800-sample bound. No pixel contents are recorded.

The automated callback tests use a null system and mock GL callbacks to check
texture attribution, delayed-call/error-check separation, ordering and aggregation.
They make no real graphics-device calls. Native CLI tests inject the mapped F12
event under SDL dummy devices; the owner presses F12 during normal gameplay.

## Release polling policy

Release builds (`RELEASE_BUILD`) omit routine GL_CALL/GL_ASSIGN error polling.
Debug builds retain it, and explicit GL_CALL_CHECK allocation/failure checks
remain active in both builds. The timed texture wrappers follow the same policy.
Explicit failure checks may still produce error-query timing entries when
allocation is attempted; the flag describes routine polling only.
Format-3 samples report `graphics.routine_gl_debug_polling` so the next capture
identifies the compiled policy. This candidate addresses the measured pre-call
error-clearing stall; real-device acceptance remains with the owner.

## Windows presentation timing

The owner's 2026-11-21 16:20 capture confirms routine GL polling was off.
After about 71 minutes, swaps changed abruptly from inexpensive calls to
sustained 68–70 ms waits inside SDL_GL_SwapWindow with swap interval 1.
Uploads remained at 0–1 ms, handles stayed at 713–715, and available RAM
stayed above 19 GiB. This identifies a presentation wait, not its underlying
driver/compositor cause.

Snatcher now disables VSync for its Windows engine lifetime by default, retaining
the existing elapsed-time simulation and 10 ms host-loop yield. This removes the
requested retrace wait; it does not guarantee that a driver will never block.
An explicit per-game `vsync` setting takes precedence. The change uses the backend
graphics transaction API, writes no configuration, and restores an inherited
enabled VSync state when the engine exits, including early returns. Other
platforms retain their existing behavior. Unsynchronized presentation can tear.

F12 still records the backend-reported swap interval and stage timings so an
owner playtest can distinguish a recurrence with interval 0. Dummy-device tests
can validate policy and recovery but cannot establish real-device acceptance.
