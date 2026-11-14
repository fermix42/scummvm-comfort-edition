# World Dig484

Status: IN PROGRESS. Parent battle-return483 commit9db4aefab6d. Complete the
actual source D command; current recognized-command fallthrough is missing it.
Source GAME726C -> lower1CB1 inventory program2 (file908C,size0267),9765-97C0.

1. Field52 bit8 is the exact shovel prerequisite. Without it print28 and refuse
   with existing cue; no pending prompt, clock/AI/RNG action or invented tool use.
2. With it, source1559 prompt12 requests direction. Arrow near cells76/110/94/92;
   Escape cancels freely. Other keys remain exclusive, no unrelated command.
3. Select actual visible object tile when object lookup exists, otherwise ground.
   Type3 tile91/92 opens actual object state3=41 and refreshes world/cache/frame.
   Type3 tile93 prints27/refusal. Type2 tile<64 or70..73 prints27/refusal.
   Every other type/tile prints18/refusal. Do not invent terrain excavation,
   consumed shovel, item reward, extra actor turn or movement.
4. Original type3 ground91/92 without a mutable object would alias past the object
   page through X255. Native explicitly refuses18 instead of corrupting NPC data.
   This is a CE safety/ownership replacement, not source malformed-context parity.
5. Stage actual object mutation and native viewport/text/cache changes, then
   commit. Source41 is the opening write; the existing visible/unoccupied
   presentation phase settles it to40 during the requested refresh. Persist that
   actual phase, not an injected41 after rendering. Clear the direction prompt on success/cancel; preserve ordinary source
   refusals. Repeated open-state41 handling is idempotent, as the original write.
6. NWIN16 bounded pending-D wrapper over existing codecs. Normal CE save eligibility
   remains idle world/ending; pending codec proof does not enable normal modal save.
7. Existing common input preparation can run before D; pending direction/cancel
   itself preserves core/RNG and clock. No classic machine screen-page ABI.
8. One original predicate/record batch and actual authored coffin WorldInput
   command/cancel/refusal/save/state/viewport check, one integration build. No
   campaign traversal, earned shovel acquisition or accepted audio/old suite.

Source coffin objects91/92 are authored in Ruin19/20/62/31. Predicate comparisons
cover valid object/floor contexts; illegal alias case is separately explicit.
Hidden !950A unlock code and3 diagnostic command are outside normal player
controls; do not invent gameplay/settings meanings for them.
