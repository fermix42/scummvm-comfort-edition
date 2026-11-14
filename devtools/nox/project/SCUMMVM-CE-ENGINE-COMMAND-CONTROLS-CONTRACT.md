Controls476b update: known roster/torch/icon/W implementation is complete;
focused receipt records29 verified behaviors and7 integration qualifications.
B-COMMAND/B-TORCH missing implementation retired within this contract. World
Attack/tactical aliases remain combat-owned; B-LOCATION stays UNKNOWN.
Next: world/simulation/NPC source closure, no gameplay discovery.

# Remaining player command controls

Status: IN PROGRESS. Defined before production changes. Parent information owner
committed0f751892ffd; audio473 and information475/party474b remain frozen. This
batch completes the remaining normal roster, torch, icon and W controls together.
It does not implement unrelated world A Attack or the remaining tactical F/+/-/
selection commands; those stay combat-owned. Unknown captions remain B-LOCATION.

## Original and existing owners

* Tab: GAME946F–9494 paints roster via8029/EB42, accepts another Tab or Space
  for inventory; other keys close and are consumed. EB42–EC76 displays every
  occupied name, HP/maxHP and MP/maxMP bars, P/S status. EC77–ECC0 maps positive
  percentage to seven segments at15/30/45/58/72/86%; current0 is empty. HP0 also
  suppresses the MP bar. 11CC computes floor(current*100/max) and converts BCD.
  Source bars occupy seven byte-columns each (HP31..25, MP32..38), pixel rows
 18/19+16*member, alternating55/2A patterns and zero markers40/01. Names at25,
  text row1+2*member, status marker37. Reuse GlyphFont/HgrFrame and established
  native Party records. Existing InventoryWindow preserves its transactions,
  documents/book/ready/drop UI and source-backed enhancements.
* I: BS.FE37–FE59 checks field32 torch count first. Empty prints17; positive
  count with active light returns unchanged. Otherwise set ticks120/active1,
  decrement the count and print1. No turn/RNG/actor action or sound call.
  Reuse TurnClock, WorldViewport/lighting and immutable messages; do not remove
  inventory functionality when restoring original I (Tab/Tab now supplies it).
* World =: GAME94B1–94D7 cycles selected on-foot hero246..253, wraps254 to246;
  refuses a differing active transport picture, prints18 and source refusal cue,
  then redraws. WorldTransportState already retains baseAppearance and returns
  it on dismount: reuse that state, no redundant new hero save field. NewGame
  already saves active appearance. Update on-foot control metadata consistently.
* Tactical =: original combatTurns D97E->DA0A–DA38 increments character record
  byte24, wraps254->246, updates that member's compact actor tile and stores the
  record. It consumes no action/RNG/round. Existing native formation already reads
  byte24, BattleRecords retains full party records, and world return copies them.
  Repaint through BattlePresentation. No combat rules rewrite.
* W: inventory6 C086,9600–9717. Native source already owns clock, random,
  WorldTransportState.alarm and complete scene lookups. Original1B1B returns0
  on foot,1 for other boats and128 for frigate (all mean boat for this query).
  Without alarm, type>=7 or map44 bypasses danger scan. Otherwise scan visible
  raw actor kinds<2, refusing same water/land domain (terrain229..232), including
  swimming; unlike Camp, W does not simply refuse water. Danger text9739;
  prompt9718 accepts1..9, other keys cancel. Increment BCD hours modulo24,
  retain minute/phase, decrement field20 if nonzero, keep typed counter in sync.
  Every other hour may select one of five authored flavor texts through the
  existing native RNG0..4. At completion clear personal-light ticks/active,
  recompute lighting and redraw. Source FC34/FB6A are clock presentation, not
  imaginary food/party/NPC hourly mutations. Inn/H rest have separate source
  transitions; do not incidentally change them.

Source references are local command_actions.inc, combat_turns.inc,
item-command-6.asm, BS/LWR disassembly and actual v119 resources. They are evidence,
not runtime instruction executors. Current production gaps: recognized Tab/W/=
fallthrough; I displaces torch; tactical = explicitly fails; WorldStatus accepts
only hero246 even though WorldAnimation already supports246..253; a movement
refresh and Open refresh still hardcode246. Retired fixture-only BattleMotion
also hardcodes246 but has no production caller: retain it, do not rewrite it.

## Required behavior inventory (36 known)

1. Tab begins exclusive roster without advancing time/RNG/world actors.
2. Display1..6 occupied members including dead, preserving opaque records.
3. Authored names and P/S status markers, without changing status or selection.
4. HP ratio/seven-segment thresholds, zero and full bars.
5. MP ratio, zero/full and HP-zero suppression.
6. Tab/Space hands off to existing inventory; preserve enhancements/transactions.
7. Other-key close consumes key, restores underlay/text pages and world boundary.
8. I empty-count refusal uses source message17, no invented state mutation/cue.
9. I already-lit behavior preserves count/ticks/active, including count-first order.
10. I success decrements exactly one count, sets120 ticks and active1/message1.
11. Torch refresh immediately uses existing light/visibility/occlusion owners.
12. Torch saves through existing field32/TurnClock schema; no inventory stack guess.
13. Torch leaves other gameplay core, phase/minute/RNG/actor clocks unchanged,
    apart from the required light/visibility and derived scene refresh.
14. World = cycles all8 heroes and wraps.
15. Refuse transport-active icon change without mutating control records.
16. On-foot active/base/status appearance metadata stays consistent.
17. Preserve chosen world hero through boarding/dismount and cold saved controls.
18. All8 heroes use on-foot terrain clipping/occlusion rather than transport rules.
19. Normal movement/Open/refresh retains chosen hero (remove production246 assumptions).
20. Character icons remain independent from the selected world-party hero.
21. Saved legacy/default246 behavior remains valid; no new format needed for icons.
22. Tactical = changes only active member byte24 and matching compact tile.
23. Tactical icon cycle/wrap for all6 member positions.
24. Tactical no action/round/RNG/occupancy change; normal active-turn restrictions.
25. Tactical repaint and subsequent normal record persistence/world return.
26. Tactical icon controls do not bypass active targeting/modal ownership.
27. W danger checks cover boat/swimming/land domains, kinds and safe-map exceptions.
28. W reads existing typed alarm state; do not invent an emulated global byte.
29. W prompt accepts1..9, other-key cancel preserves state and returns to world.
30. W increments exactly requested hours modulo24, retaining minute/phase.
31. W hourly counter saturates at0 and remains equal to world field20.
32. W flavor uses original5 strings and existing RNG range0..4 at defined phase.
33. W clears light only after successful wait, not refusal/cancel.
34. W recomputes light/HUD/viewport and runs the existing input-loop schedule
    service at the final clock, without fake turns or invented per-hour NPC moves.
35. W completion returns directly to world command ownership; no invented final
    acknowledgement, key forwarding or extra turn.
36. Pending roster/wait modal blocks saving; completed commands preserve normal
    CE save/session/assistance/audio ownership and supported cold core/control state.

## Native decisions and invariants

Restore original I while retaining the native inventory via documented Tab/Tab
and Space handoff. This changes a temporary shortcut, not available functionality.
Roster uses the established native font/chrome; saved party data stays lossless.
Source page-swap return affected whether W's first flavor occurred in hour1 or2.
CE has semantic frames rather than that machine-page ABI: define first-hour flavor,
then alternate, matching the original zero-page-parity branch. Disclose this CE
presentation/RNG-phase choice; never call it universal machine-timing parity.
Do not add busy delays or960 synthetic movement turns per wait.

Source safety refinement before validation: the alarm branch jumps directly to
9642, probing terrain at its initial Y before checking actor kind. Cached loader
services can retain the command-table cursor while the native engine has no such
ABI. Define native initial probe0, retain that original alarm-first terrain check,
then scan the full typed actor grid. The source fixture's explicit Y0 context
therefore matches all3600 cases; do not silently replace this with only an actor
scan (366 cases differ). This canonical native input context is an explicit CE
replacement for cache-register coupling, not a universal scratch-parity claim.

Implement typed roster/torch/icon/hour-wait owners, connect world/tactical command
paths, preserve the existing transport base hero and serializer, and stage shared
world/light/frame updates before commit. No new opcode/ABI fallback, silent no-op,
campaign-specific condition or broad test framework. Reuse current validation
entry point and existing native classes. Keep audio473 source/cues/settings.

## Bounded verification

One focused batch for the entire36-behavior contract, one native integration build.
Use bounded original subroutine fixtures for torch/icon/bar/hour transitions,
native production owners and deterministic pre-operation scene/control setup.
Include roster handoff/cancel, all heroes/party positions, source wait refusals/
cancel/time/counter/RNG, relevant native codec/control round trips and static
production caller verification. No campaign traversal, exploratory game launch,
old audio suite or broad completed subsystem reruns. Additional runs only for a
direct changed-code failure; at most two unsuccessful corrective reruns.
