# Native barrier touch488

Status: IN PROGRESS. Parent47d51f09819. Contract before edits.
Original shared walking probeF1D1..F273 tests8 overlapping type/tile windows at
GAME702A..7032. Matching F210..F230 calls F7A3 with damagekind0 and bitindex1.
Source F7AE..F7B7 tests questfield57 mask2 before presentation/damage. Protected
pathF7E6 shows source message32 and returnsY80,so F22E takes immediate allowed
F267. This bypasses later collision ranges after this barrier match. Unprotected
F7B9..F7E4 shows33,usesdamageGAME6155 for each actual party member,thenF230 takes
blockedF265. Normal blockedF156 displays10/impact and resumesECE6. Do not move
coordinates or invoke arbitrary scripts to apply this synchronous touch.

Existing TraversalRules intentionally returns requiresInteraction. WorldMove
and mounted walking currently abort at this boundary. WorldHazards already owns
source kind0/protectionmask2 energy damage,party damage/death and messages; add
typed barrier entry there and reuse it from motion. No new CPU/ABI/RNG owner.

1. On foot:resolve actual admitted probe before move;unprotected commits only
   touch damage/presentation and returns existing blocked result (normal caller
   owns its already-existing blocked actor/clock continuation). Protected touch
   permits that admitted position without guessing later terrain/range rules.
2. Mounted walking:typed context carries real field57mask2;horse/hound near/far
   probe reports barrier touch and authorization. Existing water/jump/actor/
   object precedence remains. Touch commits once through same hazard owner;
   protected movement continues,unprotected returns blocked. Water-only ship/
   boat movement has no invented walking/barrier behavior.
3. Death returns existing defeat owner before a move/ordinary actor phase.
   Stage party/text/frame/state;keep world/window/RNG unchanged before blocked
   commit except prior genuine transport-probe RNG. No new persisted phase.
4. One focused batch:48 direct actual-party source mask/damage/death cases over
   supported types2/4/7/8,48 mounted near barrier probes,actual authored Keep73
   barrier12,26 unprotected/protected WorldMove admissions plus core roundtrip.
   Setup flag57,source legal adjacent floor,initial party before operation.
   Protected actual subsequent actor/encounter result is reported separately;
   no injected outcome,earned traversal,campaign,audio/old hazards suite or
   normal Engine cold/session claim. Source sound event reused,not audio change.

Actual source barriers include Castle58 at29,21/28,22/27,23 and Keep73 around
12..14,26..28. Local source census only,not physical reachability acceptance.

Static prerequisite correction before native batch:source kind0 damage71 is
lethal to new-game HP50. Declare controlled HP/maxHP100 through public party
record import before each test;low-health direct cases reduce100->1 before
touch. This covers nonfatal blocked touch and lethal touch distinctly. Actual
Keep73 operations startHP100 before input;no tested outcome is injected.
