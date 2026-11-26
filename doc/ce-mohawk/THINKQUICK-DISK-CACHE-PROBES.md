# Disk-backed original cache and matching CE predicates

November 26, 2026. Step 3 remains active; engine implementation is prohibited.
See THINKQUICK-STEP3-GATE.md for the full remaining completion audit, and
THINKQUICK-DISK-CACHE-OBSERVATIONS.json for exact source/data/run identities.

## What is now exercised

Six original cases use an actual generated MHK on disk, real original path and
filesystem objects, original archive parser/open (0x45a9bc), numeric resource lookup
(0x458edc), cache retain/load (0x45a126), memory lock/unlock, release (0x45a2c3),
archive close (0x45a3c8), and memory-handle validity queries. RMAP/FILE metadata
is created by original file/archive routines rather than seeded by the harness.
The complete BCOD payload is copied from the original cache pointer and checked
byte-for-byte against the generated input. All input archive hashes remain intact.

| Original case | Measured outcome |
| --- | --- |
| disk-true | Actual cold-loaded bytecode computes 6 + 7 == 13 -> 1 |
| disk-false | Actual cold-loaded bytecode computes 6 + 7 == 99 -> 0 |
| disk-release-reload | Final release invalidates the data handle; a second load yields the exact full payload again |
| disk-close-reopen | Release, actual map close and invalid-map check; reopen/read yields the exact full payload again; final close invalidates the new map |
| disk-missing-id | Open succeeds; missing ID lookup returns zero and original error 0x28a3 |
| disk-bad-magic | Malformed archive open returns zero and original error 0x28a4 |

All six qualify on two runs of the exact final source in fresh owned processes.
Their value/contract observations reproduce; raw process addresses are not treated
as stable output. The four lifecycle/negative controls are original-only.
The seven earlier resident-cache/deleting-destructor cases also reproduce under
the updated diagnostic transport.

## Explicit original scheduling adaptation

The manual original environment does not run the application message pump or full
cooperative scheduler. Earlier disk attempts raised an exception on a secondary
worker whose entry was original 0x483754. The previous diagnostic incorrectly
read the main thread's registers/stack for that event; those stack interpretations
must not be used as evidence about the faulting worker. The updated transport
records thread identities, retrieves the faulting thread's context and treats
optional stack reads as bounded best-effort diagnostics. A deliberate child UD2
fixture verifies the event's secondary thread and matching fault EIP; it is a
debugger guard, not a language compatibility result. Debugger-owned created-thread
handles are closed for every case.

For disk probes, one process-local patch makes worker adapter **0x48366f** return
its existing unsupported-thread status **0x12c**. Original caller **0x45641c** then
runs original operation **0x456411** synchronously through its existing fallback.
No archive, path, file read, cache, allocator, bytecode or return-value result is
substituted. This serializes file operations; asynchronous scheduling and contention
are outside qualification. The original EXE on disk is unchanged.

Heap/TLS/floating conversion and original memory-handle initialization execute.
An original current-thread object supplies the fields synchronous mutex paths
access; cooperative scheduling suspension fields are explicit fixture state.
Original filesystem initialization runs, followed by the original constructors
for the cache root/group without unrelated configuration-file initialization.
Absolute generated paths are bounded below the new disposable root and under the
original path length limit. Application entry remains withheld. These are selected
original library operations, not normal game startup or an OS sandbox.

## Matching CE observations

The same generated MHK bytes contain the common expression plus an authored CE
success/failure sentinel. CE runs them through its unchanged native CLI baseline.
Success additionally requires ready page phase and loaded item 901; a false result
requires the exact existing SetPageFade sentinel diagnostic. Native original and
CE receipts share whole-archive and expression fingerprints. Both predicates
match: original/CE true = 1; original/CE false = 0.

This is **two new paired matches**. Cache release/reload/close and error-code
controls do not have matching CE lifetime observations yet and are not silently
added to paired totals. Cumulative paired evidence is prior 60 cases plus these
two: 25 matches, 10 mismatches, 27 unqualified. This is not a fresh monolithic
62-case replay; each receipt retains its own source/build identity. The final
Step-3 aggregate runner/replay is still required.

## Reproduction and remaining work

Run probe_mohawk_v5_disk_cache.py with mode `original`, `--original <pinned EXE>`
and a fresh `--root`. Mode `ce` additionally uses the documented baseline `--ce`,
`--extras`, `--game-data` and its own fresh `--root`. Run
`test_mohawk_v5_fault_thread.py --original <pinned EXE> --root <fresh root>` to
verify native fault attribution. No GUI input is needed for these probes.

Original and CE executable hashes are unchanged from the previous checkpoint.
Exact committed sources, full archive/expression fingerprints, repeated native
results and the paired comparator output are verified. Eleven comparator guards,
eight inventory tests and the deliberate native fault-thread guard pass.
Owner engine/executable/profile/save preservation remains required.

Next gates remain: representative packed/operator and lifecycle/queued-mutation
coverage, pressure-driven eviction distinct from explicit release, component and
owned storage/cold restart probes, OS-error retry/cancel evidence, matching CE
outcomes where possible and aggregate published-source reproduction. Step 3 is
not complete, and Step 4 must not begin in this goal.
