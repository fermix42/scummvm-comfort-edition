# Tactical controls481

Status: IN PROGRESS. Reuse M4-COMBAT-CONTRACT and existing BattleRound,
EncounterState, BattlePresentation, cannon, attack/cast and native world inventory.
Contract before implementation. World Attack480 is a separate commit.

Source AUXD8EA-D949 dispatch, DDE3-DE22 selection, E3C8 record accessor,
D85D-D87E retained lock admission, DAB1-DB00 round scheduling. Known missing
controls: ! @ # $ % ^ lock member1..6, ) unlock. They are not free character
switches: successful selection returns through DAB1. A nonzero lock forces
remaining party slots to be skipped and runs the ordinary allied/enemy cycle;
subsequent input selects that same member. Unlock retires the current party
position normally. Selection/refusal does not count as an attack/pass action.

1. Decode actual shifted punctuation, including normal shifted number key
   events. Other numeric keys keep their existing meaning.
2. Choices1..6 must be within actual party count and compact status&63 ==0.
   Relevant refusal retains lock/turn/records/RNG and uses established cue.
3. Native unlock0 always clears the lock. Original E3C8 with A0 accesses unused
   compact slot252 (BDFF); do not preserve this incidental scratch dependency.
4. Selected lock survives subsequent ordinary actions, target/cast cancellation
   and native EncounterState save/load. Use a bounded optional wrapper over
   existing versions, not a schema rewrite. Older state initializes no lock.
5. End-of-cycle locked member must still be alive and compact status&63 ==0;
   otherwise clear the lock and resume existing eligible-party policy. Existing
   status1 eligible-for-turn behavior does not make that member lockable.
6. Every accepted selection advances through the existing round owner, with its
   actual allied/enemy effects and RNG. Do not manufacture any expected outcome
   or implement a second AI loop. Existing action counters keep their meaning.
7. Show the retained lock in the tactical roster using native presentation.
8. Source +/- only adjust Apple II text delay (8D2C/8D38); * toggles optional
   original F754 arithmetic prose. CE's current immediate tactical presentation
   and health/mana/status panes replace those machine text modes. Retire their
   fatal missing-action boundary; unmapped keys get the established invalid-key
   response without time/RNG. This is an explicit presentation replacement,
   not an implemented original delay/math display option.
9. F cannon and = icon already have production owners; remove obsolete missing
   labels, preserve their behavior. Ordinary battle Tab inventory is still
    missing (world inventory is not its production owner); preserve its explicit
    boundary and complete that connection as the next combat/menu batch.
10. One focused source transition + retained state/round integration batch, one
    integration build. No battle traversal, campaign, old/audio suite or broad
    test platform. Actual normal Engine cold battle-save acceptance is not
    implied by codec proof; preserve current CE save eligibility.

Quick067D producer lifetime, edge/return exceptional records, spells and script
closure remain independent requirements. Do not conflate them with controls.
