# story.08 source investigation1135

Result: **UNKNOWN remains justified. Exhaustive reachable original-to-native
story mutation equivalence has not been demonstrated.** M4.3 remains closed
and VERIFIED under completion1134; its evidence and candidate are unchanged.

This assignment produced a reproducible source inventory, a32-row mechanism
reconciliation, per-instance and per-site local matrices, independent inventory
audits and276 bounded original-code checks. It did not establish the complete
original executable/caller/alias universe. None of the successful bounded checks
is substituted for that missing proof.

The authoritative detailed matrix is `SCUMMVM-CE-STORY08-1135-RECONCILIATION.json`.
Each row records original references, native source hashes/line anchors, known
call chain, conditions, ordering, side effects, persistence, failure behavior,
evidence and the exact unresolved requirement. Its dispositions are:

| Disposition | Rows | Meaning |
|---|---:|---|
| VERIFIED equivalent |10|The explicitly bounded operator/producer domain; not every incoming original context.|
| VERIFIED intentionally different |6|Identified CE ownership, host, dispatcher or recovery policies with existing justification.|
| UNKNOWN |16|A complete original/native state relation or context/provenance proof is missing.|
| MISSING or DEFECTIVE |0 demonstrated|Not a claim that the remaining unknown domain contains no defects.|

## Reconciliation index

| IDs | Original mechanism/context | Native owner/disposition |
|---|---|---|
| S01–S03 | Ordered dialogue conditions, assignments/bit changes; malformed tails; derived lookup alias | DialogueRules/Conversation/WorldConversation/World: bounded operations equivalent, malformed parameter guard intentionally different, alias context UNKNOWN |
| S04–S06 | Dialogue world callbacks, main/aux record ownership, item/gold/recruit/service requests | ConversationCallbacks/World canonical records; known callbacks equivalent, cache ownership intentionally different, complete handoff/failure context UNKNOWN |
| S07 |11 authored map rules, four effect kinds | StoryMapPatches/WorldStory; bounded complete rule grammar equivalent |
| S08–S10 | Keys/compound gates/controls, plates/current-cell flags, Search rewards | Native gates/WorldOpen/World/Search; bounded gates/plates equivalent, full Search failure/context relation UNKNOWN |
| S11–S13 | Counted inventory, Use/spell/navigation producers, recruitment/inn/reorder copies | Typed inventory/casting/party/inn owners; complete producer/caller/alias domains UNKNOWN |
| S14–S15 | Compact/role126 contact effects, retirement, protection and combat consequences | Actors/BattleWorldReturn/BattleAftermath/Loot; initial encodings equivalent, global dynamic/exceptional context UNKNOWN |
| S16–S20 | Keep65 reveal, Shadow approach, private checkpoint, Praevoc phase, ending/revelation | Existing native producers and ending: bounded equivalents; disk/CPU checkpoint intentionally replaced by CE saving |
| S21–S23 | Scheduling/population/movement records, route failures, clock/service temporaries | Shared native simulation owners; route recovery intentionally different; all dynamic/interleaving/lifetime contexts UNKNOWN |
| S24–S25 | Patched movement call and six-way loader menu | Known selector boundaries resolved; native typed dispatch/host deliberately replace writable call operands/loader |
| U01–U07 | Executable universe, arbitrary indirect writes, bank/overlay/call closure, generated code, special contexts, dynamic data producers, dialogue tails | Explicit UNKNOWN proof obligations; no guessed implementation or interpreter introduced |

## Inventory method and results

Only `Nox Archaist HDV (v119).HDV` is an original input. Its SHA256 remains
`c42a95c7a356548c78957f98693e92d33c9bae3ff5126d9887c2980e5a490c3c`.
Lord of Storms is neither scanned nor tested.

1. Enumerate all74 original resources and hash each payload. Examine every
   byte offset of960,173 payload bytes for documented65C02 write/RMW operations
   and control-transfer candidates. Include writes outside0400..04FF because
   actor/map/inventory/party changes can affect story progression. Indexed bases
   below0400 and arbitrary zero-page indirect pointers are not omitted.
2. Independently audit the opcode set: all62 explicit addressed store/RMW opcodes
   match the decoder; test indexed boundary/wrap cases. Supplement six implicit
   stack-writing opcodes (68 memory-writing instruction encodings in total)
   and bank/IO soft-switch reads, which can alter later write contexts. Extend the envelope to
  976,896 rounded file bytes, including16,723 EOF-padding bytes, and all1,474,560
   raw disk bytes. An original bootstrap copy extends beyond EOF-loaded data;
   this does **not** establish that all file padding was executed.
3. Index reconstructed reference writes and calls as auxiliary coordinates.
   This is deliberately not a complete-original-program claim. Preserve the
  6,686 lexical mutation candidates,52 additionally found increment/decrement/
   fill spellings and both revisions of the call index. Revision2 excludes four
   helper definitions incorrectly recognized as calls in revision1.
4. Rebind the retained authored inventories to immutable source. All439 valid
   packet hashes,3,227 headers and552 complete field effects reproduce exactly.
   Keep the two malformed parameter tails and19 unparsed resource tails/4,502
   bytes separate. Validate22 retained map/resource hash bindings.
5. Link known operators/producers to current native owners and retained tests;
   require explicit context bounds. The current map-patch implementation is
   text-identical to tested64 after line-ending normalization. Dialogue commit
   and eligibility sections match tested10. All377 frozen candidate source
   files remain byte-identical to the accepted current candidate.

The payload scan finds204,604 **candidate write encodings**, including6,871
indirect stores. Most have no certified executable boundary and many are data.
These numbers are **not actual story pathways, missing implementations, or an
implementation percentage**. The raw-image scan likewise includes filesystem,
padding and ordinary data false positives.

The local authored-instance matrix has1,195 rows:596 bounded operator/encoding
equivalents and599 unresolved instance/context candidates. This is not an
exhaustively verified set of1,195 executable events. The59 portal candidates
include the two map-zero returns excluded by the first57-portal gate unit;
491's later map-zero policies remain separate supporting evidence.

## Focused original-code findings

The copy helper at LWR.MAIN.RTNS file offset0841/PC1441 has an **inclusive**
source endpoint and copies forward sequentially. The fill helper at offset0873/
PC1473 has an **exclusive** endpoint. Sixteen actual original operations cover
byte/page boundaries and overlapping buffers:5,690 original instructions and
1,030 observed writes into0400..04FF. A generic memmove or shared length rule
cannot be substituted without caller-specific alias proof. This identifies a
real proof requirement, not a demonstrated native bug.

The two computed expressions in the reference call index were investigated:

* GAME's pointer928F is a patched movement call. Four original setters select
  exactly ED5B/EE20/EEDC/EF91; the four probes execute through the actual call.
  The full static payload envelope contains exactly the eight expected direct
  absolute patch stores. This does not rule out other indirect operand writers.
* LOADER.P A09F is a fixed six-way selector. All256 accumulator values were
  checked:1..6 choose the six literal destinations; the other250 reach the
  original14A7 failure helper. No destination body or gameplay is executed.

Those260 selector probes execute3,330 original instructions. Combined with the
copy/fill probes:276 cases and9,020 original instructions, zero Engine launches,
zero native builds, zero campaign replay and zero new native execution claims.
The original seven method commands were then reproduced once with exit0 and identical
immutable outputs. That is552 original probe executions/18,040 instructions
across both passes, still276 unique cases; reproduction adds no coverage.
The added stack/MMIO audit was reproduced separately without repeating the probes.

PC A3B2 illustrates why unbanked matching is unsound: it names a loader menu
branch in menu_extras.inc and a different inventory branch in inventory_lists.inc.
The reference index has9,105 nonunique PC values and3,668 call candidates; resolving
the two visible computed expressions does not close the original bank/overlay graph.

## Genuine discrepancies and corrections

No new native implementation defect was demonstrated, so no production change
or rebuild was justified. The investigation corrected its **inventory method**:
payload-only bounds were expanded; omitted lexical mutation spellings were
indexed; helper definitions were removed from the call-site count. Revision1
and its hash remain preserved beside revision2. The first audit script had a
syntax error before execution; it was corrected without a game/build run.
The initial62-opcode count was also corrected to explicitly addressed stores/RMW,
with six implicit stack writers and a61,232-row stack/MMIO context supplement
accounted separately. Asynchronous effects and generated code remain UNKNOWN.

Known intentional differences remain explicit: malformed header bounds,
canonical map ownership, CE save/session authority, recoverable NPC route
failure, typed movement dispatch and native host menus. They are not renamed
compatibility executors and do not establish every original context equivalent.

## Exact remaining work

* **U01:** A resource-offset/bank/overlay code/data and entry-root certificate.
  Available inputs include binaries and partial disassembly/reference programs;
  none currently certifies the complete executable universe.
* **U02:** Destination/value/end/overlap summaries for every indirect-store and
  copy/fill caller. The local index includes137 pointer-store expressions in37
  reference files and27 raw call candidates for the selected helper targets.
* **U03:** Complete bank/overlay, computed jump, stack-return and nonlocal
  continuation relations. A PC/case-label match alone cannot supply a call chain.
* **U04:** Every reachable code-writing range and generated opcode/operand value,
  closed to a fixed point or shown story-noninterfering. The documented driver
  patch changing an indirect read into a store demonstrates why immutable bytes
  alone are insufficient.
* **U05:** Context-specific write-set/noninterference or justified replacement
  relations for conditional65816/ROM/IRQ/bootstrap/inherited-memory behavior.
  Do not erase the existing raw53/254 Open evidence qualification.
* **U06:** Inductive provenance bounds for dynamic records/items/actors and all
  legitimate producer/consumer entries, including prototype/import paths.
* **U07:** For each of the19 indexed dialogue tails, prove original selector
  termination and buffer bounds for legitimate callers, or trace its actual
  selection/effect/refusal. A native parser's rejection does not prove original
  unreachability. These tails are not asserted to be additional quests/scripts.

The nine additional UNKNOWN S-rows specify concrete effect/lifetime gaps within
those domains, including the full physical-slot Search failure and the derived
lookup response. Every missing proof requirement is recorded per row in the JSON.

This is why the requested exhaustive reachable-equivalence matrix is **not yet
available**. The delivered conservative candidate matrices make the outstanding
domain inspectable; they do not turn unknown paths into VERIFIED or MISSING.

## Reproduction and preservation

From `D:\Codex Projects\Nox Archaist`, run the eight commands listed in the JSON:
inventory, envelope audit, helper probes, trace index, dispatch probes, retained
input validation, stack/MMIO audit, reconciliation. Output lives in `conversion\story08-1135`.
Source-diagnostic JSON files are immutable on reproduction: differing outputs
raise an error instead of replacing prior evidence. Existing canonical1135
reconciliation is a generated current report; historical1134 records are untouched.

Recovery copies of the eight tools live in CE `devtools/nox/project/story08-tools`.
To run a recovery copy, set `NOX_STORY08_CONVERSION` to
`D:\Codex Projects\Nox Archaist\conversion`; it reads the same hash-bound local
helpers/assets and checks the same private outputs. Tool and dependency hashes
are recorded in the JSON. All proprietary bytes, decoded data, reference listings
and per-site diagnostics stay local; only sanitized reconciliation metadata,
method source and canonical tracking are committed.

Residual risk: a source-reachable writer or context-specific ordering/failure
effect may remain unmapped. Successful M4.3 progression and absence of an opcode
executor do not eliminate that risk. M4.3 is not reopened; no release, packaging,
publication, push, tag or deployment was performed.
