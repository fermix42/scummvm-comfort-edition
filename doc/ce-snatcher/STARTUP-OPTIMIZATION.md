# Snatcher startup optimization — November 22, 2026

The owner uses the 21-file BIN/CUE source. Three fresh-process runs in each
mode measured these times with the normal Release executable, isolated native
CLI and SDL dummy video/audio. OS filesystem caches were not flushed. Fixture
preparation time is excluded; process bootstrap, engine initialization and the
first requested frame are measured. Desktop display/launcher overhead can differ.

| Mode | Previous process-to-ready | Optimized process-to-ready | Optimized first frame |
| --- | ---: | ---: | ---: |
| Normal startup | 10.75–11.65 s | 0.81–0.88 s | 0.83–0.89 s |
| Launch into native save | 10.79–10.85 s | 0.81–0.87 s | 0.82–0.88 s |

Stage timing on the instrumented old reader attributed 11.05–11.19 seconds to
loading the 84,748,288-byte PCM bank. Scripts took 0.17–0.19 seconds. After the
changes, bank loading took 0.03–0.06 seconds and script preparation about 0.04
seconds. Display initialization and reserved-save migration together accounted
for about 0.62 seconds. The read-only `startup_stages_ms` CLI inspection field
reports these stages; timing telemetry is excluded from saves.

## Changes

- Cache each original file length and the total virtual BIN length once per
  open resource owner. Previously each small read repeatedly queried lengths
  across all 21 BIN streams. Reopening the source reconstructs the cache.
- Read logical ISO data and raw sectors in batches of at most 256 sectors.
  Raw reads remove each sector's header/ECC padding into the exact original
  2048-byte logical payload. Scratch storage is bounded to 602,112 bytes.
  The large sound-bank read now uses 162 physical requests instead of
  roughly 41,000. BIN/CUE and CHD retain the same projected-sector contract.
- Native saves in slot 0 or 5–99 restore directly after common initialization,
  bypassing title graphics, title sound bank preparation and startup storage
  state that the loaded save replaces. Original four-slot resumes keep their
  existing startup environment.
- An initial save load does not autosave the disposable pre-load engine state.
  Later interactive loads retain their existing autosave behavior.

All 39 scripts are still read, parsed and validated before play. The complete
sound bank is still loaded before readiness, including direct-to-save loads.
No persistent caches or deferred sound reads were added. Measurements did not
justify changing script or PCM ownership for the owner's source format.

## Validation and remaining cost

Eleven component suites passed. Disc tests cover batch-size/request-count
bounds, exact tails and allocation padding, nonzero track starts, ISO/raw data
and a mid-read failure. The actual CHD was compared against preserved logical
and BIN references: 650,786,112 logical bytes, 625,192,176 projected bytes, all
21 tracks, every resource and its rounded sector allocation, plus the existing
fault and odd-byte audio cases. Original game assets were read only.

Hospital, credits/Jaime, flashlight and JORDAN saves passed exact image/state,
future mixed audio/random continuation, replacement, backup and restart checks.
Cold direct-load images also match the pre-startup-optimization build. Gens and
Nuked passed startup save continuation, initial-autosave preservation, new-game
startup, normal save/autosave integration, and queued/percussion checkpoint
imports in fresh processes with identical future PCM samples.

A local compressed CHD copy started in about 3.0 seconds in both modes; about
2.2 seconds remained in full-bank decompression. Network storage may add time.
A future CHD-specific range/cache design would need separate proof of original
shared PCM transfer behavior and first-use latency. It was not introduced into
this change. Legacy music-history saves still incur their separate import cost
until resaved in the optimized save format.

Reproduce startup timing with `devtools/ce/test_snatcher_startup.py`. It accepts
an optional previous boundary RGB file to verify direct-load image equality.
Private receipts identify the tested executable, source captures and stage times.
