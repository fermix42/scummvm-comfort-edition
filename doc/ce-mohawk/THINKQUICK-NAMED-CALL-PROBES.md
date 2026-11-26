# Numeric parsing, local shadowing and named-script calls

November 26, 2026. Step 3 research checkpoint; no engine implementation/build.
See [paired observations](THINKQUICK-NAMED-CALL-OBSERVATIONS.json) for exact
program/source/executable identities, results and retained failure boundaries.

## Numeric-string timeout resolved

The stalled original comparison entered the CRT runtime-error/message-box path,
not a qualified infinite loop in language comparison. Debugger stack candidates
included sscanf return 0x477eb7, numeric predicate 0x40fe25, runtime-error output
0x47ec84 and message-box helper 0x48179f. The harness had initialized heap and
thread state but skipped floating-parser registration normally performed by
CRT initialization. The original initializer at 0x477810 calls 0x477828 to
install the floating conversion function table at 0x4992a8..0x4992bc.

Calling **that original initializer** resolves the timeout. The harness verifies
the expected parser entry at 0x4992ac, then reapplies controlled x87 word 0x037f.
No parser implementation, language result or CE engine behavior was substituted.
Earlier timed-out diagnostic processes were terminated and remain retained.
The debugger now also lets the Windows loader finish and stops at original PE
entry 0x479123 before executing it. Application/CRT entry itself remains skipped;
only explicit reviewed initializers are invoked.

| Numeric-string assertion | Original | CE baseline |
| --- | --- | --- |
| "02" == 2 | true | true |
| whitespace-padded "02" == 2 | true | true |
| "2e0" == 2 | true | false |
| "2x" == 2 | false | false |

Exponent syntax is a measured semantic mismatch. The invalid-tail control
ensures successful partial parsing is not treated as a complete numeric string.
The former timeout is resolved in the new environment; its historical receipts
remain unqualified and are not silently relabeled as successful runs.

## Named/default/nested and shadowing probes

The new fixtures use original-compatible executable bytecode, including token
0x63 to build actual arguments and token 0x62 to evaluate/bind formal defaults.
A formal initializer is an assignment expression; a bare name plus value leaves
extra stack values. A local declaration consumes an internal terminator and
needs a separate outer terminator. Early malformed fixtures and their original
errors remain in calls-original-01..04 and local-diagnostic-01/02.

Original results, all with zero unexpected VM errors:

- Supplied named argument: callee writes 42 to global result.
- Omitted argument: callee uses default 7.
- Default side effect with supplied argument: effect count becomes 1 before
  supplied 42 replaces the default value.
- Local shadowing: handler reads local temporary=5; after return the global
  temporary remains 99. The false control deliberately expects 100 and is false.
- Nested named call: outer dispatches callee with actual 42; result propagates
  through the authored assignment and frame/context state is restored.
- Named false control deliberately expects 43 after supplying 42 and is false.

CE matches both local-shadowing controls. Supplied/nested/default-side-effect
calls stop at unsupported token 63; the omitted-default fixture stops at CE's
too-few-arguments check. The named false control is likewise blocked by token
63, not counted as a successful false result. These remain **unqualified CE
predicates**, not fabricated values or evidence that calls are compatible.

The focused 11-case run has **five matches, one mismatch and five unqualified
results; overall fail**. The full 29-case replay has **17 matches, four mismatches
and eight unqualified results**. Existing error-completion differences and
fixed-point decoder failures remain visible. No gameplay acceptance is claimed.

## What actually executes, and the code-fetch bridge boundary

`mohawk_v5_call_probes.py` generates the authored named handlers and call sites.
The original-process fixture uses real item constructor 0x4185d0, original field/
script parser via item event 1, and original listload/named script dispatch.
It initializes the original argument-stack root with 0x402e3f, the symbol/frame
context and evaluator, and runs authored setup assignments. A minimal in-memory
page/resource-owner fixture supplies the fields these paths access; it is not a
fully loaded game page or a claim that page/media initialization is qualified.

**Explicit instrumentation:** a process-local jump at 0x41d38d redirects only
BCOD fetching to a generated bridge. The bridge supplies the generated BCOD
pointer and original offset to original evaluator 0x40375e, retaining the
receiver. It replaces the resource-cache lock/fetch/unlock glue; it does not
replace name matching, script parsing, argument construction, default/local
binding, nested dispatch or evaluation. Bridge invocation counts are recorded.
Both true and false controls execute through it. The original EXE on disk is
unchanged. This is an instrumented original-process semantic probe, not an
unmodified full-game run or validation of resource handles, linked code owners,
archive lifetime, unloading, media or persistence.

Postconditions verify empty original argument stack, restored receiver/code
context and active counter, restored page sender, balanced symbol frames,
empty value stack and cleared active code. Global temporary/result assertions
check the visible state around local and nested calls. These are bounded
examples, not proof of arbitrary recursion, sender/self behavior during every
callback, every omitted/surplus rule, return form or error unwind path.

## Observation integrity and reproduction

Program fingerprints include generated setup expressions, strings, final
expression **and authored item/script fields**. The comparator rejects true
predicates accompanied by unexpected VM errors; an earlier shadowing fixture
returned true after an error and is explicitly excluded from clean qualification.
Eleven synthetic comparator guards and eight inventory tests pass.

Use the commands in THINKQUICK-INITIALIZED-VM-PROBES.md, adding `--calls-only`
to select these 11 cases. Without that switch the runner collects all 29.
Always supply new disposable roots. The original runner uses fixed reviewed
initializers and fixture addresses; there is no general user-supplied native
call command. CE continues to use generated MHK pages through its unchanged
native test CLI and owned isolated settings roots.

Final focused receipts: calls-original-final and calls-ce-final. Full replay:
calls-expanded-original, calls-expanded-ce and calls-expanded-comparison.json,
under validation/thinkquick-20261126/step3. Published runtime/dependency hashes
identify the exact runners; raw assets, original binaries and full logs remain
local. The CE executable remains baseline 04c96176...b89a9795. All 21 recorded
engine-source hashes and all 346 CE Test file hashes remain unchanged.

Step 3 continues with broader return/error/reentrancy cases and resource/lifecycle
qualification, then isolated storage/UI/cold restart. Keep implementation on hold.
PR #295 remains an unchanged draft; this work stays on the step-3 research branch.
