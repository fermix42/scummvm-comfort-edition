# Return, recovery, active evaluator and sender/self probes

November 26, 2026. Step 3 research; no engine implementation or rebuild.

Twelve new paired fixtures give **3 matches, 3 mismatches and 6 unqualified CE
observations**. All twelve original fixtures complete with the required
postconditions. See THINKQUICK-LIFECYCLE-OBSERVATIONS.json for identities and
measured results. Existing accepted executables, profiles and saves are unchanged.

| Probe group | Original observation | CE baseline |
| --- | --- | --- |
| Return expression, void return, false control | Return expression assigns 42; void preserves prior 41; both skip unreachable assignment 999; false control is false | All three match |
| Recovery after divide-zero, non-reference assignment, stringLen arity error | Original errors 4, 14, 16 respectively, one first-chance C++ exception each; following script entry writes 42 using the same VM/frame | All three abort the engine: measured completion mismatches |
| Two nested code evaluations, side effect, false control | Preserves operands: 2 + (3 + 7) = 12; shared symbol side effect is 7; wrong expected 13 is false | Unsupported token 74; predicates unqualified |
| Sender/self equality, false control, sender restoration | Self-targeted named handler sees receiver equal sender; wrong expected value is false; sender is zero after call | Unsupported token 66; predicates unqualified |

## Correction to static token mapping

The earlier contract table mislabeled nested-code evaluation as token 0x64.
The original dispatch tables at 0x4051b3 and 0x405137 map **0x74** to handler
0x404f33. Runtime probes using 0x74 confirm two entries through the already-active
branch at 0x4037ba. Earlier attempts with 0x64 failed qualification and remain in
local evidence; their false predicates are not accepted language observations.
The contract table is corrected. This does not alter the asset inventory decoder.

## Instrumentation and qualification

The existing BCOD fetch bridge and minimal page/owner fixture are retained:
original item construction, field parsing, script dispatch, evaluator, symbols
and exception catch execute. Archive-cache fetching is bypassed. The original
application entry does not execute; selected original CRT/VM constructors do.

For reentrancy only, a process-local jump at 0x4037ba counts the original
already-active path. Its trampoline increments a private counter, executes the
displaced `mov ecx,[ebp-0xa4]`, then resumes at 0x4037c0. The original recursive
evaluator construction, evaluation and destruction remain intact. Exactly two
reentries are required; output value alone cannot qualify the fixture.

Recovery fixtures require their exact single original error code, then observe
the following assignment and final predicate. Unexpected errors still reject
ordinary value probes. Completion records retain both `returned` with the
measured recovery predicate and CE `engine-abort`; stale zero is not treated as
recovery. Final stack/frame, argument stack, item/code-owner context and sender
postconditions are checked. These bounded cases do not establish arbitrary
exception safety, rollback, or recovery during every nested call.

Return fixtures establish optional-expression side effects and script-loop
termination. They do not yet establish expression-call return-value copying,
void return's shared-result contents, or every nested return interaction.
Sender fixtures use a single real item calling itself; they establish that
bounded context plus restoration. Distinct sender/target identities, linked code
owners, nested cross-item callbacks and deletion during callbacks remain open.
No resource lifetime, full page startup, persistence/cold restart or OS dialog
qualification is claimed. Step 3 remains in progress; implementation stays on hold.

## Reproduction

Use probe_mohawk_v5_vm.py with mode `original` or `ce` and the documented
executable/data/extras options, adding `--lifecycle-only` for these twelve cases.
Always use a new disposable `--root`. Without the filter all 41 fixtures run.
Historical named-call observations remain separately identified.

The fresh expanded 41-case replay has **20 matches, 7 mismatches and 14
unqualified results**, with zero incomparable programs. Overall comparison
fails. Eleven comparator guards and eight inventory tests pass. All 21
recorded engine hashes, 346 CE Test files and the CE baseline executable match
preservation identities. No accepted installation was updated.
