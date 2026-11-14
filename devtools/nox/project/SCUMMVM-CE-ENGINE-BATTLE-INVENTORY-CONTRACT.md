# Battle inventory482

Status: IN PROGRESS. Parent tactical-controls481 commit479edaeafd9.
Reuse InventoryWindow, EquipmentActions/Ownership, InventoryActions, SpellBook,
BattleRecords, EncounterState and BattlePresentation. No second inventory UI or
equipment transaction implementation; no campaign/old/audio suite.

Source AUXD975/E3D2/E68A, inventory AC6D-ACAD/AD1F-AD9C/AE70-AEF6,
9A12-9A2A, B535-B543 and B554-B831. Combat uses map-type FF in the original;
native retains the actual parent map and supplies explicit typed combat context.

1. Ordinary tactical Tab opens native inventory at the active member, using live
   battle member records, not stale pre-battle world HP/MP/status/skills.
   The existing native inventory layout replaces original stats/tab/page ABI;
   expose selected live stats useful for readiness. Existing target/cast/cannon
   Tab continuations retain priority over opening inventory.
2. Modal keys remain exclusive, including V sound-mode shortcut, Q and lock/
   movement/attack. No clock/RNG/AI/action/round retirement for opening, browsing,
   refusing, transacting, cancelling or closing this free inventory command.
3. Source AC7D refuses armor kind1 unless inventory byte5 is exactly1 (shield).
   This applies to ready and unready. Do not treat all armor as battle-ready.
4. Books refuse9 before eligibility/slot selection, including unready.
   Source AE70 refuses all item Use in combat (10); do not bind orbs, read
   documents or use coordinate tools through a world-only context.
5. Weapon/shield ready/unready reuses actual ownership/weight/STR/DEX/INT/hand
   transactions. Drop remains existing protected/spare-copy semantics; source
   has no combat-only drop exclusion. Keep commerce-counter dependencies explicit.
6. Source9A12 invokes B563 partial recalculation with global dirty flags0251
   weapons/0252 armor. Weight remains the actual readiness transaction result.
   Only changed equipment domains recompute for all party members; unrelated
   combat armor/weapon derived state survives. Source B747 always recomputes
   DEX hit chance when dirty recalculation runs. Full world variant B554 stays
   the existing default. Do not run a full world recalculation after every key.
7. Commit actual inventory/book/equipment/commerce/quest effects to their existing
   game owners and changed live member data back to BattleRecords. Preserve
   enemy/summon/credit/compact facing/status/lock/cast/armor-backup state.
8. Close restores the tactical presentation from current owned state, so changes
   affect subsequent actual Attack/Cast. No obsolete pre-menu image becomes the
   authoritative state. An active inventory retains its exact modal phase and
   context through bounded NIWN3/NBTS18 wrappers over supported older codecs.
   Keep normal CE save eligibility unchanged; codec proof is not normal cold
   mid-battle save acceptance.
9. Reuse refusal sound events. Preserve event ownership across begin/load; no
   music/mixer/synthesis change and no accepted audio suite rerun.
10. Focused batch: original combat guard + partial equipment records; actual
    native menu keys, refusals, permitted transactions, live handoff, pending
    codec/close and subsequent Attack/Cast preparation. One integration build.

Original full stats/pixel/page layout, normal Engine cold/session qualification,
earned acquisition/campaign and Quick067D remain separate. No expected outcomes
may be injected after starting a tested inventory transaction.
