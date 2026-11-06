/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_ROUND_H
#define NOX_NATIVE_BATTLE_ROUND_H
#include "nox/native_battle_physical.h"
#include "nox/native_actor_pursuit.h"
#include "nox/native_battle_targets.h"
#include "nox/native_actor_step.h"
#include "nox/native_battle_restoration.h"
#include "nox/native_battle_area_spell.h"
#include "nox/native_battle_directed_spell.h"
#include "nox/native_battle_summon.h"
#include "nox/native_battle_npc_decision.h"
#include "nox/native_battle_collisions.h"
#include "nox/native_battle_terminal.h"
namespace Nox { namespace Native {
enum BattleRoundPhase { kBattlePartyTurn,kBattleVictory,kBattleDefeat,kBattleAutomaticTurn };
struct BattleRoundState {
    byte partyCount,enemyCount,allyCount,activeParty,category,mapId,enemySpell,summonHealth[32];
    bool protectedTargets,specialSpell;
    BattleRoundPhase phase;
    uint32 rounds,actions;
    BattleRoundState() : partyCount(1),enemyCount(1),allyCount(0),activeParty(0),category(0),mapId(7),enemySpell(0),protectedTargets(false),specialSpell(false),phase(kBattlePartyTurn),rounds(0),actions(0) { memset(summonHealth,0,32); }
};
// Native party -> allied summons -> enemy scheduling. The admitted ground
// destination policy uses the parent family's authored terrain classes under
// tactical rules. Large/spell/ship-specific turn policies remain outstanding.
class BattleRound {
public:
    BattleRound() : _projectileMode(false) {}
    void setProjectileMode(bool value) {_projectileMode=value;}
    static byte firstEligibleParty(const BattleRecords &records,byte count) {
        if (!count || count>6) return 255;
        for (byte i=0;i<count;++i) if (aliveParty(records,i) && (records.compact[i*4+3]&63)<2) return i;
        return 255;
    }
    static void initializeTurn(const BattleRecords &records,BattleRoundState &state) {
        const byte selected=firstEligibleParty(records,state.partyCount);
        if (BattleTerminal::defeated(records,state.partyCount)) {state.phase=kBattleDefeat;return;}
        if (selected==255) {state.activeParty=byte(state.partyCount-1);state.phase=kBattleAutomaticTurn;return;}
        state.activeParty=selected;state.phase=phase(state,records);
    }
    bool automatic(const byte *terrain,uint length,BattleRoundState &state,GameplayRandom &random,BattleRecords &records) const {
        if (!valid(state,terrain,length) || state.phase!=kBattleAutomaticTurn || firstEligibleParty(records,state.partyCount)!=255) return false;
        BattleRoundState next=state;BattleRecords battle=records;GameplayRandom rng=random;
        next.activeParty=byte(next.partyCount-1);
        if (!finish(next,rng,battle,terrain)) return false;
        state=next;records=battle;random=rng;return true;
    }
    bool load(HdvArchive &archive) {
        if (!_physical.load(archive) || !_restoration.load(archive) || !_area.load(archive) || !_directed.load(archive) || !_summon.load(archive) || !_casting.load(archive) || !_collisions.load(archive)) return false;
        return true;
    }
    bool move(byte direction,const byte *terrain,uint length,BattleRoundState &state,GameplayRandom &random,BattleRecords &records,bool &moved) const {
        moved=false;if (!valid(state,terrain,length) || state.phase==kBattleDefeat || direction>3) return false;
        const uint index=state.activeParty*4;const int dx[]={0,0,1,-1},dy[]={-1,1,0,0};
        BattleRoundState next=state;BattleRecords battle=records;GameplayRandom rng=random;
        const byte cell=byte(battle.compact[index+1]*17+battle.compact[index]);byte blocked[4];
        if (!collisions(next,battle,cell,terrain,blocked)) return false;
        // Movement clears the prior engagement before the source collision
        // refusal. A refused attempt is free but does not roll that field back.
        battle.stats[next.activeParty*128+12]=255;
        if (blocked[direction]) { records=battle;return true; }
        const int x=int(battle.compact[index])+dx[direction],y=int(battle.compact[index+1])+dy[direction];
        battle.compact[index]=byte(x);battle.compact[index+1]=byte(y);
        if (!BattleRoster::rebuild(battle,next.partyCount,next.enemyCount,next.allyCount)) return false;
        ++next.actions;if (!finish(next,rng,battle,terrain)) return false;
        state=next;random=rng;records=battle;moved=true;return true;
    }
    bool attack(byte target,const byte *terrain,uint length,BattleRoundState &state,GameplayRandom &random,
            BattleRecords &records,PhysicalOutcome &outcome) const {
        if (!valid(state,terrain,length) || state.phase==kBattleDefeat) return false;
        BattleRoundState next=state;BattleRecords battle=records;GameplayRandom rng=random;PhysicalOutcome result;
        const byte *member=battle.stats+next.activeParty*128;
        if ((member[21]&15)!=1 && member[10]) return false;
        if (!attackHand(target,false,terrain,length,next,rng,battle,result) || !finish(next,rng,battle,terrain)) return false;
        state=next;random=rng;records=battle;outcome=result;return true;
    }
    bool attackHand(byte target,bool offHand,const byte *terrain,uint length,BattleRoundState &state,GameplayRandom &random,
            BattleRecords &records,PhysicalOutcome &outcome) const {
        if (!valid(state,terrain,length) || state.phase==kBattleDefeat) return false;
        BattleRoundState next=state;BattleRecords battle=records;GameplayRandom rng=random;
        const uint index=next.activeParty*4;
        const byte caster=byte(battle.compact[index+1]*17+battle.compact[index]);
        VortexVictim attacker;byte kind,range;bool allowed;
        if (!battle.actor(caster,attacker) || !BattleTargets::hand(attacker,offHand,kind,range) ||
            !BattleTargets::permitted(battle,caster,target,range,true,allowed) || !allowed) return false;
        PhysicalIntent intent;intent.casterCell=caster;intent.targetCell=target;intent.category=next.category;intent.protectedTargets=next.protectedTargets;
        intent.offHand=offHand;intent.exceptionalWeapon=(attacker.stats[20]&(offHand?64:128))!=0;intent.projectileMode=_projectileMode;
        PhysicalOutcome result;if (!_physical.resolve(intent,rng,battle,result)) return false;
        ++next.actions;next.phase=phase(next,battle);
        state=next;random=rng;records=battle;outcome=result;return true;
    }
    bool retire(const byte *terrain,uint length,BattleRoundState &state,GameplayRandom &random,BattleRecords &records) const {
        if (!valid(state,terrain,length)) return false;
        BattleRoundState next=state;BattleRecords battle=records;GameplayRandom rng=random;
        if (!finish(next,rng,battle,terrain)) return false;state=next;random=rng;records=battle;return true;
    }
    bool pass(const byte *terrain,uint length,BattleRoundState &state,GameplayRandom &random,BattleRecords &records) const {
        if (!valid(state,terrain,length) || state.phase==kBattleDefeat) return false;
        BattleRoundState next=state;BattleRecords battle=records;GameplayRandom rng=random;++next.actions;
        if (!finish(next,rng,battle,terrain)) return false;state=next;random=rng;records=battle;return true;
    }
    bool restore(byte spell,byte cost,byte target,bool cancelled,const byte *terrain,uint length,const byte *objects,uint objectSize,
            byte engagement,byte *armorBackup,uint backupSize,BattleRoundState &state,GameplayRandom &random,
            BattleRecords &records,RestorationOutcome &outcome) const {
        if (!valid(state,terrain,length) || state.phase==kBattleDefeat || !armorBackup || backupSize!=6) return false;
        BattleRoundState next=state;BattleRecords battle=records;GameplayRandom rng=random;RestorationOutcome result;
        byte backups[6];memcpy(backups,armorBackup,6);
        if (!_restoration.resolve(spell,cost,next.activeParty,target,cancelled,next.partyCount,next.category,terrain,length,
                objects,objectSize,engagement,backups,6,battle,result)) return false;
        if (!result.cancelled) { ++next.actions;if (!finish(next,rng,battle,terrain)) return false; }
        state=next;random=rng;records=battle;memcpy(armorBackup,backups,6);outcome=result;return true;
    }
    bool area(const CastPreparation &prepared,byte center,bool cancelled,const byte *terrain,uint length,
            BattleRoundState &state,GameplayRandom &random,BattleRecords &records,AreaSpellOutcome &outcome) const {
        if (!valid(state,terrain,length) || state.phase==kBattleDefeat) return false;
        BattleRoundState next=state;BattleRecords battle=records;GameplayRandom rng=random;AreaSpellOutcome result;
        const uint index=next.activeParty*4;const byte cell=byte(battle.compact[index+1]*17+battle.compact[index]);
        if (!_area.resolve(prepared,cell,center,cancelled,next.category,next.protectedTargets,rng,battle,result)) return false;
        if (!result.cancelled) { ++next.actions;if (!finish(next,rng,battle,terrain)) return false; }
        state=next;random=rng;records=battle;outcome=result;return true;
    }
    bool directed(const CastPreparation &prepared,const DirectedSpellSelection &selection,const byte *terrain,uint length,
            BattleRoundState &state,GameplayRandom &random,BattleRecords &records,DirectedSpellOutcome &outcome) const {
        if (!valid(state,terrain,length) || state.phase==kBattleDefeat) return false;
        BattleRoundState next=state;BattleRecords battle=records;GameplayRandom rng=random;DirectedSpellOutcome result;
        const uint index=next.activeParty*4;const byte cell=byte(battle.compact[index+1]*17+battle.compact[index]);
        if (!_directed.resolve(prepared,cell,selection,next.category,next.protectedTargets,rng,battle,result)) return false;
        if (!result.cancelled) { ++next.actions;if (!finish(next,rng,battle,terrain)) return false; }
        state=next;random=rng;records=battle;outcome=result;return true;
    }
    bool summon(const CastPreparation &prepared,const byte *terrain,uint length,BattleRoundState &state,GameplayRandom &random,
            BattleRecords &records,SummonOutcome &outcome) const {
        if (!valid(state,terrain,length) || state.phase==kBattleDefeat) return false;
        BattleRoundState next=state;BattleRecords battle=records;GameplayRandom rng=random;SummonOutcome result;
        const uint index=next.activeParty*4;const byte cell=byte(battle.compact[index+1]*17+battle.compact[index]);
        if (!summonEffect(prepared,cell,terrain,next,rng,battle,result)) return false;
        ++next.actions;if (!finish(next,rng,battle,terrain)) return false;
        state=next;random=rng;records=battle;outcome=result;return true;
    }

private:
    static bool valid(const BattleRoundState &s,const byte *terrain,uint length) {
        if (!terrain || length!=187 || !s.partyCount || s.partyCount>6 || s.enemyCount>16 || s.allyCount>16 || s.activeParty>=s.partyCount) return false;
        return s.category<=8 && s.category!=1;
    }
    bool collisions(const BattleRoundState &s,const BattleRecords &b,byte cell,const byte *terrain,byte *blocked) const {
        VortexVictim actor;if (!b.actor(cell,actor)) return false;
        return _collisions.probe(s.category,actor.actor[0],actor.actor[1],actor.actor[2],actor.kind!=3 && (actor.actor[3]&8),
            actor.kind!=3 && (actor.actor[3]&32),terrain,b.indices,nullptr,nullptr,blocked);
    }
    static bool aliveParty(const BattleRecords &b,uint i) { return READ_LE_UINT16(b.stats+i*128+2) && b.stats[i*128]!=255; }
    static bool aliveOther(const BattleRecords &b,uint i,bool ally) {
        return READ_LE_UINT16(b.stats+(ally?1280:768)+i*32+2) && b.extended[(ally?128:0)+i*8+2] && b.extended[(ally?128:0)+i*8+3]!=255;
    }
    static BattleRoundPhase phase(const BattleRoundState &s,const BattleRecords &b) {
        return BattleTerminal::defeated(b,s.partyCount)?kBattleDefeat:!BattleTerminal::hostiles(b)?kBattleVictory:kBattlePartyTurn;
    }
    bool summonEffect(const CastPreparation &prepared,byte cell,const byte *terrain,BattleRoundState &s,GameplayRandom &rng,
            BattleRecords &records,SummonOutcome &outcome) const {
        byte objects[187];memset(objects,255,187);const byte *blocking=_restoration.blocking(s.category);
        if (!blocking || !_summon.resolve(prepared.spell,prepared.manaCost,cell,terrain,187,objects,187,blocking,4,s.summonHealth,32,rng,records,outcome,s.protectedTargets)) return false;
        for (uint i=0;i<outcome.slots.size();++i) s.allyCount=MAX<byte>(s.allyCount,byte((outcome.slots[i]-128)/8+1));
        return true;
    }
    bool npcCast(byte cell,byte target,const byte *terrain,BattleRoundState &s,GameplayRandom &rng,BattleRecords &records) const {
        VortexVictim caster;if (!records.actor(cell,caster) || caster.kind==3) return false;
        NpcCastPreparation prepared;byte mana=caster.stats[4];
        if (!_casting.prepareNpc(caster.kind==2?1:2,s.enemySpell,caster.stats[15]&15,caster.stats[5],s.mapId,s.specialSpell,mana,prepared)) return false;
        caster.stats[4]=mana;if (!records.store(caster)) return false;const byte spell=prepared.cast.spell;
        if (BattleDirectedSpell::supports(spell)) {
            DirectedSpellSelection selection;selection.cell=target;DirectedSpellOutcome outcome;
            return _directed.resolve(prepared.cast,cell,selection,s.category,s.protectedTargets,rng,records,outcome);
        }
        if ((spell>=2 && spell<=4) || (spell>=27 && spell<=29)) {
            AreaSpellOutcome outcome;return _area.resolve(prepared.cast,cell,target,false,s.category,s.protectedTargets,rng,records,outcome);
        }
        if (spell==14 || spell==16 || spell==17 || spell==18 || spell==19 || spell==20) {
            SummonOutcome outcome;return summonEffect(prepared.cast,cell,terrain,s,rng,records,outcome);
        }
        return false; // Non-authored combat/world caller branches remain explicit.
    }
    bool finish(BattleRoundState &s,GameplayRandom &rng,BattleRecords &b,const byte *terrain) const {
        s.phase=phase(s,b);if (s.phase==kBattleDefeat) return true;
        for (uint i=s.activeParty+1;i<s.partyCount;++i) if (aliveParty(b,i) && (b.compact[i*4+3]&63)<2) { s.activeParty=byte(i);return true; }
        for (uint side=0;side<2;++side) {
            const bool ally=side==0;
            for (uint i=0;i<(ally?s.allyCount:s.enemyCount);++i) {
                const uint index=(ally?128:0)+i*8;
                if (!aliveOther(b,i,ally) || b.extended[index+7]>=2) continue;
                const byte cell=byte(b.extended[index+1]*17+b.extended[index]);VortexVictim caster;
                if (!b.actor(cell,caster)) return false;NpcBattleDecision decision;
                if (!BattleNpcDecision::choose(cell,rng,b,decision)) return false;
                if (decision.action==kNpcCast) {
                    if (!npcCast(cell,decision.target,terrain,s,rng,b)) return false;
                } else if (decision.action==kNpcPhysical) {
                    PhysicalIntent intent;intent.casterCell=cell;intent.targetCell=decision.target;intent.category=s.category;intent.protectedTargets=s.protectedTargets;intent.projectileMode=_projectileMode;
                    PhysicalOutcome ignored;if (!_physical.resolve(intent,rng,b,ignored)) return false;
                } else if (decision.target!=255) {
                    const int dx[]={0,0,1,-1},dy[]={-1,1,0,0};byte blocked[4];
                    if (!collisions(s,b,cell,terrain,blocked)) return false;
                    ActorPursuitChoice choice;const byte relativeX=byte(128+int(caster.actor[0])-int(decision.target%17));
                    const byte relativeY=byte(128+int(caster.actor[1])-int(decision.target/17));
                    if (!ActorPursuitRules::choose(relativeX,relativeY,kPursuitPreferredThenOtherAxes,blocked,rng,choice)) return false;
                    if (choice.direction<4) {
                        b.extended[index]=byte(int(caster.actor[0])+dx[choice.direction]);b.extended[index+1]=byte(int(caster.actor[1])+dy[choice.direction]);
                        if (!BattleRoster::rebuild(b,s.partyCount,s.enemyCount,s.allyCount)) return false;
                    }
                }
                ++s.actions;s.phase=phase(s,b);if (s.phase==kBattleDefeat) return true;
            }
        }
        ++s.rounds;
        for (uint i=0;i<s.partyCount;++i) if (aliveParty(b,i) && (b.compact[i*4+3]&63)<2) { s.activeParty=byte(i);return true; }
        s.activeParty=byte(s.partyCount-1);s.phase=kBattleAutomaticTurn;return true; // No eligible party input;retain the actual next automatic cycle.
    }
    bool _projectileMode;BattlePhysical _physical;
    BattleRestoration _restoration;
    BattleAreaSpell _area;
    BattleDirectedSpell _directed;
    BattleSummon _summon;CastRules _casting;BattleCollisions _collisions;
};
} }
#endif
