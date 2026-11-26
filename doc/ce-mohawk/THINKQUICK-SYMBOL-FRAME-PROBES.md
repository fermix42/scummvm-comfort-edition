# Symbol/frame, string, reference, call and error probes

November 26, 2026. Step 3 continues; no engine implementation or rebuild.
See [paired observations](THINKQUICK-SYMBOL-OBSERVATIONS.json) for exact inputs,
source dependencies, executable identities, raw-receipt hashes and outcomes.

## New matched scope

| Probe group | Original result | CE result | Qualification |
| --- | --- | --- | --- |
| String equality, unequal-string false control, concatenation | true, false, true | same | Three predicate matches |
| Symbol assignment/read, copy by value, compound reference assignment | true | same | Three predicate matches |
| Native stringLen, abs, min calls | true | same | Three predicate matches |
| Division by zero | VM error 4, typed catch, return | engine abort with division diagnostic | Error-completion mismatch |
| Assignment without reference | VM error 14, typed catch, return | engine abort with reference diagnostic | Error-completion mismatch |
| stringLen with zero arguments | VM error 16, typed catch, return | engine abort with parameter diagnostic | Error-completion mismatch; no assertion that both use the same error taxonomy |
| Numeric string comparison | bounded timeout | true | Unqualified original result |

The 13 new cases therefore give **nine matches, three mismatches and one
unqualified case**. Replaying the six earlier numeric cases at corrected entry
offsets adds three matches and three retained CE fixed-point decoder errors:
**19-case aggregate: 12 matches, three mismatches, four unqualified; overall fail.**
No timeout or engine failure is substituted with zero. Error comparisons measure
completion (`returned` versus `engine-abort`), not equality between a stale
shared-result value and a false assertion. Original error codes and returned
numeric/tag details are retained separately. The first-chance C++ exception is
delivered to the original handler with DBG_EXCEPTION_NOT_HANDLED; reaching the
harness return trap verifies that its typed catch and subsequent cleanup ran.

## Original symbol and frame setup

After the original heap initializer, run original CRT thread-state initializer
0x47a9ec. It initializes static locks, allocates a TLS slot and thread data.
Construct a symbol table with 0x41482c(1024), push a local frame through
0x414c47, then pass that table to evaluator constructor 0x402eef. Evaluate each
case's generated setup expressions and final assertion in the same evaluator.
Pop the frame with 0x414d09 after capturing the result.

Postconditions verify CRT-thread initialization success, the **16-bit** frame-
push result, restored frame pointer/depth after pop, evaluator construction,
non-null value stack, empty evaluation stack and cleared active code. Native
symbol writes use authored bytecode assignments; values are not injected into
symbol cells to force the expected assertion. Copy testing sets a=1, copies b=a,
then changes a=2 and checks b==1. Compound assignment uses original opcode
0x6b/0x13, confirmed through its secondary table. This is global-symbol/reference
qualification under an initialized frame, not proof of local shadowing, nested
call frames, defaults, alias-cycle safety or arbitrary reference lifetime.

Calls here are general/native command calls to stringLen, abs and min. Named
item-script calls, sender/self, formal/default binding and nested returns remain
separate pending probes. No item/resource graph or full game startup is claimed.

## Wrapper and entry-offset correction

This pass corrected a harness interpretation error in the earlier checkpoint.
The original reader **peeks** the third word without advancing its code base:
both original and CE address code from wrapper +8, with identical entry offsets.
The reserved first code word is the original string-table offset; it is set to
the generated code length. The original can consequently resolve the same
cache-prefixed identifier/string table used by CE. CE's wrapper retains its
unused reserved word; executed expressions and string payloads are identical.
Only that non-executed wrapper metadata differs.

Earlier original fixtures subtracted four from entry offsets. The first numeric
expression happened to traverse zero padding; subsequent setup expressions
could enter preceding code. Those reference failures were harness artifacts,
not original language behavior. The corrected full replay confirms the six
previous numeric outcomes and the new reference/string results. Earlier setup
attempts and diagnostics are retained under symbol-original-01..06 and
reference-diagnostic-01/02; no failed setup is silently promoted to acceptance.

Matched fingerprints now include the common complete generated program:
setup expressions, string table and final expression, as well as the final
expression's own hash. This prevents unrelated initial states or string operands
from passing merely because their final expression bytes or case labels match.
Comparison guard tests also reject mixing error-completion and numeric-value
observations. Ten comparator guards and eight inventory tests pass.

## Error instrumentation and boundary

The debugger sets a one-byte breakpoint at original VM-error entry 0x402fc6.
On entry it records the error argument, restores the original byte, resets EIP
to execute the original function, and continues. It does not bypass the throw
or fabricate a return value. Each deliberate error records one first-chance
C++ exception, then reaches the return trap with stack/code/frame postconditions
satisfied. CE's corresponding fatal diagnostics and process termination are
retained as distinct completion behavior. This is a controlled comparison, not
a policy decision that CE should silently ignore every error.

The original numeric-string case still times out in this manually initialized
environment. Its CRT floating-parse prerequisites remain unresolved; this is
not qualified as an original hang or a semantic mismatch. Application startup,
full CRT/global initialization, storage, profiles and modal dialogs remain
outside the harness. All processes are bounded and terminated in cleanup.

## Reproduction and preservation

Use the commands in THINKQUICK-INITIALIZED-VM-PROBES.md. Add `--symbols-only`
to either runner to select these 13 cases. Without that switch, both runners
collect all 19. Always use new disposable roots. The published full-run receipts
come from context-original-full and context-ce-full, with comparison in
context-comparison.json. The complete comparison intentionally exits nonzero.

The exact final source bytes and dependency hashes are recorded. CE still uses
the preserved integrated baseline executable; no build or engine adapter change
was made. All 21 recorded engine-source hashes and all 346 CE Test file hashes
remain preserved. PR #295 stays an unchanged draft. Step 3 remains open for
numeric-string CRT setup, deeper frame/script-call/return/error-state probes,
lifecycle/resources and isolated persistence/UI comparisons before implementation.

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
