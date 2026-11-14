# Party identity and ordering contract

Status: IN PROGRESS. Defined before production edits, November14.
This is a complete scoped party-order owner, not completion of the whole input,
inventory or combat subsystem. Dependencies: Party, Inventory, PartySelection,
HgrFrame, WorldInput. Consumers: battle formation/Quick roles, equipment,
recruitment, inn and semantic saves.

## Original evidence

GAME94AC selects SRTN.INVENTORY selector7. Existing local disassembly
`conversion/item-command-7.asm`9600–9791:
two selectors960B/961C; cancellation unwinds at9684; character record swap
9688–96D7 through GAME1970 mapping; equipment swap96DA–9723 is16 bytes;
ownership permutation9725–9791 visits all256 six-byte inventory records,
both bytes3 and4, retaining all other bits/bytes. Tables9792/9799 provide
set/clear slot masks, shared convention with EquipmentOwnership data.
Manual pp41–42: N changes roster positions and therefore battle starting
positions and Quick roles. Existing native selection GAME8600–8728 contract
supports living default, arrows/space, shifted numeric aliases, Enter/Escape.
Confirm exact original numeric-slot and dead-selection mode before final closure.

## Required behavior inventory (12)

1. N begins first selection only at the world command boundary.
2. Select first occupied roster member; display names and selection prompt.
3. Select second occupied member with the same existing native selector.
4. Cancellation at either stage restores underlying frame and leaves all state.
5. Same-slot exchange succeeds without mutation.
6. Exchange all128 character bytes, including opaque/status/name data.
7. Exchange all16 equipment bytes with the character identity.
8. Permute both ownership masks on all256 inventory slots, including empty ones;
   preserve quantity/category/flags and bits belonging to other members.
9. Refuse unready party/out-of-range selections atomically.
10. Preserve party count, unused roster slots, gold/food/clock/RNG/world/quests,
    retained commerce selector, spellbook, navigation, inn and audio settings.
11. Block world commands, saving and cross-modal transitions while selecting,
    following existing normal-world save eligibility. Restore native command
    boundary on close; existing serialized party/inventory formats persist result.
12. Consume no world turn, RNG or actor processing for selection/commit/cancel;
    subsequent inventory and battle consumers see the new slot identities.

## Native design and approved CE behavior

Party owns record exchange; Inventory owns mask permutation. A small transactional
coordinator validates before either commit and resolves the authored masks from
data through EquipmentOwnership. UI owns only two selectors/underlay and calls
the coordinator. Reuse PartySelection; no raw machine-memory scratch or original
instruction execution. Names/prompts use native glyph rendering; pixel-identical
Apple II record-swap animation is not required by this semantic contract.
Preserve supported Classic/Modern paths and current enhanced I inventory access.
Do not reopen audio or add new audio cues as part of this subsystem.

## Validation budget and unfinished evidence

One focused deterministic batch: full-record sentinels, every valid pair/count,
both ownership fields/all records, same-slot/cancel/refusal, round-trip semantic
state and UI phase transitions. Use actual C++ owners; source-derived fixtures,
not a rewritten Python model as implementation proof. One native-only integration
build when complete. No game launch/traversal/campaign/audio acceptance.
Resolve selector original mode and actual production callers before claiming all
12 verified. The contract may be clarified by original evidence, never weakened
silently. B-ORDER owns this work; B-COMMAND remains independently open.
