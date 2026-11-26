# Distinct-target sender/self and expression-call returns

November 26, 2026. Step 3 comparison research; no engine implementation/build.

Eleven new original fixtures complete with zero VM errors. CE cannot evaluate
them: sender token 0x66 and invocation token 0x77 are unsupported. The eleven
paired predicates remain **unqualified**, not false values or measured semantic
mismatches. Machine-readable observations identify exact programs, source and
executables in THINKQUICK-CROSS-CALL-OBSERVATIONS.json.

| Fixture | Measured original predicate |
| --- | --- |
| distinct-sender-self | Authored numeric-ID send from item 901 to item 902: target self ID 902 and sender ID 901 |
| distinct-sender-false-control | Deliberately expects the recorded context predicate to be zero: false |
| distinct-sender-restored | Sender is numeric zero after cross-item call |
| expression-return | Named expression call returns 42 into a global assignment |
| expression-return-false-control | Deliberately expects 43 instead of 42: false |
| expression-return-preserves-left | Left operand survives call: 3 + returned 42 = 45 |
| expression-return-copy | First saved result stays 42 after a separate call returns 99 into a second variable |
| expression-return-void | A handler evaluates 17 then returns without an expression; caller receives 17 |
| expression-return-nested | Outer expression-return adds 1 to inner returned 42; caller receives 43 |
| expression-return-siblings | Two calls within one expression preserve distinct results: 42 + 99 = 141 |
| expression-return-distinct | Expression call targets item 902 and propagates its returned 42 to caller 901 |

Items are compared by explicitly converting item values numerically with +0,
then comparing their IDs; mixed item/integer equality is not assumed equivalent.
The void-return result is bounded to a handler with a preceding expression 17;
it does not establish a default result for an empty handler.

## What executes and what is instrumented

Generated item 901 and, where required, item 902 use real original constructor
0x4185d0, field/script parsing and authored named-event dispatch. Both share the
same generated BCOD. The existing BCOD fetch bridge is retained: original VM,
formal/frame/argument/result behavior executes; archive cache fetching does not.
A minimal page/owner fixture supplies accessed context, as previously documented.

Expression calls execute original token-0x77 handler 0x40498c, named-call helper
0x4089f3 and original dispatch helper 0x4068f0. The latter reads/writes the current
item-ID word at global context pointer 0x49a738 +0x4c. For these probes only, that
pointer names zeroed fixture memory; the original helper reads/writes the word.
No full engine-context constructor or application entry is claimed. Postconditions
verify that word returns to zero, alongside sender, symbol/frame/value/argument
stack and item/code-owner restoration. Both constructed items are checked.

The original expression reader consumes argument/target delimiters plus a
trailing token before resuming the surrounding expression. Generated no-argument
self calls use `name 77 10 10 10 07`; explicit item-902 expression calls use
`name 77 10 10 <902> 07 10 07`. These exact bytes and surrounding statements are
fingerprinted. Early malformed delimiter attempts raised errors 14/16 and remain
retained locally. They are not qualified return or error-recovery observations.

A preliminary two-statement copy fixture only ran the first statement because
the evaluator returns at its statement boundary. The final copy fixture uses two
authored script entries; four fetch-bridge calls prove both statements and both
callee return expressions execute. The sibling-expression fixture independently
checks that the first return survives a second call before addition.

## Comparison scope and remaining work

Original fixture construction/parsing registers both items before the caller's
listload dispatch. CE BITL puts the target first so its named handler is available
before caller listload. This tests the same authored context/call graph, not a
new loader-ordering claim. Second-item fields and the fixed target identity are
included in program fingerprints; ordinary value probes reject unexpected errors.

All original new predicates and false controls have clean postconditions. CE
reports unsupported token 66 for context checks and token 77 for expression-call
checks. Execution failure is retained without substituting an expected result.

Nested cross-item callback behavior remains open: a diagnostic 901 -> 902 -> 901
attempt did not observe its expected back-handler effect and is not promoted to
a qualified fixture. Earlier diagnostic receipts are retained. Distinct target
identity and normal restoration are now measured; arbitrary callback reentrancy,
linked receiver/code-owner differences, deletion and resource lifetime are not.
Return propagation is measured for these integer results, not every value type,
argument/default combination, target collection, failure path or empty void call.
Resource loading beyond the bridge, persistence/cold restart and OS-error UI
comparisons remain step-3 work. Implementation remains on hold.

## Reproduction and preservation

Use probe_mohawk_v5_vm.py modes `original` / `ce` with documented executable,
extras and game-data arguments and a fresh disposable `--root`. Add
`--cross-calls-only` for these eleven cases; no filter runs all 52 cases.
The original executable SHA-256 remains
219d81ea3d91b31da406e6891d01ba6ad12005e77b4d9f8f83930161dbfc9966;
CE baseline remains
04c96176beb3f4a8a5e53f0be20c971aab603d625057c09c9a548cb2d89a9795.
No engine build, owner installation update or release promotion occurred.

Fresh 52-case replay: **20 matches, 7 mismatches, 25 unqualified**, zero
incomparable programs; overall fail. Eleven comparator guards and eight
inventory tests pass. All 21 recorded engine-source files, 346 CE Test files
and the baseline executable match preservation hashes.
