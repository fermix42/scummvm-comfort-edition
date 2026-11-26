# ThinkQuick: edge cases and lifecycle contracts

Recovered November 26, 2026 from the pinned CarmenTQ.exe identified in
THINKQUICK-CONTRACT-EVIDENCE.json. Addresses below are virtual addresses;
manifest ranges are end-exclusive. These are static observations, not executed
original-runtime results. No engine behavior changes accompany this document.
This supplements the VM and loader/storage contracts and closes the specific
static edge/lifecycle questions left at the second checkpoint.

## Deletion, links and resource ownership

Script namespace command 2 (`0x40735f`) calls `0x406c48`, which resolves a
recipient and appends event 7 through `0x4206e7` to page queue +0x88. It captures
the resolved item's short ID, not a retained item pointer. Numeric target zero
has the broadcast path; an unresolved nonzero target raises VM error 13.
The consumer `0x41fecb` drains this FIFO through `0x420c02`, removing each
head **after** dispatch. New events appended by a callback can therefore be
processed in the same drain. Reentering the drain while a callback runs can
see the current head again; there is no pop-before-callback protection here.
Ordinary script destruction is deferred beyond that VM call, not direct deletion
of its active receiver. ID reuse before delivery is not pointer identity.

Direct event 7 (`0x41a500`) performs notifications, attachment/focus cleanup,
then virtual destruction. This branch does not test the receiver's active-script
counter. It must not be conflated with the queued script-command path.

The item destructor (`0x4189c5`) invalidates each registered VM value via
`0x40f2cd`, replacing it with the string **"~" + itemName**. Registration and
removal operate on individual value-cell pointers (`0x418d09`); this is not
reference counting the item. Aliases observe the changed cell. The destructor
also releases owned scripts, outgoing ScriptLink nodes and list memberships.
It contains no incoming-link repair pass. ScriptLink donor pointers are raw
pointers in link nodes, not registered VM value cells.

Dispatch (`0x41e174`) advances script/link nodes after callbacks, without a
snapshot, donor retain or recursion bound in this path. Link removal
(`0x419d08`) frees a matching node immediately; clear frees the entire outgoing
list. Existing-link updates preserve order. Self-removal during traversal or
using an already destroyed donor has no recovered safe fallback. Record these
as unguarded original paths, not a promise that CE should reproduce a crash.

Resource unload (`0x4268d3`) destroys every owned item with event 7 before
releasing its code handle. Normal resource lifetime therefore keeps bytecode
alive through item teardown. Linked evaluation (`0x41d38d`) obtains bytecode
through the code owner's resource, separately from receiver identity. Cross-
resource dangling incoming donors are not repaired by the observed destructor.
Page teardown (`0x421475`) deactivates/unloads, clears the global active page,
frees pending event/action/timer nodes and owned event-1 payloads, then clears
the loading flag. It does not execute pending queued events as a final drain.

## Conversion, alias and error boundaries

Real-to-integer uses `0x477860`: save the x87 control word, select truncation
toward zero, convert to signed 64-bit, restore the control word. VM conversion
uses the low 32 bits. This establishes finite fractional truncation and low-word
wrap for values representable in int64, not saturation at int32 limits.
NaN/infinity/out-of-int64 behavior depends on x87 exception masks; no fixed
script result is qualified without a controlled run.

Reference creation (`0x40f176`) resets the destination before binding. A null
target leaves the reset value. Self-target assignment consequently leaves the
reset value rather than creating a self-cycle. Other cells are raw alias targets;
recursive conversion follows references without cycle detection. List growth
(`0x4168fc`) takes a signed short, ignores values below 1 and never shrinks.
List insertion (`0x4167b2`) ignores indices below 1; no upper-index bound is
visible before the indexed store. Invalid list accesses are not uniformly zero.

VM error construction (`0x402fc6`) ends in a C++ throw. The evaluator's exception
metadata (`0x48c470`) has one typed catch, whose type descriptor at `0x493430`
is `.PAD` (pointer-to-const-char MSVC encoding). Catch handler `0x403a5d` resets
the evaluation stack cursor if nonempty, clears active code/offset and returns
the shared result cell `0x49a540` without assigning a new result there. It does
not supply a fresh zero result or transactional rollback of prior side effects.
The stack reset is a cursor reset (`0x4061b0`), not a push of a default value.
This typed catch is not evidence of handling arbitrary OS faults.

Frame push (`0x414c47`) increments the short depth before checking the 32-frame
boundary. Failure does not decrement it in that helper; callers can raise error
18. Script dispatch saves/restores context on normal return, not through its
own exception handler. The evaluator's handled VM error can return normally;
other failures must not be assumed to restore every outer context.

Formal binding (`0x404dda`) evaluates each default before substitution, consumes
available actuals in order, and retains the evaluated default for an omitted
actual. The binding loop has no surplus-actual error branch; native-component
arity validators are separate contracts. Failure during default evaluation is
not evidence that earlier default side effects are rolled back.

## Database corruption and selector boundaries

Index read (`0x43e20e`) reads a fixed 0x48-byte header and count-driven blocks.
There is no magic/version/source-hash validation or trailing-data rejection in
this path. Header counts drive allocation and iteration without a demonstrated
check against all fixed field/deck capacities. The read wrapper (`0x45559b`)
requires the requested byte count: a successful OS short read becomes status
0x283f. Thus truncation is detected, but structurally complete malicious counts
are not established as safe. No source TAB freshness check is performed here.

Read failure can retain the nonzero +0x420 file handle and partial state; the
already-open branch returns success on another open attempt. Ready +0x424 is
set only after successful close. The database destructor (`0x43afba`) frees
buffers and the base component, without closing that retained handle itself.
These paths have no recovered atomic rollback/retry contract. The record setter
accepts record==count before its offset lookup; a null GetQUnit result is not
safely handled by every later unit accessor.

Mission reorder (`0x441959`) uses 16-bit cumulative/moved/insertion counters.
For each location, walk its captured list length with one-based indices. Add
entry[4] to cumulative, wrapping at 16 bits. Capture the first index where
cumulative > questionsPerChunk. Whenever cumulative > questionsPerDeck, copy
and delete the current entry, insert at the captured insertion index, increment
that index and add its weight to moved; stop when moved >= questionsPerChunk.
The scan index still increments after moving. Cumulative is not reduced.
Threshold equality does not trigger a move. This is not a stable partition.
If configuration allows deck overflow before the chunk insertion point exists,
index zero is ignored by insertion after deletion: no configuration guard here
establishes preservation. Signed weights enter the wrapping short arithmetic.

Shared deck lookup (`0x442660`) searches all locations by case-sensitive name;
the first matching deck retains its database association and gains a use count.
Same-name decks from different databases do not automatically get separate pools.
A repeated location key makes factory `0x442530` return null, while the caller
continues toward append without a null guard. addUser walks then appends without
deduplication; delUser removes the first match. Repeated user pointers share
history state rather than cloning it. Empty pools and invalid denominators have
unguarded arithmetic/access paths already identified in the storage supplement.

## Profile shutdown and durability

Startup `0x42974a` registers `0x458570` through the CRT on-exit registrar
`0x477998`; filesystem cleanup was registered earlier. The exit consumer
`0x4782f2` invokes registered callbacks in reverse order on normal cleanup.
The profile cache is therefore cleared before that filesystem teardown.
Cache clear repeatedly calls `0x458202`, which frees cached records/buffers
without invoking profile flush `0x45876c`. Normal cache shutdown is reachable
and is **not** an implicit flush of buffered edits. Abrupt termination is not
claimed to run these callbacks.

Component close resets its section/open state without removing the shared cache.
A reopen can reuse the same in-memory file, so close/reopen visibility does not
prove durability across restart. Explicit write-through mutation and enabling
write-through after dirty edits remain the recovered flush paths. Read-only,
missing-directory and interactive retry outcomes need controlled original runs;
helper statuses must remain distinct from script return cells. CE portable
storage ownership is unchanged by these historical path observations.

## Coverage and comparison handoff

The previously open deletion/lifetime, conversion/unwind, index validation,
mission reorder/shared-name and shutdown-reachability questions now have explicit
static contracts, including the absence of safety checks. Do not invent safe
original results for undefined or unguarded paths. Step 3 must measure observable
normal and negative outcomes before implementation decisions or equivalence claims.

Required probe groups: queued self-delete and ID reuse; deletion with aliases
and linked donors; list/link mutation and nested loading; handled error followed
by another call; omitted/surplus/default-side-effect calls; fractional/overflow
conversion and alias cycles; truncated/stale index and retry; duplicate location/
deck/user names and threshold equality; buffered close/reopen versus cold restart;
read-only/missing-path errors; encryption zero/key/parse/overflow vectors.

Static recovery does not qualify every arithmetic operator error, packed-value
consumer or interactive OS error path. Those broader VM semantics remain visible
in the companion matrix; no blanket completion of step 2 or gameplay acceptance
is asserted by closing this checkpoint's tracked lifecycle questions.
