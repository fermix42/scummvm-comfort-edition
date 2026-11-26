# ThinkQuick original-interpreter contracts — step 2

Tracking [CE #1](https://github.com/fermix42/scummvm-comfort-edition/issues/1).
November 26, 2026, second recovery checkpoint. **Step 2 remains in progress.**
PR #295 remains an unchanged draft step-1 checkpoint. No engine behavior has
been implemented or changed during this recovery pass.

This document distinguishes **observed** original machine-code paths from
**inferred** interpretation and **open** contracts. Observed does not mean
tested against a running original interpreter. Step-3 differential probes are
still required; the game remains blocked in CE Main initialization.

## Evidence identity and reproduction

All addresses apply exclusively to original CarmenTQ.exe SHA-256
`219d81ea3d91b31da406e6891d01ba6ad12005e77b4d9f8f83930161dbfc9966`.
The [evidence manifest](THINKQUICK-CONTRACT-EVIDENCE.json) records 130 inspected
regions with byte hashes, value-tag branch targets, numeric conversion formats,
constants, all nine component vtables/dispatchers and all 18 datafile arities.
It contains metadata, not executable bytes or full game scripts. The
[loader/component/storage supplement](THINKQUICK-LOADER-STORAGE-CONTRACTS.md)
traces the two-pass loader, nested proxies, database/selector algorithms, history,
profile-cache flushing, path selection and encryption.

Recreate it from the authoritative checkout with a new output path:

```powershell
python devtools/ce/recover_mohawk_v5_contracts.py --exe '..\validation\thinkquick-20261125\disc\95instal\CarmenTQ.exe' --output '<new-contract-evidence.json>'
```

Optional `--disassembly-root <new-local-directory>` emits local research text;
it requires Capstone, optionally found via `--module-dir <existing-python-libs>`.
Do not publish the original executable or raw asset payloads. Retained local
disassembly is under `validation/thinkquick-20261126/step2` outside the checkout.

## Values, conversion and ownership

**Observed:** a value has a tag byte at offset 0 and payload starting at +8;
evaluation-stack slots advance by 16 bytes (`0x4061f0`, `0x406500`). These are
original layout observations, not a requirement to copy its C++ representation.

| Tag | Recovered role | Evidence |
| --- | --- | --- |
| 1 | Argument binder treats this tag as unspecified/omitted | `0x404e67–0x404e92` checks exact tag and preserves the default |
| 2 | Signed 16-bit numeric value; reset/zero also uses this tag | `0x405fc0`, `0x40fef4`, `0x410f40` |
| 3 | Signed 32-bit integer | `0x405b10`, `0x40ff0b` |
| 4 | In-memory double; bytecode literal encoding is fixed point | `0x40ff1b`, `0x40198a`, `0x4022f0` |
| 7 | Owned NUL-terminated string | Copy path `0x40f205`, release `0x40ef19` |
| 12 | Item pointer | `0x410812`; numeric conversion reads item ID at +0x26 |
| 13 | Shared list object | `0x4108a0`, `0x40f26c`, `0x40ef4d` |
| 14 | Native/component object | `0x410859`, retain/release via common methods -1/-2 |
| 15 | Reference to another value | `0x40f176`, `0x4107b7`, recursive conversions |

Other tags remain in the manifest without invented names. Tag 11, for example,
has packed-field conversion/comparison paths; its complete meaning is open.

**Observed literal contract:** subtype 2 reads two bytes and constructs tag 2,
subtype 3 reads four bytes and constructs tag 3, subtype 4 reads an unsigned
big-endian 32-bit word, divides by 10,000 and constructs tag 4. A token-6 string
is resolved separately into an owned tag-7 string (`0x4063e0`, `0x4060b0`).

This corrects another prototype/first-pass assumption: subtype 2 is **not** a
literal string-table reference. The actual inventoried code has 29,340 subtype-3
and three subtype-4 literals, with no subtype-2 literal. Updating the research
decoder therefore changes no step-1 counts or recorded component selectors.
The engine's generated literal-string case does not prove original encoding.

**Observed numeric conversion:** `0x40fdb1` recognizes tags 2/3/4 and recurses
through tag 15. Strings are numeric when `%lg%n` parses a number and scanning
the remainder with `%1s` finds no further non-whitespace token. Decimal and
exponent spellings accepted by that CRT parser are not restricted to digits.
`0x40fe5d` converts recognized numeric payloads/strings to double; ordinary
unhandled tags fall through to 0.0. Item conversion uses its numeric item ID.
Integer conversion `0x40ffa9` delegates real/string conversion to CRT helper
`0x477860`; overflow, special values and rounding boundaries require probes.

**Observed comparisons:** `0x40f559` unwraps references. Two numeric values
(including numeric strings) use numeric subtraction; same-kind item/list/native
objects have pointer-based comparison paths; the fallback converts both to
strings and calls the CRT comparison routine at `0x477ca0`. Exact ordering of
packed tag-11 values and overflow/special-number cases remains open. CE's
integer-only string predicate and direct same-type string equality are not a
complete implementation of this contract.

**Observed truth operations:** logical helpers `0x405ef0`/`0x405f40` compare
double conversions against 0.0; NOT at `0x404948` does the same. CE's current
`toInt()==0` path changes fractional truthiness. For the postfix logical handler,
operands have already been evaluated on the stack; there is no retroactive
language-level short-circuit of those operand expressions. Compiler-generated
control-flow alternatives still need separate analysis.

**Observed copying and lifetime:** `0x401f60` first checks storage equality, then
initializes by source tag. Strings allocate/copy their bytes. Lists increment
the shared object's 16-bit count (`0x416e7e`); release decrements it and deletes
when nonpositive. Native objects use common addref/subref virtual calls, with
deletion when subref returns nonpositive. Reference initialization uses a
different path: tag-15 sources are recursively copied from their target;
explicit reference construction (`0x40f176`) stores a target pointer instead.
Thus “all operands are late references” and “all copies preserve references”
are both insufficient models. Alias/cycle and dangling-reference behavior
remain step-3 probe requirements.

## Names, expressions, frames and calls

**Observed name lookup:** `0x4035e5` resolves an identifier through symbol
lookup `0x414b81`; a returned tag-0 cell becomes numeric zero. The original
string entry's first word is a mutable symbol-cache index, before its name.
Lookup special-cases `self`, checks the current local symbol frame before the
cached/global cell, and otherwise creates a global entry and records its index.
The ordinary tree lookup uses case-insensitive comparison (`0x414a04`, flag 0).
Nested dynamic-scoping edge cases remain open rather than inferred from one
lookup path.

**Observed local-frame storage:** `0x414c47` increments a depth and allocates a
symbol frame while the incremented depth is below 32; `0x414d09` pops and destroys
the current frame. That boundary is an original behavior to probe, not a CE
limit to copy without checking callers and error handling.

**Observed expression entry/result:** `0x40375e` returns numeric zero for a null
code offset. If its evaluator is already active, it constructs another evaluator
using the same symbol context. Otherwise it records code/receiver context, reads
tokens, evaluates, copies the result into global result cell `0x49a540`, checks
the evaluation stack and clears active code pointers on exit. This shared result
cell requires careful copying around nested calls; its existence is not proof
that CE should introduce equivalent global storage.

| Token/path | Recovered behavior | Evidence / remaining boundary |
| --- | --- | --- |
| 0x61 | Declares local cells and evaluates their initialization/default expressions | `0x404eca–0x404f33`; loop ends on expression EOF |
| 0x62 | Evaluates a formal/default, then copies a supplied argument into that cell unless supplied tag is 1 | `0x404dda–0x404eca`; incoming iterator advances per formal; unlike CE's strict count-only model |
| 0x63 | Evaluates expressions and appends values to the current outgoing argument frame until marker 0x76 | `0x404d70–0x404dda` |
| 0x64 | Runs nested code from a numeric code offset and pushes the result | `0x404f33–0x404f7b`; literal pointer/code-owner lifetime needs qualification |
| 0x72/0x73 | Evaluates a selector, compares candidate expressions, skips unmatched groups, evaluates the chosen group, then forces expression EOF | `0x404f7b–0x405067`; exact source syntax and all nesting cases remain open |
| 0x77 | Separates a native object receiver from a string-callable name, collects argument expressions, and dispatches accordingly | `0x40498c–0x404d70`; native method selector can be numeric or string |

For 0x62, default evaluation precedes substitution; any default side effects
must therefore be considered even when an argument is supplied. Missing/surplus
argument behavior outside this handler still needs the caller/frame helpers
and controlled probes; no blanket “strict arity” rule is established.

**Observed sender/receiver separation:** named script invocation `0x4089f3`
pushes an argument frame, saves the global calling-item context at engine +0x6c,
sets it to the current receiver, invokes the named event on target(s), restores
that calling-item context, pops the frame and pushes the result. Token 0x66
reads that context. This distinguishes caller/sender from the target's `self`.
It does not yet settle donor-code ownership under ScriptLink.

**Observed targeting:** `0x40347d` accepts direct item values, numeric IDs or
string names. `0x406965` also iterates list targets in one-based order and keeps
the last dispatched result. `0x4068f0` saves/restores a current-item ID around
the target's virtual event call. Duplicate targets, reentrant deletion and empty
list/zero-target special cases are open.

**Observed indexing/property access:** `0x403d71` resolves a list or creates one
through a reference when appropriate. Numeric indexing converts through the
short-integer accessor, grows a short list, then obtains its one-based cell.
The string-key path looks up a named cell and creates it when missing. List
growth fills with default values (`0x4168fc`); out-of-range element lookup returns
null (`0x416a83`). The prototype's explicit 1..65535 bound is not established
by this original signed-short path. Read-vs-write mutation and failure cases
need runtime comparison.

## Native/component interface contracts

The nine table affiliations from step 1 are now tied to original virtual tables:

| Class | Vtable | Numeric method | Named method |
| --- | --- | --- | --- |
| aaDatabase | 0x488bb8 | 0x43b06c | 0x43b032 |
| aaDirectory | 0x488bc4 | 0x43feca | 0x43fe90 |
| aaQSelector | 0x488bd4 | 0x4406f1 | 0x4435b0 |
| aaUserHistory | 0x488bf8 | 0x4455bd | 0x4462a0 |
| aaStringLib | 0x488bec | 0x444eb3 | 0x444e79 |
| datafile | 0x488c04 | 0x44652b | 0x4464f1 |
| encryptor | 0x488c10 | 0x447e73 | 0x448600 |
| font | 0x488c1c | 0x448897 | 0x44885d |
| clock | 0x488c34 | 0x449523 | 0x4494e9 |

The extractor derives these through each named-method table's unique code xref
and its vtable pointer. It rejects ambiguous matches rather than selecting one.
It also confirms each affiliation against the constructor's descriptive name
(`lbxAADatabase`, `lbxAAUserHistory`, etc.) immediately after its vtable store;
those constructor/name addresses are retained in the manifest.

**Observed common dispatch:** native objects have destructor/numeric/named
virtual entries at +0/+4/+8. Named dispatch `0x448d61` compares names without
case sensitivity, invokes the class's numeric entry for a match, then tries
common method names. The special name `methods` enumerates the class table.
An unknown name returns an integer-zero value on the observed fallback path.
Common numeric methods -1/-2 adjust the object's refcount, -3 reports it, -4
returns ID, -5 name, -6 methods, and -7 the error field (`0x448f81`).

**Observed LBXFunc contract:** general command 0x58 builds an argument list
excluding receiver and selector; selector classification chooses numeric virtual
+4 or string virtual +8 (`0x40b417–0x40b6ae`). It pushes the returned value object.
This is materially different from a universal adapter API that conditionally
pushes no value based on a C++ boolean.

### Datafile

`0x44652b` validates argument count through table `0x446a33`, excluding the
receiver and method selector. Invalid count returns signed-short -1. Normal
dispatch returns a value object; operations with only status side effects can
leave that value at numeric zero. Internal helper status words must not be
silently reinterpreted as boolean return values.

| Arguments | Methods |
| ---: | --- |
| 0 | close(2), buildList(4), getCurSection(6), loadVars(8), deleteCurSection(10), writeThruOn(12), writeThruOff(13), loadSection(16), deleteFile(17) |
| 1 | open(1), addSection(3), setCurSection(5), deleteVar(9), deleteSection(11), isASection(14), getVar(15), openReadOnly(18) |
| 2 | setVar(7) |

The operation-handler address for every method is retained in the evidence JSON.
The first argument is converted to a string where needed; the second setVar
argument retains its value representation. `loadSection` creates a list result.
Both open paths close/reset prior state and resolve through original file/path
helpers. The read-only path has an interactive error/retry path, not merely an
alternate INI parser flag. `deleteFile` calls close then a file-delete helper;
write-through toggles route to `0x447baa`, which performs additional file work.
Exact file formats, caching/flush boundaries, error fields, path roots and
original UI behavior remain open. CE's permanent portable-storage ownership
must remain intact when those contracts are eventually implemented.

### Other recovered entry contracts

- **aaStringLib:** methods 1/2 require one string-convertible argument; method 3
  requires two. Wrong counts return integer zero. Successful case transforms
  return strings; comparison routes through original case-insensitive comparison.
  Encoding/locale behavior is not yet qualified (`0x444eb3–0x4452a0`).
- **aaDirectory:** SetPath requires one argument or returns zero. GetFiles accepts
  a supplied string when count is one, otherwise takes the original default
  buffer; it returns a list. SelectFolder returns a string through its helper.
  Enumeration/filter/order/UI contracts remain open (`0x43feca–0x440298`).
- **clock:** positive method IDs call a tm-shaped calendar helper and return an
  integer. Month adds 1, year adds 1900, weekday and year-day add 1; seconds,
  minutes, hours, day and DST use their record fields directly. Unknown positive
  IDs return -1. Mapping to CRT time/localtime is an inference from signatures
  and record layout; timezone/DST requires probes (`0x449523`, `0x449583`).
- **font:** setfont(1) checks exactly one argument, converts to string and returns
  its font-helper integer result; getfont(2) returns its stored/helper integer.
  Wrong setfont count or unknown positive ID returns zero. The original helper's
  integer must not yet be labeled a ScummVM font or OS handle (`0x448897`).
- **aaDatabase, aaQSelector, aaUserHistory, encryptor:** numeric and named entry
  functions are located and hashed, but their deeper algorithms, defaults,
  argument/type failures and persistence formats are not fully recovered.
  They remain open work, not implementations inferred from method names.

## Lifecycle recovery and implementation gate

**Observed ScriptLink mutation:** the original general-command path defaults
mode to 1 and emits a big-endian field-0x192 payload (`0x40c9aa–0x40cad6`). The
item field handler `0x419d08–0x419eb2` establishes these normal-path rules:

- A zero-length field clears the link list. This is a payload-length condition,
  not a universal rule that every nonempty ID-zero field means clear-all.
- A nonempty field resolves the donor item and searches existing links by its
  pointer. Mode 0 removes that matching link. A nonzero mode adds a new link at
  the head or updates an existing link in place.
- The cascade flag is true exactly when mode equals 2. This handler has no
  explicit rejection of other nonzero modes; CE's prototype currently rejects
  modes above 2. Missing-donor and deletion behavior still require recovery.

**Observed dispatch/identity/order:** `0x41e14e` enters virtual script dispatch
with receiver=self, code owner=self, and linked traversal enabled. At
`0x41e174`, the receiver's context pointer +8 is saved and replaced with the
code-owner pointer. Entries are read from the code owner's list at +0xd4, while
execution stays on the original receiver. Named-event matching is
case-insensitive. Matching entries set a handled flag before condition checks;
that flag is not a command return value or proof a condition passed.

Own entries finish before links are considered. If traversal is enabled, the
code owner's link list at +0xdc is walked head-first; recursive virtual calls
retain the same receiver, pass the donor as code owner and use the stored
cascade flag. Handled flags are ORed. On normal return, the receiver's previous
context pointer is restored. Thus new links run newest-first, existing-link
updates retain order, and receiver identity differs from donor code context.
The prototype's ID re-resolution, snapshot-of-links traversal and recursion
limit are policies that still need comparison with original mutation/lifetime
behavior, not contracts established by these paths.

**Observed initial-code dispatch:** field 0x194 at `0x41a04c` endian-converts
its offset and directly calls `0x41d38d`; that helper calls page evaluation
`0x41fdf2`, which directly calls evaluator `0x40375e`. There is no queue in this
handler chain. The loader caller is now recovered: it first constructs and
registers every item, stores their field pointers, then applies each item's
fields and dispatches that item's listload in authored order. Thus later items
exist before earlier initial code, but their fields may not yet be applied.
See the supplement for the phase passes and synchronous nested proxy loading.
Implementation still awaits the planned original-interpreter probes.

The supplied Startup SYM establishes authored listload ordering: immediate fade,
detector/path events, proxy filename assignment, then Main proxy loading. That
is data evidence, not proof of the interpreter's global initialization order.
Keep the prototype's deferred-init and donor-code behavior provisional.

| Step-2 area | This checkpoint | Still required before claiming recovery complete |
| --- | --- | --- |
| Values/conversion/reference model | Concrete tags, ownership, numeric strings, comparison and truth paths | Packed tags, conversion overflow/rounding, alias cycles and all operator errors |
| Frames/calls/returns | Default-before-substitution, argument frame paths, sender restoration, reentrant evaluator | Complete missing/surplus/default rules and failure/unwind behavior |
| Lifecycle/linking | Two-pass creation/field/listload order, phase passes, nested proxy loading, link cascade/receiver rules | Mutation/deletion, reentrancy failure and code-owner lifetime |
| Components/storage | Nine dispatchers, datafile arities, index/selector/history paths, profile formats/flushes, default paths, encryptor recurrence | Index validation, exact reorder boundaries, shutdown flush reachability, negative/error/UI probes |
| Evidence/comparison | Pinned executable, repeatable hashes and branch-table extraction | Step-3 controlled original runs; no differential acceptance exists |

Implementation remains gated. Priority probes for step 3 are fractional truth
and numeric-string comparison; signed-short/fixed-point literals; copy-vs-alias
behavior; default side effects and omitted arguments; nested self/sender return
restoration; unknown native names/wrong arity; datafile read-only/write-through/
delete and cold restart; and ScriptLink donor/receiver/lifecycle ordering. The
loader/component/storage supplement gives concrete normal-path contracts and
negative probe targets. Remaining edge paths stay explicit step-2 work; no
engine implementation or original-runtime acceptance is implied.
