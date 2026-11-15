# Known inventory behavior model closure510

Status: VERIFIED for static known implementation/production-owner review only.
Parentabb35bb2818. No code change, build or executable run.

This review closes implementation classification for the matrix's enumerated
inventory operations, retaining their separate verification and cross-owner
qualifications. It reuses the original/native contracts and current semantic
owners. Historical NATIVE-EQUIPMENT/ITEMS-COMMERCE documents describe a machine
ABI stage: that stage is source evidence, not the native production owner.

| Requirement | Current semantic owner and required model |
|---|---|
| inventory.01 definitions/records | Closed509: typed source catalog, all256 opaque six-byte records, canonical core persistence. |
| inventory.02 grant/remove/capacity | Inventory separates award grant, merchant quantity purchase and removal; preserves source byte quantity/unlimited255, overflow/capacity policies, authored slot order, flags/empty suffixes. SearchRules and BattleLoot own currency/food/capacity and checked reward presentation; merchant/service owners call the same record owner. |
| inventory.03 weapons | EquipmentOwnership/selectWorldWeapon/EquipmentActions own member and available-copy admission, replacement, hands/two-hand reservations, weight, STR/DEX/INT constraints, owner bits, record commit and derived recalculation. InventoryWindow and battle482 supply actual member/context. |
| inventory.04 armor | selectWorldArmor/EquipmentActions own slot replacement, shield/hand rules, ownership/weight and source derived fields. Battle482 passes explicit combat context and preserves unrelated live combat derived state. |
| inventory.05 books | SpellBook readiness and InventoryWindow own eligibility, available quantity, numeric0..9 direct shortcut478, replacement/unready and shared world/battle lookup. Reserved definitions and original NULL45 are explicitly classified499. |
| inventory.06 Use | Existing item-use-audit.json and478 contract enumerate57 usable misc definitions:47 documents,9 orbs,1 coordinate tool. InventoryActions resolves original document table/text; InventoryWindow owns read/paging/return and coordinates; WorldNavigation owns typed binding and492's aligned/X0 selection. No use consumes an item. Combat refuses Use before world operations482. |
| inventory.07 Drop | InventoryActions preserves protected/readied/spare-copy policy and retained commerce selector/quantity semantics, through canonical inventory/field transaction. Current CommerceCounterContext initializes a known nontracked identity255/0 for new/pre-context native saves; it is not the historical439 unresolved startup-memory dependency. Existing recorded native context/import qualifications stay separate. |
| inventory.08 torch | Source torch count, active state and expiry are native476b; no historical invisibility misinterpretation. |
| inventory.09 Search/Open/secret doors | SearchInput/World/SearchRules/LockRules own directional selection, predicates, tool/member admission, retries, trap and item/food/gold/generated outcomes. WorldOpen hands locked doors into that shared owner487. Source map7 repeat specialization exists.495 adds living-only selection and terminal defeat before any subsequent actor/clock pass. |
| inventory.10 stealing | Guarded53|64 records enter SearchInput's confirmation owner before lock/reward. Decline cancels; confirm raises canonical guard hostility/alarm then resumes the same search transaction. No alternate treasure implementation. |
| inventory.11 locks | LockRules owns skill/tool/attempt/RNG/trap/death effects; World.pickDoor/searchQuestItem commit record/party/inventory/counters.495 terminal handling supplies the previously excluded game-over consumer. |
| inventory.12 generated/scripted handoff | SearchRules/BattleLoot/Conversation/Recruitment retain typed item/currency/companion handoffs, generated group choice/capacity and canonical record commit.494/500/502 classify the known immutable offer/mutation/contact award encodings; unknown additional original code producers remain story.08. |

The current read-only Use preflight agrees with the existing audit: all57 have
one of those three owners; all47 document references are in the original76-entry
table, terminate within the768-byte buffer (longest577) and contain only native
supported plain text/CR. The old item audit and old native suites were not rerun.
This is data admission evidence, not57 actual Use workflows or new raster parity.

The ten remaining known operation rows become IMPLEMENTED. Existing broad
verification statuses remain unchanged; in particular component/historical
evidence does not become current ordinary Engine cold/session coverage. The
inventory subsystem stays IN PROGRESS for those joined verification requirements.
There is no new production gap demonstrated by this review and no reason to
rewrite these owners or rebuild506b.

Additional arbitrary script producers, unqualified historical machine snapshots,
and exact historical presentation outside the known CE interfaces remain
explicitly separate. No entire-port percentage, earned campaign or release claim.
