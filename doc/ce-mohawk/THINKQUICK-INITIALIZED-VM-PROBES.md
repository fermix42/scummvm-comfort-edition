# Initialized original evaluator and matching CE observations

November 26, 2026. Step 3 checkpoint; no engine implementation or rebuild.
[Machine-readable results](THINKQUICK-VM-OBSERVATIONS.json) retain original
and CE identities, exact expression hashes, initialization checks and failures.
This extends the earlier leaf-function harness without claiming a fully started
game, fully initialized CRT or complete VM/lifecycle/persistence qualification.

## Actual paired results

| Identical generated assertion | Original evaluator | Existing CE executable |
| --- | --- | --- |
| 6 + 7 == 13 | 1 (true) | 1 (true) |
| 6 * 7 == 42 | 1 | 1 |
| 6 + 7 == 99, negative control | 0 (false) | 0 (false sentinel) |
| fixed-point 1.5 + 0.5 == 2 | 1 | Engine error: token 13 at 18 |
| fixed-point 0.5 OR 0 == 1 | 1 | Engine error: token 00 at 18 |
| NOT fixed-point 0.5 == 0 | 1 | Engine error: token 00 at 18 |

Three assertion observations match. Three cases are **unqualified**, because CE
aborts before producing the assertion outcome. The overall comparison fails;
there are no fabricated zero results for these errors. The negative control is
expected false and establishes that the observation channel can distinguish
false from true. These are predicate observations, not raw CE stack-value reads.

The errors are consistent with the independently recovered real-literal width
mismatch: original four-byte fixed-point encoding versus CE's provisional
8-byte-double reader. The actual CE logs above are evidence; identifying the
width mismatch as the cause is an inference supported by source inspection.
Fractional logical semantics remain unqualified on CE because decoding fails
first. Do not replace original encoding with CE's format to obtain a passing
comparison. No corrective engine implementation accompanies these findings.

## Original initialization and isolation

`devtools/ce/probe_mohawk_v5_vm.py` uses the existing debugger transport and the
same pinned original EXE. Each case has a fresh disposable process. After the
WOW64 loader break, the generated stub runs these original functions:

1. 0x47cd44(1): original private CRT heap initialization; capture success.
2. 0x47b977: initialize the original static critical sections.
3. 0x401ed0: initialize the shared result value at 0x49a540.
4. 0x402eef: construct a fresh evaluator with a null symbol context; its
   constructor allocates/constructs the 64-value evaluation stack.
5. 0x40375e: evaluate the generated original bytecode; 0x40fe5d extracts the
   resulting Boolean as a double for the common observation receipt.

The transport verifies heap-init return 1, constructor identity, non-null value
stack, empty stack cursor after evaluation and cleared active-code pointer.
Every successful probe returned result tag 2. Thus this is an initialized
**evaluator** run, rather than another call to isolated arithmetic helpers.
The imposed x87 state remains 0x037f. Application startup/PE entry, global
application constructors, source compiler, complete CRT startup and settings
loading are still skipped. No symbol/frame context is claimed initialized: these
six expressions use no names, item receivers or component objects.

The generated BCOD wrapper includes padding and numeric expressions only.
The original reader consumes three header words and has a +12 code base; CE's
reader uses +8. The adapter supplies offsets 4 and 8 respectively to point at
**the same expression bytes at wrapper offset 16**. It does not change opcodes,
literal representations or expressions between runtimes. This is a narrow
numeric wrapper, not a claim of general original archive/string-table support.
Name/string/reference probes need a verified symbol context and wrapper layout
before they can be added. Expression SHA-256 must match across the paired rows.

No native filesystem, profile or dialog functions are invoked. The heap exists
only in the disposable process; it is reclaimed by termination. The original
application is never resumed after the result trap. This continues execution
isolation by withholding application startup, not a general OS sandbox.
Storage/UI and full application probes remain gated on stronger isolation.

## CE observation channel

The existing, unchanged native test CLI launches generated MHK pages with the
same expression bytes in an authored condition. A true condition skips the
failure sentinel. A false condition executes the existing deliberately
unsupported nonzero SetPageFade operation, whose exact diagnostic is recognized
as false. Other engine failures remain errors. Positive observations additionally
require inspect.adapter phase >= 2 and loaded generated item 901; process
survival alone is insufficient. The isolated profile and all generated archives
live under a new validation root. The existing CLI's owned-root storage rules
remain in force; accepted profiles are never used.

No engine adapter was added: this is an external adapter over generated scripts,
existing read-only inspection and retained engine logs. Its limits are explicit:
Boolean assertions only, bounded observation interval and one small numeric
page. It does not expose arbitrary VM state or establish gameplay compatibility.
The comparator now rejects differing expression fingerprints under the same
case name. It still rejects missing/fault/unqualified results; eight synthetic
comparison guard tests pass, alongside eight inventory boundary tests.

## Reproduction

From the authoritative checkout, supply new roots each time:

```powershell
python devtools/ce/probe_mohawk_v5_vm.py original --original '..\validation\thinkquick-20261125\disc\95instal\CarmenTQ.exe' --root '<new-original-root>'
python devtools/ce/probe_mohawk_v5_vm.py ce --ce 'build-scummvm\Releasex64\scummvm.exe' --extras 'dists\engine-data' --game-data '..\validation\thinkquick-20261125\disc\95instal' --root '<new-ce-root>'
python devtools/ce/compare_mohawk_v5_observations.py --reference '<original-root>\receipt.json' --candidate '<ce-root>\receipt.json' --output '<new-comparison.json>'
```

The CE runner and comparison exit nonzero while the three unsupported cases
remain unresolved. That is an expected failing comparison, not a harness pass.
Original initialized results reproduced across vm-original-01/02/03; explicit
initialization checks were added for 02/03. CE outcomes reproduced across
vm-ce-01/02/03; loaded-page qualification is enforced by the final 03 runner.
The final raw receipts and logs remain under validation/thinkquick-20261126/step3.
Published dependency hashes distinguish historical runner versions. Do not
relabel older observations as results from the newer checked-in runner.

The CE executable remains the preserved integrated baseline SHA-256
04c96176beb3f4a8a5e53f0be20c971aab603d625057c09c9a548cb2d89a9795.
No build was needed. All 21 recorded engine-source hashes and 346 CE Test file
hashes match baseline; owner saves/installations are unchanged.

## Remaining gates

Step 3 remains in progress. Next work is original symbol/frame initialization
and matched string/reference/call/error probes; correct original wrapper offsets
for those consumers; lifecycle/resource probes; isolated persistence/cold
restart and Retry/Cancel behavior; and broader comparisons before implementation.
Three matching predicates do not accept the entire arithmetic or VM subsystem.
PR #295 stays an unchanged draft; this research remains on the step-3 branch.
