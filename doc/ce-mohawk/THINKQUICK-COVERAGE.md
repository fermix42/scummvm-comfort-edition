# ThinkQuick step 1 — baseline and completed static inventory

Tracking: [CE #1](https://github.com/fermix42/scummvm-comfort-edition/issues/1).
Updated November 26, 2026. Implementation remains gated on steps 2–3.

Step 1 is complete as a **static requirements inventory with explicit unknowns**.
All declared BCOD bytes and all collected expression roots are accounted for.
This does not establish runtime reachability, complete method contracts, original
interpreter equivalence, or playability. ThinkQuick still fails in Main init.

Detailed, reproducible tables: [opcode/command/component tables](THINKQUICK-INVENTORY.md)
and [machine-readable inventory](THINKQUICK-INVENTORY.json). The latter records
all constructor, numeric-method and dynamic-invocation offsets without game code.

## Baseline and data identity

The authoritative consolidated checkout resumed at
`14a6b0b45f4b108a7b1bca347790be79589a484a`, with prototype
`ec4cc8c716cb219d14c4405e6b5e38c3c808023a` integrated locally. Textual conflicts
were resolved while retaining Cat v3 diagnostics, key registration, numeric
conversions and motion overrides. V5's provisional draw-order path remains
separate from the newer v3 path. No VM behavior was added during this inventory.
The unrelated existing release-notes edit was preserved.

The fresh baseline executable SHA-256 is
`04c96176beb3f4a8a5e53f0be20c971aab603d625057c09c9a548cb2d89a9795`.
The pre-commit engine/runner patch SHA-256 is
`5f59c79a9fffa631c34ef173b2dbccf38e6c7d790912332601847455ae14a39b`.
Local `validation/thinkquick-20261126/baseline-identity.json` links the source,
executable, tools, data manifests and test receipts. The executable and replay
instructions are preserved there. [The baseline metadata](THINKQUICK-BASELINE.json)
publishes the exact per-source-file hashes and CE command-table snapshot.
The inventory publication branch starts from the already-pushed prototype and
carries tools, reports and the regression runner's path fix; it does not publish
unrelated newer engine changes. Its runtime tree is not the integrated executable
tested above. The patch and per-source-file hashes identify that local binary.

The retained extracted Windows installation is
`validation/thinkquick-20261125/disc/95instal`, outside the checkout. It is not a
freshly verified retail installation. Its file identities include:

| File | SHA-256 |
| --- | --- |
| CarmenTQ.exe | 219d81ea3d91b31da406e6891d01ba6ad12005e77b4d9f8f83930161dbfc9966 |
| Outline.txt | 81d0f2965214980886b7380bef75075870b078cb1026f472c21a94797accfa58 |
| ACME Support/Cache/Startup.MHK | 6e40734a5587b6a48c24ec846e11dc75a4da5d72786bd3bba1b04226cba35745 |
| ACME Support/Cache/Main.MHK | 3381510607bfd7c4677b97d809ccca868c0c872f0def3e1d93ac70f4ea676c4f |

Keep Outline.txt, System.MHK and supplied FON files at the installation root;
retain `ACME Support/Cache` and `ACME Support/Product`. The checkpoint copied
the disc's Product directory into that layout. Startup's page base is 900,
Main's is 1000; bases for every archive remain in the local manifest. Custom
Questions is inventoried separately and is not assumed necessary for main-game
playability. All 1,155 files have path, size and SHA-256 in the local manifest.

## Inventory result and correction of the initial scan

- 1,115 MHK archives, 5,670 items, 1,113 BCOD resources.
- **848,560 / 848,560 declared code bytes decoded**, with zero decode errors
  and zero collected expression roots inside operands.
- **57 token values**, including separate operator/control tokens and reserved
  zero bytes. This is a lexical inventory, not 57 independent VM operations.
- **86 command namespace/ID pairs**: 50 general, 14 item, 1 notify, 13 collection
  and 8 rectangle IDs. Of the 64 general/item IDs, 33 have no handler, one is an
  explicit stub, and 30 have handlers whose v5 semantics remain unqualified.
  All 22 IDs in the other three namespaces lack v5 evaluator dispatch.
- **130 constructors**, **253 numeric component-method calls**, and **802 dynamic
  invocations**. Constructor arguments identify nine canonical component families.
  Dynamic calls comprise 623 literal script-callable names, 178 reference-valued
  receivers and one receiver produced by a prior invocation; they are not all
  external-component calls. Every site and its classification is retained.
- 56 event IDs, 51 BITL script-operation IDs and 10 item-field IDs are counted,
  including 6,295 initial-code fields. Lifecycle presence is not lifecycle order.

The original token reader at `0x4018ab` supplies operand sizes independently of
the CE evaluator. Its tables are `0x401a62/0x401a76`; the execution dispatch at
`0x403ac0` uses `0x405137/0x4051b3`. The extractor rejects any executable with a
different SHA-256. It records all token handlers and general-command handlers.
Unknown semantics are retained by address, without guessed operand lengths.

**Newly confirmed mismatch:** literal subtype 4 is an unsigned four-byte
big-endian fixed-point value divided by 10,000 (`0x4022f0`, divisor at `0x4885e0`).
The prototype instead reads eight-byte doubles. Correcting the inventory reader
removed the apparent stray 0x0d/0xac tokens and the ZapMissle offset-860 alignment
failure. The earlier 18 generated tests use the prototype's encoding, so their
real-number case does not validate the original format. Engine code is unchanged.

The original initial scan stopped at nine token types and excluded a whole
question archive. These are superseded findings, not outstanding inventory gaps:
`CRIM114_6.MHK` has three truncated sound resources, `tWAV:29002–29004`.
The first is file-table index 12, offset 6021, claimed length 6, archive size 6022.
The completed scan reports all three resource exceptions, skips only unavailable bytes and inventories the archive's valid
BCOD/BITL. It does not repair assets or change engine resource handling.

## Component requirements and initial interface specification

The original factory `0x448ad6` accepts numeric IDs and case-insensitive registered
names through table `0x4985a0`. It registers 13 classes; only these nine families
appear in constructors. Aliases are normalized using that original registry.

| Required class | ID | Constructor sites | Required interface/storage area | CE status |
| --- | ---: | ---: | --- | --- |
| datafile | 1001 | 28 | Config/mission/user sections, read-only open, variable read/write, write-through and delete | Partial numeric adapter; named dispatch and many methods missing |
| clock | 1003 | 4 | Date/time fields used by game and custom editor | Missing |
| font | 1004 | 81 | Set/get supplied font objects and text references | Provisional FON adapter only |
| aaDatabase | 1005 | 2 | Indexes, question counts, record fields, decks/QUnits, paths | Missing |
| aaDirectory | 1006 | 6 | SetPath/GetFiles; directory selection interface exists in original | Missing |
| aaQSelector | 1008 | 1 | Locations/decks/chunks, difficulty, users, filtering and mission selection | Missing |
| aaStringLib | 1010 | 4 | Case conversion and case-insensitive comparison | Missing |
| aaUserHistory | 1011 | 2 | User name/level, deck load/save and question-unit history | Missing |
| encryptor | 1012 | 2 | Set key, encrypt/decrypt and common error query | Missing |

All numeric calls have literal method IDs; the observed set is
`1,2,3,4,5,7,8,10,12,13,14,15,16,18`. Symbolic inspection retains the receiver
expression rather than assigning an inferred runtime object type. Named
component method strings and script-callable names remain separate in the JSON.
Full original name/ID tables for these families are published alongside the
inventory, including uncalled methods; table affiliation is the initial map,
not a completed argument/return/storage contract. The original common interface
also includes addref/subref/refcount/id/name/methods/error.

Dynamic receiver binding, call-frame aliasing, method side effects, failure
results and filesystem/configuration contracts belong to steps 2–3. For example,
Main offset 16259 invokes getDeck on the result of the preceding invocation;
the site is inventoried but its runtime class must be traced. No literal
constructor selector or numeric method ID remains undecoded.

## Refined coverage matrix

Evidence keys: **D** = static game data; **X** = original executable inspection;
**G** = generated CE regression; **R** = real CE startup; **O** = original runtime
comparison. O is absent for every v5 row. Flow assignments marked inferred must
be confirmed by traces. Main/shared modules contain helpers for several flows.

| Requirement | Flow/asset evidence | Baseline CE coverage | Remaining qualification |
| --- | --- | --- | --- |
| Archive-local bases and proxy filenames | Startup, Main; D/R | Startup setup reaches Main | Multiple-base policy, palette and proxy lifetime; X/O |
| Literals/conversions/references | All asset groups; D/X/G | Selected generated paths only; real literal format is wrong | Four-byte reals, coercion, reference/list alias lifetime; X/O |
| Numeric unary and compound operators | Main and round helpers; D/X | Negation 0x67 blocks Main; integer divide/modulo/compound gaps | All listed operator variants, failure/conversion rules; X/O |
| Frames, named arguments, callable invocation | Main/shared/menu/round; D/X/G | 61/62 and selected named events; 63/77 unimplemented | Defaults, nested calls, callable-vs-object dispatch and return values; X/O |
| Structured control and namespaces | Shared and round code; D/X | 72/73 and notify/collection/rectangle v5 dispatch missing | Recover selector/control contracts; X/O |
| Event/link/load lifecycle | 56 event IDs; Startup/Main; D/G/R | Deferred init and forward-link fixture only | Ordering, cascades, receiver-vs-code ownership and reentrancy; X/O |
| ACME native items | Types 0x0fa0/0x0fa5, Startup/Main; D/R | Generic fallback for motion type; provisional detector paths | Original item interfaces and lifecycle; X/O |
| Static/edit text and focus | 210 static-text and 49 edit-text items; D | Falls through generic item; SetText/font does not implement editing | Render, selection, key focus, input, font metrics; X/O |
| Movies/palettes/animation/sound | Item counts and media resources; D | Existing older-version classes; v5 unqualified | Timing, order, clipping, audio/media and transitions; O |
| datafile/directory services | Main config and user helpers; D/X; menu/round/persistence inferred | Partial datafile; directory missing | Read-only opens, deletes, writes, enumeration, portable ownership; X/O |
| Database/question selector | Main and question assets; D/X; round flow inferred | Missing | Question selection, filtering, scoring inputs, error handling; X/O |
| User history/encryption/clock/string services | Main/editor helpers; D/X; persistence inferred | Missing | Cold restart, stored history, deterministic comparisons and failures; X/O |
| Menu user flow | Main/shared templates; D | Not reached | Original menu/focus trace and normal CE navigation; O |
| Question-round user flow | Question content and shared templates; D | Not reached | Actual right/wrong answers, scoring, transitions; O |
| Persistence user flow | mission/config/user helper interfaces; D/X | Not reached | Original I/O contract and CE portable cold-restart comparison; O |

The generated asset-group partition is in the tables: startup 1,244 code bytes;
Main core 43,376; question content 404,200; other runtime 379,252; custom editor
20,488. This provides bounded static scope without claiming those paths execute.
The custom editor can be scoped separately during support qualification.

## Initial behavior specification and next phase

The VM is postfix and token decoding is independent of execution. Original
reader observations establish word operands for identifiers, strings/constants
and five command namespaces; literals use a subtype byte plus 16-bit integer,
32-bit integer or four-byte fixed-point payload. Step-2 recovery corrects the
first-pass subtype-2 label: it is a signed 16-bit integer, not a string-word.
No inventoried BCOD uses subtype 2, so counts and call-site metadata are unchanged.
See [the recovered contracts](THINKQUICK-INTERPRETER-CONTRACTS.md) and
[page-loader/component/storage recovery](THINKQUICK-LOADER-STORAGE-CONTRACTS.md).
The second step-2 checkpoint traces two-pass registration/fields/listload, nested
proxy loading, database index and selector pools, history serialization, profile
write-through/flush paths, default directories and encryption. These are static
original-code contracts; runtime reachability and compatibility remain unqualified.
Execution uses separate token
and command dispatch tables. Original 0x67 calls numeric negation; 0x77 separates
string-callable and object-method dispatch. Precise value/reference/frame/control
semantics are deliberately not inferred from successful decoding.

Step 2 is recovering those contracts. Normal loader and component/storage paths
now have evidence; lifetime, index validation, exact reorder and failure paths
remain open in the supplement.
Step 3 needs isolated original-interpreter probes before semantic acceptance.
No coherent subsystem implementation starts on this inventory alone.

The step-2 recovery supplement refines the evidence side of the matrix without
changing the provisional CE coverage above:

| Requirement | Original contract now recovered statically | Still unqualified |
| --- | --- | --- |
| Page creation and initial code | Register all items, then per-item fields/listload in authored order, then phase passes | Mutation/deletion, error/reentrant traces |
| Proxy loading | Filename assignment precedes phase load; nested loader is synchronous and uses the same passes | Full normal Startup/Main trace and failure behavior |
| Database and selector | Source/index block layout, record/deck/unit handles, pool loading, history filtering, destructive selection and mission entries | Index validation, exact reorder boundaries, empty-pool/error probes |
| History and datafile | Per-user .dat and LevelN/deck keys; script-value serialization and reload; shared profile cache | Malformed/truncated values and cold restart |
| Storage ownership and flushing | Original default directory and process-directory side effects; write-through default and dirty flush paths | Shutdown reachability and negative paths; CE portable mapping remains unimplemented |
| Encryption | Key derivation, 32-round recurrence, textual result, parse/key errors; isValid aliases decrypt | Original-runtime vectors, overflow/zero/error comparisons |

The revised scope is at least six substantial workstreams: VM/value/frames;
event/link/lifecycle/control; database/question selection; portable component
persistence/history/encryption; presentation/input/media; and the original
comparison harness plus user-flow qualification. Missing 33 general/item
handlers and 22 additional namespace IDs are inventory facts, not an estimate
of 55 independent fixes. A credible duration still depends on recovered method
contracts; no completion percentage or unsupported time estimate is assigned.

## Validation and reproduction

Earlier in this November 26 baseline: clean Release x64 build passed with zero
warnings/errors, 18 generated VM cases passed, five strict-storage checks passed,
and focused Cat v3 dialogue reached page 4 with all six clips advancing source
samples. Real ThinkQuick startup reproduced `Unsupported V5 bytecode token 67 at 22`.
These are the same preserved executable's receipts, not a rebuild during this
inventory pass. No title/menu/round/original-runtime qualification is claimed.

This inventory pass adds seven asset-free boundary tests covering the original
real format, truncated operands, indexed component arguments, dynamic methods,
partial archive recovery, command-table holes and wrong-executable rejection.
Step 2 adds an eighth boundary test for signed-short literal classification.
Run `python devtools/ce/test_mohawk_v5_inventory.py`.

From the consolidated checkout, use new output filenames:

```powershell
python devtools/ce/inspect_mohawk_v5_reference.py --exe '..\validation\thinkquick-20261125\disc\95instal\CarmenTQ.exe' --output '<new-reference.json>'
python devtools/ce/inventory_mohawk_v5.py --game-data '..\validation\thinkquick-20261125\disc\95instal' --reference '<new-reference.json>' --ce-baseline 'doc/ce-mohawk/THINKQUICK-BASELINE.json' --output '<new-inventory.json>'
python devtools/ce/summarize_mohawk_v5_inventory.py --input '<new-inventory.json>' --json '<new-summary.json>' --markdown '<new-tables.md>'
```

The shareable summaries omit binary payloads, full scripts and arbitrary method
argument strings. Full file hashes, historical scans and validation receipts
remain local. Earlier failed relative-path and unavailable-network launch
attempts are retained, with successful isolated retries identified separately.
All 346 CE Test files matched the baseline preservation check; no installation,
release, save/profile or game-data refresh is part of this inventory work.

## Edge/lifecycle recovery checkpoint — November 26, 2026

See [edge and lifecycle contracts](THINKQUICK-EDGE-LIFECYCLE-CONTRACTS.md).
The preceding second-checkpoint unknowns are superseded as follows; prototype
implementation classifications and gameplay qualification remain unchanged.

| Area | Static evidence added | Remaining qualification |
| --- | --- | --- |
| Destruction/lifetime | Queued ID-targeted destroy; value-cell invalidation to name string; raw donor links; item-before-code unload; queue teardown | Controlled mutation, reentrancy and dangling-donor probes |
| Conversion/error | x87 truncation and low word; self-alias reset; typed evaluator catch/cursor reset; nontransactional frame failure | Runtime overflow, cycles, result/context after errors; remaining operator errors/packed consumers |
| Index failure | Exact-read truncation status; absent magic/source checks; retained handle/partial state on failure | Corruption/retry probes; no safe malformed-count guarantee |
| Selector edges | Exact wrapping reorder thresholds; first shared deck wins; duplicate location/user behavior | Threshold/duplicate/empty-pool probes |
| Storage shutdown | Registered LIFO cache teardown without flush; shared-cache reopen visibility | Cold restart, read-only/missing-directory/UI outcomes |

The tracked lifecycle edge investigation is documented. Step 2 remains open for
broader operator/packed-value and interactive-error semantics, with controlled
original-runtime comparison still in step 3. No implementation starts here.

## Final static recovery checkpoint

The [operator/packed-value/OS-error supplement](THINKQUICK-OPERATOR-PACKED-OS-CONTRACTS.md)
resolves the three remaining static gaps from the lifecycle checkpoint: guarded
VM errors versus unguarded processor faults; point/rectangle/Boolean/key payload
consumers; and OS error mapping, callback installation/suppression and Retry/Cancel.
Earlier statements that these are open are historical checkpoint status.
Step 2's structural/static recovery baseline is complete. Runtime equivalence,
UI reachability, environment-dependent fault outcomes and gameplay remain step-3
and later acceptance work. Implementation and installation are unchanged.

| Final step-2 area | Static recovery result | Step-3 acceptance work |
| --- | --- | --- |
| Operator errors | Secondary opcode table, reference errors, division zero check, unguarded IDIV and x87 predicate distinctions | Controlled negative results and fault environment |
| Packed values | Point/rectangle/Boolean/key roles, conversion maps, key comparison and event-trigger differences | Parsing boundaries, key/modifier input traces |
| Interactive OS errors | Raw error mapping, installed callback, suppression/default policy, modal Retry/Cancel and exact-I/O failures | Controlled dialogs, cancellation, retries and durability |

These rows close the remaining static gaps, without promoting any prototype
behavior to original-runtime verified coverage. Step 2 is complete at the
planned structure/contract baseline; step 3 is the next gate.

## Step-3 original-code probe checkpoint — November 26, 2026

[Harness scope and results](THINKQUICK-COMPARISON.md) and
[machine-readable observations](THINKQUICK-ORIGINAL-PROBES.json) add the first
original-code runtime evidence: 31 selected conversion/key-function cases,
repeated in fresh loader-only processes. Batch-01's Boolean disagreement is
retained and corrected in the static contract. No original-versus-CE, initialized
VM, lifecycle, persistence/UI or gameplay acceptance is promoted by this result.
The existing CE implementation coverage remains provisional. Step 3 continues.

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


## Native resource/destructor checkpoint — November 26, 2026

[Native resource probes](THINKQUICK-NATIVE-RESOURCE-PROBES.md) and
[observations](THINKQUICK-NATIVE-RESOURCE-OBSERVATIONS.json): seven new
original-only cases exercise real resident-cache retain/final release, actual
deleting destructors and alias invalidation, unrepaired incoming links, real
event-7 resource unload, and unbridged selection between two separately cached
BCOD resources with positive/negative/routing controls. Maps/FILE shells are
seeded resident fixtures; disk opening/population remains unqualified. These
seven are outside paired totals and have no claimed CE equivalents.
No engine implementation; step 3 continues.

Fresh existing paired replay remains **23 matches, 10 mismatches, 27 unqualified**
across 60 cases. Seven native original-only observations are excluded from these
totals. Cold disk-cache and matching CE lifetime observations remain open.
