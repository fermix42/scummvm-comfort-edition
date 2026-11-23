# Snatcher native save optimization

Native saves retain save-anywhere continuation. New saves and replacements use
SNSV version 2; the loader retains SNSV version 1 and music-history versions 1–8.
A replacement keeps its previous generation as `.bak`. Older executables cannot
load the new format, so retain a compatible executable when reverting a build.

## Format and retained state

The version 2 envelope has a bounded, sorted section directory. The directory
and each payload have separate CRC32 checksums. A full load validates every
payload; the chooser reads only the directory and description, and reads the
thumbnail only when requested. The description is limited to 255 bytes.

Music saves capture the driver, command queue, synthesizer, pending output and
percussion continuation directly (FMAH 9 for Nuked, 10 for Gens). Immutable music
sequences are references into the verified original music asset. Loading a new
checkpoint does not replay the elapsed music history. Legacy music checkpoints
still use their historical importer and acquire a direct checkpoint on resave.
New gameplay does not accumulate that replay history.

Input section 13 omits an inactive startup presenter and inactive presentation
load buffers. The credits resource is represented by a checked source reference.
Active startup, deferred work, graphics memory, random state, timers, shared PCM
staging and active feed/resampler continuation remain serialized. In particular,
shared sound memory may contain earlier writes and must not be recreated from
only the most recent bank ID. Active PCM source/feed buffers are retained in
this revision; further source-reference work must prove exact continuation.

The already loaded immutable PCM catalog and bank are reused within the same
open disc instead of rereading the 84,748,288-byte bank on each load. A first-use
load still stages and validates these assets before committing engine state.

Writes use a checked temporary file, finalization and the existing backup/rename
transaction. Routine whole-file read-back has been removed. Checksums are built
before the write and verified on load. This does not promise durability against
power loss beyond the save manager/filesystem guarantees.

## Validation

`devtools/ce/test_snatcher_save_performance.py` runs the normal executable through
the isolated native test CLI with SDL dummy drivers. It compares image and
logical state at save/load boundaries, optionally checks exact future audio and
random continuation, tests legacy-slot replacement and backup fallback, and
restarts the process to test cold loading. Original captures remain read-only.
The state-codec component suite covers indexed round trips, corruption,
truncation, duplicate sections and metadata limits. Timing receipts identify the
actual tested executable; elapsed times include CLI boundary rendering.

The existing `fm_publication_cold_export` / `fm_publication_cold_import` CLI
probes accept `file_packet: 1` for checkpoints larger than the JSON request
limit. They use only fixed target-derived save filenames for the two supported
prefix positions (17 and 1473 frames), with an 8 MiB read bound. This avoids
increasing the controller's request limit or accepting arbitrary paths.

## Measured results — November 22, 2026

Normal Release x64, native CLI, SDL dummy; wall times include boundary rendering.
The previous test build's rewritten saves provide the baseline below. Different
captures contain different amounts of active PCM state.

| Capture | Old size | New size | Old load | New load | Old save | New save |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Hospital ambush | 1.89 MB | 1.08 MB | 15.31 s | 0.056 s | 0.070 s | 0.035 s |
| Credits / Jaime | 2.65 MB | 1.49 MB | 16.50 s | 0.088 s | 0.064 s | 0.031 s |

Hospital, credits, flashlight and JORDAN captures passed picture/state checks,
exact future mixed audio/random continuation, legacy-slot replacement, backup
fallback and process restart. Their new saves were 43–46% smaller than the
original captured files. Startup save/load, autosave isolation and manual-slot
preservation passed for Gens and Nuked. Both music backends passed fresh-process
checkpoint imports at 17 and 1473 frames with identical future PCM samples,
including queued commands and percussion. Ten component suites passed.

Existing legacy saves still require their old music-history reconstruction on
first load (approximately 0.8–7 seconds for these captures after PCM reuse).
Saving them again upgrades the slot to the direct checkpoint format.

The portable-storage audit passed four checks and reported a pre-existing
raw-write inventory mismatch. Every modified production file's inventory was
compared with HEAD and is unchanged; the new checkpoint codec adds no raw-write
sites. The unrelated inventory baseline was not refreshed.
