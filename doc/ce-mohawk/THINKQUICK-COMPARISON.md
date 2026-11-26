# ThinkQuick original comparison harness — step 3

November 26, 2026. Step 3 has started; implementation remains on hold.
PR #295 remains the unchanged draft step-1 inventory checkpoint.

## First executable observations

[Published observations](THINKQUICK-ORIGINAL-PROBES.json) record four batches:

| Batch | Original function results | Disagreements with then-current static contract |
| --- | --- | --- |
| batch-01 | 23/23 observed | One: Boolean numeric conversion |
| batch-02 | 31/31 observed | Zero after contract correction and added controls |
| batch-03 | 31/31 observed | Zero; actual results match batch-02 |
| batch-04 | 31/31 observed | Zero; same results using exact Git-normalized source bytes |

These execute original machine code in a real Windows WOW64 process, rather
than emulate the recovered algorithms. They cover signed/fractional integer
conversion, low-32-bit overflow, signed-short boundaries, Boolean payloads,
key numeric conversion, key comparison and key trigger matching. Expected
values are static-contract predictions, not a second executable's results.
The comparator's batch-02/batch-03 success establishes repeatability only.
**No original-versus-CE comparison or full VM/gameplay acceptance exists yet.**

A useful correction emerged immediately: tag-8 TRUE converts to 1.0, contrary
to the prior static prose. Branch reinspection confirms toReal -> 0x40fef4
and toInt -> 0x41003c. Both read the signed-short payload; it is not normalized
to 0/1. Zero, -1, noncanonical 2 and -32768 controls confirm both conversions.
The initial disagreement remains in batch-01 and the static contract has an
explicit erratum. No engine implementation was changed to match it.

The probes also observed fractional truncation toward zero, low-word wrap
(4294967297 -> 1), printable-character comparison ignoring modifiers,
nonprintable key inequality returning +1 in both directions, and trigger
matching rejecting modifier differences. Tag-11 toReal uses character/key byte,
while toInt returns zero. These are observations under the stated environment.

## Isolation and exact scope

`devtools/ce/probe_mohawk_v5_original.py` first rejects any executable other
than pinned CarmenTQ.exe SHA-256
219d81ea3d91b31da406e6891d01ba6ad12005e77b4d9f8f83930161dbfc9966.
It copies that executable into a **new** disposable output directory. It does
not copy or edit game archives, source assets, saves or owner installations.
Each case starts a fresh debugger-owned original process with a hidden startup
window. It handles the native loader breakpoint, then at the WOW64 loader break
redirects the initial x86 thread to a generated call stub. The original PE entry
point, bundled CRT startup and application startup are skipped. Only fixed
conversion/comparison/matcher addresses selected by the checked-in case plan
are called. No native file/registry or application initialization functions are
invoked by these probes. Windows loader/DLL initialization still occurs; this
is execution isolation by withholding application startup, **not an OS VM,
container, redirected registry or general filesystem sandbox**.

The stub sets x87 control word 0x037f using FNINIT, supplies generated values,
calls the original function, captures EAX/AX or x87 double, and breaks at an
exact generated trap address. The harness reads the result and terminates the
process without returning to application startup. Unexpected faults, timeout,
wrong image base, unexpected breakpoint and short memory transfers fail closed.
The debugger kills children on debugger exit; finally cleanup terminates/drains
each process. Timeout is bounded per case. Live process IDs and trap addresses
are in local receipts; the shareable summary omits those transient details.

Skipping original CRT/application initialization is a material boundary. It is
appropriate for these selected leaf paths, which do not require application
state, but is **not sufficient** for general expression evaluation, allocation,
file persistence, events, modal dialogs or gameplay. Do not extend the allowlist
to those paths and infer that their state is valid. Full-state probes need a
controlled initialized environment with verified file/registry ownership first.
The x87 mask/rounding state is imposed by the harness, not measured at normal
game startup. No claim about unmasked NaN/overflow faults follows from these runs.

## Reproduce and compare

Requires 64-bit Python on Windows with WOW64 debugging. Run from the authoritative
checkout, using fresh output directories (the harness refuses reuse):

```powershell
python devtools/ce/probe_mohawk_v5_original.py --exe '..\validation\thinkquick-20261125\disc\95instal\CarmenTQ.exe' --output-root '..\validation\thinkquick-20261126\step3\new-original-run'
python devtools/ce/compare_mohawk_v5_observations.py --reference '<first-run>\receipt.json' --candidate '<second-run>\receipt.json' --output '<new-comparison.json>'
python devtools/ce/test_mohawk_v5_observations.py
python devtools/ce/test_mohawk_v5_inventory.py
```

Each run writes the generated input plan, individual results and receipt with
original executable hash, harness hash, plan hash, host OS/Python identity,
initialization boundary and imposed x87 state. The published summary records
receipt/plan identities for all four batches, including the initial mismatch.
The comparator compares actual observations by exact unique case name; missing
cases, unavailable results and faults cannot pass as matching values. Static
expected fields do not override differing actuals. Its seven asset-free guard
tests use explicitly synthetic fixtures and are not original-runtime evidence.

Initial setup attempts are retained locally as smoke-01..04. The restricted
launch failed with WinError 623; a permitted debugger launch then exposed a
stub-buffer type error. An early harness treated the native loader break as
the WOW64 break, producing status 0x4000001f; that was a harness breakpoint issue,
not an original-language failure. smoke-04 produced the first valid conversion.
A later DebugSetProcessKillOnExit ordering error occurred before the expanded
batch created its output; the call was moved into the active debug session.
Only the named successful batches are counted as observations above.

## Remaining step-3 work

| Area | Current evidence | Next gate |
| --- | --- | --- |
| Pure conversions/key helpers | 31 original cases repeated; one static erratum | Matching CE observations through a test adapter; retain exact build identity |
| Bytecode/VM semantics | Static contracts only | Initialized evaluator, real-literal/truth/operators/errors, references, calls/defaults/returns |
| Lifecycle | Static ordering/lifetime contracts only | Controlled pages/resources, queued deletion, links and nested loads |
| Storage and UI | Static format/callback contracts only | Isolated initialized process, settings ownership, Retry/Cancel, cold restart |
| Full game | Existing CE startup failure unchanged | Normal original/CE flows and later gameplay qualification |

The comparator can accept future CE receipts with matching names, observed
values and executable/source identities. No CE adapter or semantic change is
introduced here. Next work is initialized original-VM probes and an observation
adapter using the existing CE test facilities. Keep storage/UI probes gated on
verified isolation; do not use owner profiles as an experiment.

## Preservation

After the runs, all 21 recorded engine-source hashes and all 346 CE Test file
hashes matched baseline, with no added CE Test files. No CarmenTQ process was
left running. No build, accepted-executable replacement, release promotion,
profile refresh or game-data mutation was performed. Local evidence lives under
validation/thinkquick-20261126/step3 outside the source checkout. Raw original
binaries and game assets are not published.

Publication verification: Git normalizes CRLF source to LF. Batch-04 reran all
31 cases using those exact committed harness bytes and matched batch-03. Raw
harness hashes in earlier receipts are retained; no older receipt is silently
reidentified as the publication run.

## Initialized-evaluator comparison checkpoint

[Initialized VM probes](THINKQUICK-INITIALIZED-VM-PROBES.md) and
[paired observations](THINKQUICK-VM-OBSERVATIONS.json) supersede the earlier
statement that no CE adapter/comparison exists. A test-only adapter now uses
generated assertions and the existing native CLI. Original heap/locks/evaluator
and value stack are initialized with verified postconditions. Six numeric
assertions produce three matching CE outcomes (including false control) and
three retained CE fixed-point decoding errors; overall comparison fails.
This is partial evaluator qualification, not full application initialization,
full VM/lifecycle/storage coverage or authorization to implement.

## Symbol/frame and error-completion checkpoint

[Symbol/frame probe details](THINKQUICK-SYMBOL-FRAME-PROBES.md) and
[paired receipts](THINKQUICK-SYMBOL-OBSERVATIONS.json) add nine matching string,
reference and native-call predicates, three error-completion mismatches and an
unqualified original numeric-string timeout. Corrected wrapper entry offsets
also reproduce the previous numeric results. The full 19-case comparison has
12 matches, three mismatches and four unqualified cases; overall fail.
This supersedes earlier pending symbol-context setup, but does not qualify
named/default script calls, local shadowing, lifecycle, persistence or gameplay.
Implementation remains on hold and PR #295 remains a draft.

## Numeric timeout and named-call checkpoint

The [numeric/local/named-call supplement](THINKQUICK-NAMED-CALL-PROBES.md)
resolves the earlier original numeric-string timeout by invoking original CRT
floating-parser initialization, and adds clean local-shadowing, supplied/default,
default-side-effect and nested named-script probes. Its process-local BCOD fetch
bridge is explicit and does not qualify resource loading/lifetime. The focused
11-case comparison has five matches, one exponent-string mismatch and five
unqualified CE call cases. Full 29-case replay: 17 matches, four mismatches,
eight unqualified, overall fail. Earlier timeout/setup receipts remain historical.
No implementation, build, accepted-installation update or PR #295 change.


## Return/recovery/reentrancy/context checkpoint — November 26, 2026

See [lifecycle probe contracts and limits](THINKQUICK-LIFECYCLE-PROBES.md) and
[paired observations](THINKQUICK-LIFECYCLE-OBSERVATIONS.json). Twelve new cases:
3 matches, 3 recovery-completion mismatches, 6 unqualified CE predicates.
Original reentry is directly counted twice per nested-code fixture; corrected
static nested-code token 0x64 to 0x74. Self-targeted sender context is measured;
distinct-target/link/deletion and expression-call return copying remain open.
No engine implementation; step 3 remains in progress.


## Distinct-target and expression-return checkpoint — November 26, 2026

[Cross-call probe report](THINKQUICK-CROSS-CALL-PROBES.md) and
[paired observations](THINKQUICK-CROSS-CALL-OBSERVATIONS.json): eleven clean
original predicates cover target 902 / sender 901, restoration, explicit/void
expression returns, left operands, saved copies, sibling and nested calls.
CE rejects 0x66/0x77, so all eleven new paired predicates remain unqualified.
Nested cross-item callbacks, links/deletion/resources and storage/UI comparisons
remain open. Step 3 continues; no engine implementation.


## Callback/linked-owner/unload checkpoint — November 26, 2026

[Owner/lifetime probe report](THINKQUICK-OWNER-LIFETIME-PROBES.md) and
[observations](THINKQUICK-OWNER-LIFETIME-OBSERVATIONS.json): nested callback gap
resolved by authored subentry padding; linked receiver 901/code-owner 902, own
before donor and unlink behavior directly traced. Eight new paired predicates
separate order from numeric item-ID conversion. Five additional original-only
unload control-flow probes use explicit fixture teardown/release callbacks;
they do not qualify real destructors/cache or add to paired totals.
Cross-resource code selection, dangling donors and real resource lifetime remain
open. No engine implementation; step 3 continues.

Fresh 60-case paired replay: **23 matches, 10 mismatches, 27 unqualified**.
Eight new cases = 3 matches, 3 item-ID-conversion predicate mismatches, 2
unsupported sender-token cases. CE matches the isolated link ordering/removal
controls. Five bounded original unload controls are recorded separately.
