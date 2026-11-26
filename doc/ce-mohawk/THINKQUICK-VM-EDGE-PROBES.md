# List, reference, key and masked floating-point comparisons

33 new identified pairs produce **12 matches,16 mismatches,5 unqualified**.
All33 original cases qualify twice with exact core-program/source identities.
The unchanged CE baseline retains its actual unsupported operations and errors.
This is research evidence before Step4, not an implementation change.

## Original context and negative controls

The selected CRT/symbol/frame/VM initialization is unchanged. An empty caller-owned
page lookup shell is supplied at global0x49a678: numeric non-list lookup must
return no item through the actual original lookup, rather than dereference an
absent page. Earlier two wrong-context faults at0x41fb06 are diagnostics and
excluded. No item lookup or language-return function is replaced.

- Unbound indexed variables grow lists; an assignable scalar is replaced with a
  list when no item resolves. A literal non-list instead raises caught VM error15.
- Numeric index65537 wraps through signed-short conversion to1. Reads at0/-1
  return zero without a VM error in these bounded cases. Assignment at0 raises
  caught reference-required error14. Safe writes to every invalid index are not
  inferred from a zero-valued read.
- Nested list construction copies the indexed value: after source42 changes to99,
  the captured nested entry remains42; the99 predicate is false. Literal/growth
  and nested-copy controls match CE. Scalar replacement, index wrapping and invalid
  reads remain unqualified on CE's exact indexed-value/list-index diagnostics.
- Integer prefix/postfix increment/decrement and mutation controls match. Fractional
  unary changes1.5 to2.5; fractional +=0.5 produces2, and *=2 produces3 in the
  original. Numeric strings plus ordinary arithmetic construct fractions, avoiding
  the known real-literal format mismatch. CE's integer-only unary/+= paths differ;
  *= is unsupported. Compound division byzero raises original VM error4; CE aborts
  on unsupported assignment19. Wrong-reference unary and zero-index assignment
  likewise compare original typed recovery with precise CE aborts.

## Key and nonfinite behavior

Original key construction from character a or virtual65 converts numerically to
97/65. Modifier2 preserves the measured numeric character. CE's key command pushes
zero; numeric and distinct-character predicates differ. The equal-key predicate
matches but is interpreted alongside the false control, which CE incorrectly
makes true. Prior31 direct original conversion/key-trigger cases retain separate
raw-Boolean/key payload and trigger-versus-comparison boundaries; they are not
paired event-input acceptance claims.

The nonfinite producer parses1e309 and subtracts infinity from itself. Every
original case records x87 control0x037f after evaluation; masked-NaN cases also
require the invalid-operation status flag. With that measured state, original
NaN versuszero predicates return equal1,not-equal0,less0,less-equal0,greater1,
greater-equal1. CE differs on equal/not-equal/greater/greater-equal. These are
observations of actual x87 predicate paths, not a claimed IEEE ordering rule.
A NaN divisor takes original guarded division VM error4; CE returns without that
VM error. Unmasked floating exceptions and arbitrary alternate CRT settings are
not claimed.

Typed-error comparisons retain completion and actual VM-error identities, not
an invented zero success result. Ordinary assertion predicates require no VM
errors. CE invalid-list/unsupported-multiply outcomes stay unqualified as value
comparisons. Every ordinary original result restores the checked stack/frame/
active-code state; the typed catch remains distinct from transaction rollback.

Reproduce mohawk_v5_vm_edges.py original --original <pinned EXE> --root <fresh
owned root>, then mode ce with --ce <baseline> --extras <engine-data> --game-data
<owned Outline.txt data> and another fresh --root. Use the existing comparator
on both receipts; mismatch/unqualified cohorts intentionally fail comparison.
Original FP words, complete results/errors, CE diagnostics and repeated core
hashes are in the JSON companion. Sources are frozen before each run.

Cumulative separately identified evidence: prior60 + disk2 + values30 + edges33
= **125 pairs:48 matches,34 mismatches,43 unqualified**. This is not a fresh
monolithic125-case replay; aggregate reproduction remains required. The bounded
VM/value/key/geometry/operator queue now has representative normal/negative
observations and explicit CE boundaries. Lifecycle/queued mutation/hook reentry,
database/selector/applicable CE storage and final aggregate audit remain.
Stop before Step4; keep PR295 an unchanged draft. No engine/build/install change.
