# ThinkQuick page loader, components and storage — step 2

November 26, 2026, second recovery checkpoint for [CE #1](https://github.com/fermix42/scummvm-comfort-edition/issues/1).
Read with [the VM contracts](THINKQUICK-INTERPRETER-CONTRACTS.md) and
[the evidence manifest](THINKQUICK-CONTRACT-EVIDENCE.json). All addresses are
from the same pinned original executable identified there. These are static
observations, not original-runtime results. No engine behavior is changed.
PR #295 stays an unchanged draft. Step 2 is still open at the boundaries below.

## Resource creation, registration and initialization order

The original loader is two-pass, not immediate field evaluation inside each
item constructor. The complete normal path is visible at `0x4265fd–0x4268d0`:

1. Resolve BITL in the resource context; obtain its owning archive, optional
   VRSN and same-ID BCOD, and hold the code resource before visiting items.
2. Walk BITL records in physical order. Decode a record header and construct
   its item through `0x42620b`. The base constructor `0x4185d0` assigns its
   identity and registers it before returning: append to the resource-owned
   list (+0x80 next, +0x84 previous); insert into the page's other item lists.
3. Send event 5 with the field byte count and pointer. Its base handler at
   `0x41a098` only stores those at +0xac/+0xb0. Send event 0x4e with the parent
   proxy. Advance to the next record. **Fields have not been applied yet.**
4. After the entire resource has been constructed, walk the resource-owned
   list from its head, in authored order, sending event 0x23 to each item.
   Handler `0x41b336` locks the saved field block, sends each field through
   virtual event 1, advances by its big-endian length plus four-byte header,
   decrements the remaining byte count, and clears the saved pointer afterward.
5. Each item's 0x23 handler then dispatches script event 0x17 (listload).
   Thus an earlier item's listload runs before a later item's fields/listload,
   although both items already exist and are registered.
6. After the field/listload pass, unlock/release BITL, send event 0x3a with
   phase -2 to every resource-owned item, call the resource maintenance helper,
   obtain one timestamp, then send event 0x3b with phase -2 and that timestamp
   to every item. Return the loader status.

If an event-0x23 handler returns nonzero, the loader deletes the resource-owned
items through event 7 and clears the owner pointer; this is a failure/cleanup
path, not a request to skip the remaining item. Reentrant deletion or insertion
during these raw-pointer traversals needs controlled original probes.

Initial-code field 0x194 runs synchronously **within step 4**, through
`0x41a04c -> 0x41d38d -> 0x41fdf2 -> 0x40375e`. Consequently it can resolve
later items, but those items' fields need not have been applied. A link field
0x192 is applied in the same field stream: link-before-initial-code and
initial-code-before-link are observably different orders. Do not lump all
initial code into a separate pass after every item's listload.

The resource list follows construction order; it is distinct from the page's
head-inserted traversal/draw lists. Sorting all events by the draw list would
lose the recovered order. Constructor defaults include load phase -2 at +0x46,
script dispatch enabled at +0xd0, and code-owner context initially self at +8.

The outer page entry `0x42041b` sets its phase words to -3 and loading flag 1,
calls the resource loader, returns early on failure, then sets phase words to
-2, enables page activity, performs page setup and advances toward phase -1.
The two resource passes therefore precede this outer page progression.

## Phase events and proxy nesting

At `0x41bbc9`, event 0x3a compares its phase with item +0x46 and sends 0x3c
only on equality. At `0x41bbfd`, event 0x3b can enable eligible item state and
send event 0x2f on that equality; it also maps phases -2/-1/0/1/2 to script
events 0x1b/0xff/0/1/6. Those script events are separate from listload 0x17.
The subsequent timer helper `0x418e13` schedules matching timing entries using
the supplied timestamp; this is not immediate execution of every timed script.

Base event 0x2f (`0x41b5d2`) checks whether the item is loaded, invokes virtual
event 0x5e if needed, stores its result, then performs enable/load-script/timer
setup on success. Proxy dispatch `0x42555f` handles 0x5e by trying the
script-assigned filename (+0x30), then the configured proxy-name lookup,
then a same-context resource fallback. External loading `0x4258e7` opens the
archive and calls the **same two-pass loader**, synchronously, with a nested
resource owner. Explicit resource IDs and named BITL lookup both have paths.

The proxy delegates event 0x23 to the base handler. Thus its initial-code fields
and filename assignment run before its later phase-driven load. Its wrapper
can forward an event to nested children only when the page's current item ID
matches the proxy and both its forwarding flag (+0xfc) and loaded flag (+0x3a)
are set. This is conditional propagation, not unconditional broadcast.

## Database: files, native handles and record access

`aaDatabase.SetPaths` (`0x43be70`) copies separate source (+0x321c) and index
(+0x341c) stems. Original file suffixes are `.tab`, `.mer` and `.ndx`:

| Path | Reader/writer | Recovered role |
| --- | --- | --- |
| source stem + `.tab` | `0x43f096` | Read whole tab-delimited, CR-ended source into memory |
| source stem + `.mer` | `0x43edf7` | Read the field-name source |
| index stem + `.ndx` | `0x43e20e`, `0x43da12`, `0x43dbfa` | Read or create a binary index |

`OpenIndex` (`0x43bb60`) tests the index path, tries index reading plus source
loading, and falls back to `CreateIndex` (`0x43beb4`) when the index is absent.
This fallback can write; OpenIndex is not intrinsically a read-only operation.
It returns descriptive strings for its success/failure paths, not a universal
Boolean. Existing-open-index and error handling must preserve their individual
paths rather than assuming every nonempty result means success.

The index reader uses these sequential blocks and endian conversions
(`0x43e20e`, `0x43eb21`): a 0x48-byte header; an 8-byte count block and
0x22-byte field entries; an 8-byte count block and 0x50-byte record entries;
an 8-byte count block and 0x5c-byte deck entries; then an 8-byte count block
and 8-byte question-unit entries. Numeric fields are byte-swapped from the
stored representation. Names remain byte strings. The conversion helper
identifies fields per record kind; arbitrary native struct serialization would
be wrong. A deck entry contains five level-specific inclusive chunk ranges at
+0x0c/+0x10 onward and five inclusive single-question ranges at +0x34/+0x38
onward; -1 denotes an absent range. Question-unit entries contain two 16-bit
fields and a 32-bit field; the +2 field is the unit's question count.

`SetCurRecord` (`0x43c535`) seeks to a source offset from a 0x50-byte record
entry, then rebuilds tab-separated field pointers. The original comparison
allows an index equal to its stored record count; this apparent boundary defect
needs a negative probe and must not be silently treated as a safe valid index.
`GetRecordField` (`0x43c774`) tokenizes its field request, finds a case-sensitive
field name, returns integer zero on lookup failure, and reads at most 0x100
bytes of field content. A space-suffix request uses the list parser
`0x43f484`, splitting on byte 0x1d into a VM list of strings.

`GetDeck` (`0x43c921`) compares the first four bytes of a deck name and returns
an **opaque native address encoded as a VM integer**, or zero. `GetQUnit`
(`0x43cac3`) uses a zero-based ID and returns an 8-byte native descriptor
address; its bounds check can return null. `GetQUnitInfo` (`0x43cc29`) returns
the descriptor's question count and +4 integer as a two-element VM list.
`RecordLookUp` (`0x43cb02`) searches record names case-sensitively and returns
the zero-based index, with zero also used for failure. `GetDeckList`
(`0x43cd97`) returns deck-name strings. These integer handles must become safe
CE-owned handles eventually; they are not portable addresses to persist.

`GetQuestionCount` (`0x43cf25`) uses the inclusive ranges, selected level and
kind: kind 1 counts chunk units; other positive kinds count single units;
negative level aggregates five levels. With both selectors negative it returns
the deck's stored aggregate. Invalid/missing deck lookup returns zero. Full
index-builder classification, reserved header bytes and malformed-file/error
boundaries remain to be recovered; the block layout alone is not a complete
index writer specification. The builder's main classification is now visible
at `0x43d325`: records are grouped by the first four deck-name bytes; a record
classified as `C` uses `Difficulty_Level` and the final decimal character of
`Cascade_Placement` to create a multi-question unit and advances by that unit's
question count. Other records form one-question units using `Difficulty_Level`.
Each unit stores level, question count and starting zero-based source record.
Range endpoints are updated as the ordered input is consumed. Sorting input
would change unit IDs and history meaning.

## Question selector: graph, history, draws and mission shape

`setDatabase` stores a native database pointer. `addUser` appends a native
history pointer; `delUser` removes the first matching pointer. The count setters
store 16-bit words at +0x50/+0x52/+0x54/+0x56/+0x58; the random-selection word
is +0x5a. Level is stored at +0x48 and its `LevelN` label at +0x4c. No uniform
arity/error contract should be inferred from datafile's stricter validator.
Constructor `0x4404ba` initializes database/graph pointers to null, level and
random-selection to zero, the first four count words to zero, and questions
per deck to six. Authored setters must run before using those empty defaults.

`setSubjectList` (`0x440e29`) replaces the subject graph. The input is a keyed
outer list of locations whose entries contain deck name and native database
object. Deck nodes are shared by name across locations; each reference increases
a deck-node use count. The level used for ranges is the configured level minus
one. The first reference populates that deck's pools (`0x44218a`): inclusive
chunk and single ranges are expanded to unit-ID lists with separate counters.

`loaddecks` (`0x4410f5`) visits the location/deck graph. For each deck it loads
each user's history by that deck name (`0x443800`), then repeatedly takes one
history ID per user until all iterators are exhausted. `0x441e8b` classifies an
ID by its descriptor question count: count != 1 uses the chunk pool; count == 1
uses the single pool. It removes matching IDs only while enough pool capacity
remains relative to use count and requested questions. It is not simple global
deduplication or removal of every historical question regardless of capacity.

`filterdecks` (`0x44136c`) clears and rebuilds the mission list, one sublist per
location, and visits each referenced deck. It computes an integer threshold:

`requestedQuestionsPerDeck * 100 * initialChunkCount / (chunkQuestionTotal + singleQuestionTotal)`.

It draws CRT random modulo 100 plus one and attempts a chunk when the draw is
at or below that threshold. A chunk contributes its descriptor question count;
then single-question draws fill toward the requested count. There is no clamp
visible around the threshold and no denominator-zero guard at the division.
Do not replace these with a generic shuffle and claim equivalent selection.

Draw helpers `0x441fd8`/`0x44205f` choose the first pool entry in nonrandom mode,
or `rand % poolCount + 1` in random mode, remove the chosen node, decrement the
corresponding pool counter and return 0xffff for an exhausted pool. In random
mode the modulo occurs **before** the later nonempty check: an empty-pool
negative probe is required. Pool draws are destructive and shared deck nodes
make location order relevant.

Each mission entry allocates a VM list with initial capacity five, then writes
four one-based elements: native database object, deck name, unit ID, and question
count (one for a single). Capacity five is not proof of a fifth authored value.
`0x441959` subsequently reorders whole entries using cumulative question counts
and the configured questions-per-chunk/deck thresholds. `makeMissionList`
returns the stored mission list; it does not rebuild it. `getMissionList`
(`0x441b88`) builds a fresh deck node and returns all remaining chunk IDs
followed by single IDs. Exact reorder boundary cases, shared-name collisions,
all default settings and deterministic random-seed behavior remain probe targets.

## User history and persistence

`setfilepath` copies the prefix; `setName` (`0x445a29`) compares the new user
name case-sensitively. An identical name returns immediately. A changed name
constructs **prefix + userName + `.dat`**, releases the old profile wrapper,
resets in-memory history, and creates a profile wrapper (`0x439680`) that creates
the file if absent. No path separator is inserted in this concatenation.

`setLevel` builds section name `Level` + decimal level. Changing deck name
clears the current in-memory ID sequence; setting the same name preserves it.
`loadDeck` (`0x445e12`) reads the deck key from the selected level section into a
0x400-byte buffer, clears the sequence, skips its outer delimiters and tokenizes
comma-separated integer IDs. `saveDeck` (`0x445f77`) emits `[id,id,...]` as text
and writes that string through the profile wrapper. The helper returns 1
without propagating the profile write status. `getDeck` returns a new VM list
of the stored short IDs. `getDeckList` enumerates keys in the current level.

`addQuestionUnit` (`0x446136`) finds and removes the first equal ID, then uses
the list's front-insertion path to add the ID; it returns the short ID. This
updates memory only; `saveDeck` is a separate operation. Duplicate existing
entries, malformed brackets, list capacity/truncation and failure visibility
need original probes. No history file or owner profile was modified here.

## Shared profile cache and datafile contract

The backend is a custom section/key text-file cache, not a call to Windows
WritePrivateProfileString. Original helpers are `0x457f14` (open/cache),
`0x457b01` (read), `0x4585be` (edit), `0x45881d` (buffer mutation),
`0x45876c` (disk flush), and `0x458a58` (write-through switch).

- Cache records are shared by resolved file identity and start with
  **write-through enabled** (+0x0e = 1). On-disk files must be smaller than
  0x10000 bytes; growth checks the same 16-bit buffer boundary. Embedded NUL
  is rejected; a final DOS EOF byte 0x1a can be removed, but an interior one
  is an error. New sections/keys use `[section]\r\nkey=value\r\n` syntax.
- Section and key searches compare lengths and use a case-folding comparison
  (`0x483f40`). Key scanning trims spaces/tabs before `=`; semicolon lines
  are skipped. Reads trim trailing spaces/tabs and strip matching outer single
  or double quotes. Missing keys and buffer truncation return distinct status
  codes (0x296c and 0x296e); reads are not universally successful empty strings.
- Null section/key/value combinations implement clear-all, delete-section,
  delete-key and replace/append-value paths. The mutation updates the cached
  text, then flushes immediately when write-through is on; otherwise it records
  a dirty flag. Flush acquires the file lock, opens for writing, sets file size,
  writes the full cached content, closes and releases the lock. Atomic replacement
  or crash durability is not established by this path.
- Switching from buffered to write-through flushes dirty content and clears
  the dirty flag only after a successful flush. It returns the prior mode.
  The component's `close` and destructor **do not call this flush path**.
  Cache-removal helper `0x458202` releases resources without a flush call.
  Shutdown-helper reachability and all buffered-close/reopen outcomes still
  require a controlled run; do not assume close saves buffered edits.

`datafile.open` closes/reset its previous component state, resolves the supplied
path relative to the original default context, creates a missing file, then
marks it open. `openReadOnly` uses the existence/retry UI path and does not
create a missing file. It sets the **same** open flag as `open`; no independent
read-only flag or write rejection is visible in the subsequent mutation paths.
Its name is therefore insufficient evidence for enforcing a read-only policy.
The default-directory chain is now traced: `0x453ee2` returns the startup
directory object at 0x49b0c4. Initialization `0x4543eb` records the process
current directory separately at 0x49b0c0, obtains the executable path with
GetModuleFileNameA, and constructs its parent using `..` for 0x49b0c4. The
component calls `0x453dfc` to set the process current directory to the selected
default before constructing its requested path. Normalization `0x453695`
converts `/` to `\`, handles drive/UNC paths, and resolves relative paths via
the current-directory object. `open` does not locally restore the previous
directory. Per-user history also selects that default before constructing its
path. Drive/UNC errors, long names and subsequent-directory side effects still
need probes; no external directories were written during this research.

Section names for add/select/delete are checked as lengths 1–31 and return
0x64 otherwise; select clears the old section before validating the replacement.
`isASection` accepts length 32 too, uppercases its argument and enumerated names,
and returns a Boolean in its result cell. `buildList` enumerates sections.
`addSection` selects it and writes/deletes a temporary `pd_dummy` key to create
the section. Empty current section/key yields 0x65 for several value helpers.

`setVar` (`0x446e57`) deletes the previous key then serializes its value. A
string gets outer double quotes, doubled internal double quotes and a trailing
semicolon; other values use the value serializer with `;` suffix. `getVar`
reads text and calls the original source-expression evaluator `0x40363a` to
reconstruct a VM value. `loadVars` composes `key = value` and evaluates it,
skipping names beginning `/`; it has script-visible assignment side effects.
`loadSection` evaluates each value into a newly returned keyed VM list without
assigning those keys as globals. These are not equivalent to plain INI strings.

All 18 method arities remain in the evidence manifest. The numeric dispatcher
returns its VM result cell at `0x4469f3`: helper status words are often stored
only in a local and **are not the VM result**. In particular mutation/open/close
errors cannot be modeled by universally returning helper status or success=true.
`deleteFile` closes, calls the filesystem delete helper and returns helper zero
regardless of that helper's recorded deletion status. The write-through wrapper
also zero-extends a short before comparing it to -1, so its final Boolean test
does not faithfully detect the backend's short -1 error. Preserve these as
observations and probe their script-visible outcomes before choosing CE policy.

Original path selection is evidence, not authorization to copy external writes
into CE. Future CE-owned history/configuration must retain permanent portable
storage ownership. No such implementation or storage-policy change is made here.

## Encryptor

`setkey`/`getkey` store/read one 32-bit key K; initial K is zero. `encrypt`
(`0x4480f5`) encrypts the pair `(inputInteger, K)` with four derived key words
`(K, K+1, K+2, K+3)`, unsigned 32-bit arithmetic, 32 rounds, delta 0x9e3779b9,
and the observed shift/XOR/add recurrence. It returns the two resulting signed
decimal integer strings joined by a comma. This is a TEA-shaped recurrence
observed directly; no external algorithm-name assumption is needed.

`decrypt` (`0x4483bb`) parses `%ld %1[,] %ld%n`, rejects trailing non-whitespace
using `%1s`, reverses the 32-round recurrence starting at delta << 5, then checks
that the second recovered word equals K. On success it returns the first word.
On parse failure it sets `Invalid Encryption String` and returns zero; a key
failure sets `Decryption Key mis-match` and returns zero. Valid plaintext zero
is not distinguishable by the numeric result alone. The dispatcher's method
table maps **both `decrypt` and `isValid` to ID 4**, so isValid invokes the same
decrypt-and-key-check operation rather than a separate Boolean predicate.
Error-string lifetime and integer overflow parsing remain runtime probe targets.

## Recovery boundary and next evidence

Normal two-pass item initialization and synchronous nested proxy loading now
have a caller-to-evaluator trace. Database source/index organization, opaque
handles, selector pool/filter/mission paths, history formats, profile mutation
and flush paths, datafile serialization and encryptor recurrence have concrete
original-code evidence. The smaller directory/string/font/clock interfaces
remain documented in the companion VM contracts.

The previously open index validation, exact reorder/shared-name, shutdown,
deletion/code-owner and conversion/unwind questions are now traced in
[edge cases and lifecycle contracts](THINKQUICK-EDGE-LIFECYCLE-CONTRACTS.md).
That supplement distinguishes defined paths from unguarded original behavior
and specifies the step-3 negative probes. Runtime equivalence remains unclaimed;
broader VM operator/packed-value and interactive-error coverage is still open.
