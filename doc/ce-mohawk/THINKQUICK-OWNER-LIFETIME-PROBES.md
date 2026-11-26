# Nested callbacks, linked ownership and bounded resource unload

November 26, 2026. Step 3 research; no engine implementation or rebuild.
The paired script cases and five original-only unload observations have separate
qualification boundaries. See THINKQUICK-OWNER-LIFETIME-OBSERVATIONS.json.

## Nested cross-item callback gap resolved

The prior diagnostic did execute the back-handler, but did not resume the next
entry in item 902's script. The generated named-call subentry lacked word padding.
Appending a zero byte to the nested `back` call's name payload fixes the fixture;
it does not modify the original interpreter. Final trace:

1. Item 901 listload calls item 902's `callee`.
2. Item 902 calls item 901's `back`. Its expression observes self ID 901,
   sender ID 902 and stores a true predicate.
3. Item 902 resumes its following expression, observes self ID 902 and sender
   ID 901, and stores another true predicate.
4. Sender, both item contexts and all checked VM/frame/argument state restore.

A false control expects the back predicate to be zero and returns false. Both
fixtures have zero VM errors. Dispatch traces contain (receiver,code-owner)
pairs (901,901) then (902,902) for the two expressions, and qualification requires
this exact sequence. Earlier unpadded attempts remain retained locally; they are
fixture failures, not evidence of original callback suppression.

## Linked receiver versus code owner

The generated scripts execute original ScriptLink with the one-argument default
mode and two-argument mode-zero removal forms. The receiving item's own handler
writes 1; the linked donor appends digit 2, producing 12. A second donor expression
converts self to a numeric ID and records 901. The fetch trace verifies that
execution retains **receiver 901 with code owner 902** in both donor expressions.
Removing the link and calling again produces 1 with no further donor trace.
False, order-only and standalone item-ID controls separate these observations.

The bridge logs the real original receiver and its temporary code-owner field
at each fetch. Exact receiver/owner sequences are required, including two donor
expressions and their absence after removal. These fixtures still share generated
BCOD and bypass original cache fetching; the trace establishes dispatch context,
not bytecode selection across distinct resource archives or donor retention.

The minimal global current-item-ID context used for expression calls is also
needed by ScriptLink's event delivery. Missing initialization caused an early
fixture access violation and is retained as a setup failure. An exploratory
three-argument call also had incorrect stack ordering; final fixtures use the
reviewed one/two-argument forms. No engine behavior was changed to accommodate it.

## Bounded resource-lifetime observations

`probe_mohawk_v5_resource_lifetime.py` executes original resource-unload routine
**0x4268d3** in fresh processes stopped before application entry. Its owner structure
and item list are generated fixtures. Item event callbacks record the actual
received event, snapshot the code handle and remove the list head. A process-local
jump at original release entry **0x45a2c3** records release ordering and returns.
These callbacks deliberately replace item destruction and code-cache release;
they do not execute real destructors, allocate a real cache entry or free memory.

| Original routine input | Observed calls/state |
| --- | --- |
| Active owner, two items, handle 0x1234 | Event 7 twice while handle remains present; release after the item head is empty; owner fields cleared |
| Active owner, two items, no handle | Event 7 twice; no release; owner fields cleared |
| Active owner, no items, handle | Release only, with empty head; owner fields cleared |
| Inactive owner with deliberately populated fields | No callbacks or release; populated state remains unchanged |
| Repeat unload after active two-item unload | Only the first invocation's two event-7 calls and one release; no duplicate release |

All five observations pass their recorded order and owner-state checks. They are
**original-only control-flow probes**, outside the paired script comparison count.
They test the original routine's decision/order logic with explicit observational
callbacks; they are not a successful real-resource lifetime, memory safety,
destructor, dangling-donor, archive-unload or original-versus-CE qualification.
In particular, inactive state short-circuits cleanup rather than repairing
inconsistent populated fields. No real game files, profiles or saves are written.

## Reproduction and remaining boundaries

Run probe_mohawk_v5_vm.py with `--owners-only` for eight new paired script cases,
or no filter for the expanded set, using the documented original/CE/extras/data
options and fresh disposable roots. Run the separate resource-lifetime tool with
`--original <pinned CarmenTQ.exe> --root <new disposable directory>` for its five
original-only cases. It rejects an executable with a different SHA-256.

The callback setup gap is closed for this two-item normal path. Remaining step-3
work includes linked code from distinct resource archives, real cache retention
and release, actual item destruction and incoming donor links, mutation/deletion
during callbacks, broader cascade/target/argument paths, isolated persistence and
cold restart, and OS-error UI comparisons. Implementation stays on hold.

## CE comparison and validation

Eight new paired script cases: **3 matches, 3 mismatches, 2 unqualified**.
CE matches the order-only linked result 12, the post-unlink order-only result 1,
and the false control. Two nested-callback cases stop at unsupported token 66.
The full linked predicates and standalone item-ID predicate differ: original
self + 0 satisfies ID 901; CE does not. Thus a false full linked predicate must
not be attributed to wrong receiver or traversal order. Source inspection shows
CE LBValue::toDouble falls back to zero for item values; that is consistent with
the isolated conversion difference, rather than a direct raw-value observation.

Fresh aggregate: **60 paired cases = 23 matches, 10 mismatches, 27 unqualified**,
zero incomparable programs; overall fail. Five original-only unload controls are
separate and do not increase paired totals. All 60 original evaluations complete;
the eight new script cases have zero VM errors and exact dispatch postconditions.
Eleven comparator guards and eight inventory tests pass. Committed source bytes
are verified against runtime identities, and the committed comparator reproduces
the aggregate. All 21 recorded engine-source hashes, 346 CE Test files and the
unchanged baseline executable match preservation identities. No accepted
installation or release was updated. Step 3 remains in progress.
