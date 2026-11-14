# World Attack480

Status: IN PROGRESS. Parent callbacks479 commit175176310cb. Reuse the existing
combat contract, EncounterPreparation/State, BattleAftermath, source roster,
round/return owners and immutable earlier evidence. No campaign traversal.

Source GAME932A-9416, lower1559-1598/17E9-185C, GAME8E70/8F8F/9417.
Native WorldInput currently recognizes A but returns Ignored: genuine missing
command, not a tactical Attack replacement.

1. A begins exclusive direction input with source prompt12; no time/actor turn.
   Existing common input preparation precedes A and can consume route RNG;
   pending direction/cancel itself must preserve core/RNG state.
2. Arrow keys select the source near cell76/110/94/92. Escape clears/cancels;
   other keys retain direction input. No unrelated command or key forwarding.
3. Frigate119 uses reversed authored eight-cell scan GAME6AA5, independent of
   selected direction:92,109,127,128,112,95,77,76. Other appearances use chosen
   near cell only. Do not mistake the source branch direction.
4. Empty target prints24 and refusal cue; return to ordinary command ownership.
5. GAME8E70 refuses all attacks from map48 at16/18,35 with the same no-target
   reply/cue. This is distinct from battle protected-target lifecycle.
6. Speed>=4 refuses18, without inventing a refusal cue or turn.
7. GAME8F8F permits an owned frigate/caravel, otherwise refuses standing on
   water229..232 with18. Skiff/mount membership is not blanket boat permission.
8. Source target225, type2 target205 and type6 target209 refuse24/cue.
9. Resolve actual retained actor lookup/kind/record. Keep the real bank/slot,
   direction and selected cell; no manufactured actor, HP, loot or encounter.
10. Accepted input hands off to existing EncounterPreparation and complete battle
    lifecycle. No extra actor turn/clock advance for starting A.
11. Retain pending direction and post-battle alarm through a bounded native
    WorldInput wrapper; older supported versions remain readable.
12. Source93F1 queues alarm after battle/return when map is nonzero except101
    and raw selected kind>=2. Do not raise it before encounter formation or
    change target capability early. Flee also retains this caller behavior.
13. After existing parent-contact retirement, before final viewport refresh,
    apply source9417 guard/civilian capability changes and typed alarm state.
    On foot mark transport-control owner used so the alarm actually persists.
14. Clear queued alarm once, then return through existing normal world/input/
    session/audio owners. Do not add independent battle-return machinery.
15. Preserve original refusal/cancel/accepted state and clock/RNG invariants;
    ordinary notice/cast/search/roster/conversation modals keep exclusivity.
16. Use one focused source/owner/codec/encounter-return batch and one integration
    build after implementation. No earned traversal, exploratory launch or old
    audio/whole-campaign suite. Qualification limits remain explicit.

Original3-command shares part of932A but bypasses battle initiation and has a
different caller. It is not implemented by pretending it is A. Remaining
tactical F/+/-/selection aliases and Quick067D remain separate combat requirements.

Source9417 refinement before native build:257F is written only when a matching
NPC tile is found. Previous boarding/cannon owners set the flag unconditionally.
WorldTransportControl::raiseAlarm now owns the shared predicate and all three
callers preserve a prior alarm while avoiding a new one for no matching NPCs.
The source fixture includes authored and empty NPC arrays across six asset
families. No old audio/transport suite rerun; the directly changed shared alarm
transaction is included in this coherent source-driven batch.
Pending-direction codecs do not change the existing normal CE policy that permits
saving at idle world commands/ending screens. Full normal cold A/battle-return
acceptance remains separate from focused control/formation/return-owner proof.
