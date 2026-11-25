# AGI timing inspection

The normal executable's opt-in native CLI can launch detected AGI games with
`engine:"agi"`, their normal game ID, and `platform:"coco3"`. Detection is still
required. No platform override bypass or resource mutation is provided.

`inspect.adapter` reports started/completed logic-cycle counters, current
room/logic, script variable 10 (`time_delay`), player control, sound enabled,
music-wait count/current elapsed/last elapsed milliseconds, and the first 32
animated or drawn screen objects (ID, position, view, loop, cel, cycle time).
These are observations, not commands. Inspecting does not advance a counter.

`sound` reports current resource and playback status. For the CoCo3 generator,
`sound.generator` reports the last resource, number of starts, actual queued
note count, raw duration sum, selected duration units per second, and queued
microseconds. Durations are counted in the existing playback loop without
changing its scheduling or its existing KQ3 interpreter heuristic.

AGI's `advance.frames` means completed `interpretCycle()` calls. Blocking music
does not complete a cycle. Inspect and input remain available through the
normal event loop during that wait. Use numeric ScummVM key events (Enter 13,
Right 275), with distinct down/up requests. The MI4 logical adapter is not an
AGI input API. No desktop input is used.

## CoCo3 pilot, November 25, 2026

Evidence root in the private workspace:
`validation/coco3_sound_investigation`.
Candidate executable SHA256:
`5ca0a08b7fb49ad5a58598f5f874f3f689b4cb671268bb2a3190a75b963b73a5`.
The clean-environment Release build passed with zero warnings/errors.

`probe.py` ran 32-second headless observations for standalone and collection
versions of Black Cauldron, Gold Rush, King's Quest II, Manhunter 1,
Manhunter 2 and Mixed-Up Mother Goose. `summarize.py` aggregates the sampled
intervals; `verify.py` checks the resulting assertions. JSONL transcripts retain
native requests/responses, executable identity and read-only window checks.

- All 12 processes launched as CoCo3 using SDL's dummy video driver.
- 29 sampled uninterrupted music waits had zero completed logic-cycle changes
  and zero changes to the reported objects. This is a sampled observation,
  alongside the source-defined blocking behavior, not proof of every game scene.
- Black Cauldron's same title sound queued 23.449992 seconds in both variants;
  observed completed waits were approximately 23.8 seconds including output
  buffering. Afterward the scripts resumed naturally.
- Outside music, the Black Cauldron opening ran at 39.77 cycles/s standalone
  versus 10.09 cycles/s collection. Logic 107 changes `assignn(10,0)` to
  `assignn(10,2)`. This matches the normal engine's delay scheduling.
- Manhunter 2 is the identical-data control: approximately 10.02 versus
  10.03 cycles/s in its opening. Other rates include scene-specific waits and
  must not be interpreted as universal game-speed benchmarks.
- All six pairs' decoded sound resource bytes are identical. Several scripts
  differ, including explicit speed assignments. Compression/message encoding
  differences must not be mistaken for changed executable logic.
- `interruption.py` tests Enter during music for both Black Cauldron and KQ2
  variants, continues into gameplay, then injects Right and advances eight real
  cycles. All four cases resume and move the player normally. An initial harness
  assertion incorrectly assumed Black Cauldron started in room 1; the corrected
  assertion uses its actual room 8. Historical failed-attempt logs are retained.
- No test process exposed a visible window. The accepted Test executable,
  profiles, saves and game assets were not replaced or edited.

This pilot tests normal startup, sound completion/interruption and selected
movement. It is not a full playthrough, a hardware-emulator comparison, an
audio fidelity assessment, proof of the KQ3 duration heuristic for every port,
or AGI save/load/persistence acceptance. No sound-blocking or gameplay-speed
behavior was changed.
