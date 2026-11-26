# Packed geometry and operator comparison extension

30 identified original/CE cases add to the earlier cohorts. All 30 original
observations qualify twice: 23 ordinary assertion returns, four typed unsupported
command returns and three captured processor faults. Core expression/program
hashes agree across original and CE. The exact-source results are **11 matches,
8 mismatches, 11 unqualified; overall fail**, not a compatibility milestone.

## Observed boundaries

- Point construction, coordinate extraction, ordinary numeric/space/short-overflow
  controls match. The original rejects incomplete `12,` and trailing `12,-7 junk`
  point strings to zero geometry; CE's corresponding predicates are false.
- Original getRect(two points) normalizes opposite corners, while its four-number
  form preserves supplied shorts. A rectangle converted to point copies its first
  pair. These command observations are separate from the direct value conversion
  contract; do not infer that every rectangle conversion normalizes corners.
- Original general commands 13 through16 (top/left/bottom/right names in CE) hit
  the actual unsupported-command handler and typed VM error2, then return through
  the evaluator catch. CE executes its named implementations without that error.
  Four error-completion mismatches record the difference. The receipt's CE value
  field is the assertion/truth predicate, not a raw scalar coordinate read; the
  error-presence difference is the relevant comparison.
- Positive/negative integer division and remainder predicates return true in the
  original. CE reports unsupported binary operation32/34 and remains unqualified
  as an ordinary value comparison. Subtraction and a signed-boundary false
  predicate match; no overflow-clamping policy is inferred from one predicate.
- Original integer division by0.5 faults with 0xc0000094 at0x404316; remainder by
  zero faults with that code at0x404354; INT_MIN/-1 faults with 0xc0000095 at
  0x404316. Actual fault-thread EIP must equal the captured address. These are
  processor outcomes, not VM error4 and not normal evaluator/frame restoration.
  CE's zero-remainder/overflow paths produce exact unsupported-operation aborts,
  yielding two completion mismatches. Fractional division remains unqualified
  on CE: its documented real-literal parser mismatch instead reaches unsupported
  bytecode token03 at24. It is not forced into the expected operation diagnostic.
- Six reversed-corner rectangle assertions fail to receive CE inspection within
  the explicit eight-second native-command bound. Their owned processes are
  terminated and traces retained; these are unqualified transport observations,
  not false predicates or proven geometry values. Malformed-zero and ordinary
  coordinate-copy controls complete. No engine fix or longer-wait success is
  invented. An earlier diagnostic batch lacked the short command bound and was
  stopped after verifying its exact runner/child ownership; it is not final data.

## Scope and reproduction

mohawk_v5_value_probes.py original --original <pinned EXE> --root <new owned root>
runs selected CRT/symbol/frame/VM initialization and generated code before normal
application entry. Mode ce uses --ce <baseline EXE> --extras <engine-data>
--game-data <owned Outline.txt data> and a fresh --root. The CE driver uses the
existing native CLI, with bounded commands and process abort on nonresponse.
No desktop input or engine modifications are used. Both sides execute the same
core generated program; CE's surrounding page/assertion sentinel is explicit.
A nonzero CE driver exit preserves incomplete qualification rather than hiding
unsupported operations or timeouts. Run compare_mohawk_v5_observations.py on
both receipts; mismatch/unqualified cohorts intentionally return a failing status.

The JSON companion retains actual receipts, repeated original values/errors,
source/executable/core-program hashes, original fault registers and CE diagnostics.
Cumulative separately identified evidence is now **92 paired cases: 36 matches,
18 mismatches, 38 unqualified**. This is prior60 + disk2 + this30, not a fresh
monolithic92-case replay; aggregate reproduction remains required.

Unqualified earlier rectangle experiments without original item context are
retained locally and excluded. They are not original-game defects. Direct key/
Boolean helpers from the prior original-only batch remain separately identified.
Remaining list/reference/operator mask and lifecycle/OS-error/storage graph cases
are still tracked in THINKQUICK-STEP3-GATE.md. Step3 is active; stop before Step4.
PR295 remains a draft; engine, executable and owner installations stay unchanged.
