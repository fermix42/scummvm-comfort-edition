# Known magic model and production connection closure519

Status: VERIFIED for static known implementation/connection review only.
Parent249037bdfc0. No code change,build,executable run or old suite.
Current15 reviewed owner headers match frozen518b source hashes exactly.
Reuse499's39functional named player spells,34battle/9world family coverage,
141NPCdescriptors/14overrides andconservative NPCspell domain. No duplicate
inventory/service/weapon/spell rewrite is justified. Inventory510 and services
497/500 remain their existing known implementation closure;do not redo them.

| Known requirement | Complete existing semantic owner and production connection |
|---|---|
| magic.01 caster,slot,cost,cancel | CastRules reads authored64cost/power/threshold descriptors andbook intelligence requirement;WorldCast andBattleCast collect living caster/slot/target and retain modal phase. Empty/refusal/cancel paths precede effect commit;effect owners preserve source-specific spent cancellations,such as outward/invalid NoxVortex direction. |
| magic.02 world restoration | WorldCast49..52 shares BattleRestoration forresurrection,heal,greaterheal/divine;caster andtarget costs/records commit together. WorldInput owns C/open/close andworldrefresh. Living-target resurrection refusal still spends mana as the original transaction does. |
| magic.03 navigation/light/coordinates | WorldCast57..60/63 owns light duration/active state,coordinate message,QuickExit destination/orb selection;WorldNavigation owns9boundrecords/surface-parent anchors,including492's explicit alignedX0replacement. WorldInput's common travel commits destination,terrain/NPC/light/navigation,now also origin-projectile cleanup/population lifetime517. |
| magic.04 directed impact | BattleCast andBattleRound dispatch missile/blast/chain/blade/Nox families into BattleDirectedSpell;target identity/PvP remembers the shared Attack state. SpellDamage owns power/armor RNG,resistance255/percentage immunity,protected suppression,instant kill,HP/death/removal andcredit. These are record transactions,not instruction handlers. |
| magic.05 area andfriendliness | BattleAreaSpell/SpellAreaTargets own source11x11clip/ring order,radius1..3,fireballdecay/outercorners,draws onemptycells,large-leadingquadrants andtargetdedup. Shared impacts use the same affiliation/protected/death/reward policy. BattleRound advances actual resulting state once. |
| magic.06 known status/immunity fields | The functional catalog's actual nondamage fields are armor/backup,engagement/party movement,light counters,resurrection/death andsummon affiliation/placement;the relevant owners above commit them. Resistance/protection are shared impact policies. No separate poison/sleep/fear spell family is evidenced by499's entire named functional dispatch domain;do not invent handlers from a generic matrix label. Additional unknown original script/status producers remain story.08,not silently included here. |
| magic.07 summoned actors | BattleSummon owns allfive standard templates andcount/placement/allocation/conditional RNG;BattleMadCow explicitly replaces invalid forced-placement/stale-slot behavior. BattleRound updates pooled membership andschedules allies/foes byactual affiliation;BattleRoster/removal andterminal aftermath own exits. OriginalHPsnapshot data are retained inround/nativeEncounter serialization. No invented wall-clock duration system is required bythis catalog;exact additional original consumers stay qualified. |
| magic.08 Vortex/portal consumers | NoxVortex is tacticalspell12:BattleDirectedSpell handles source cardinal cone/direction/refusal,shared impact andengagement/mana. The world's authored portal/orb destinations are distinct:WorldPortal/WorldNavigation/WorldTravel,not a missing extra worldVortex engine. Keep the matrix's grouped label fromcreating duplicate functionality. |

NPCcasts use CastRules.prepareNpc's group/power-column andmap32/map8remaps,
thenBattleRound's same directed/area/summon transactions. Existing family census
covers their entire admitted spell domain. BattleCast,EncounterState andNewGame/
WorldInput serializers persist slot/preparedcost,target/selection,records,RNG,
light/orb/navigation outcomes. Host rebinds transient sound sinks andreturns
through shared round/after-battle owners;no second magic-specific game loop.

All8 enumerated magic implementation rows become IMPLEMENTED for this known
source scope. Their verification status is NOT promoted:current full normal
Engine cold/session,earned branches andcampaign are not rerun or claimed.
Existing361/276..285runtime contracts remain tied to their own artifacts.
No new missing code is demonstrated inthis complete known magic pass.
Continue remaining concrete script producer/terminal/shared lifecycle source
connections;audio473 and518b remain accepted. No campaign traversal/acceptance.
