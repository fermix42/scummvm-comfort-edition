/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_QUICK_BATTLE_H
#define NOX_NATIVE_QUICK_BATTLE_H
#include "nox/native_battle_physical.h"
#include "nox/native_battle_spell_damage.h"
#include "nox/native_battle_terminal.h"
namespace Nox { namespace Native {
struct QuickBattleState {
    byte partyCount,enemyCount,side,cursor,phase,category;
    byte living[22],enemyMetadata[32],partyLevels[6];
    uint32 actions,rounds;
    QuickBattleState() : partyCount(0),enemyCount(0),side(0),cursor(0),phase(0),category(0),actions(0),rounds(0) {
        memset(living,1,22);memset(enemyMetadata,0,32);memset(partyLevels,0,6);
    }
    bool valid() const {
        if (!partyCount || partyCount>6 || !enemyCount || enemyCount>16 || (side!=0 && side!=2) || cursor>=(side?enemyCount:partyCount) || phase>4) return false;
        for (uint i=0;i<22;++i) if (living[i]!=1 && ((i<6 && (living[i]&3 || living[i]>=partyCount*4)) || (i>=6 && (living[i]&7 || living[i]>=enemyCount*8)))) return false;
        return true;
    }
    bool save(Common::WriteStream &out) const {
        if (!valid()) return false;
        out.writeUint32BE(MKTAG('N','Q','B','S'));out.writeUint16LE(1);
        out.writeByte(partyCount);out.writeByte(enemyCount);out.writeByte(side);out.writeByte(cursor);out.writeByte(phase);out.writeByte(category);
        out.write(living,22);out.write(enemyMetadata,32);out.write(partyLevels,6);out.writeUint32LE(actions);out.writeUint32LE(rounds);return !out.err();
    }
    bool load(Common::ReadStream &in) {
        if (in.readUint32BE()!=MKTAG('N','Q','B','S') || in.readUint16LE()!=1) return false;
        QuickBattleState next;next.partyCount=in.readByte();next.enemyCount=in.readByte();next.side=in.readByte();next.cursor=in.readByte();next.phase=in.readByte();next.category=in.readByte();
        if (in.read(next.living,22)!=22 || in.read(next.enemyMetadata,32)!=32 || in.read(next.partyLevels,6)!=6) return false;
        next.actions=in.readUint32LE();next.rounds=in.readUint32LE();if (in.err() || !next.valid()) return false;*this=next;return true;
    }
};
// Quick Combat is its own ordered policy over shared semantic effects. The
// temporary level/status interpretation has an explicit lifetime;no CPU state
// or overlaid executable data is retained by this owner.
class QuickBattle {
public:
    bool load(HdvArchive &archive) {
        Common::Array<byte> data;
        if (!_physical.load(archive) || !_damage.load(archive) ||
            !archive.readRange("NA/BS.AUX.RTN.BK1",0x574,40,data)) return false;
        memcpy(_policy,data.data(),18);memcpy(_power,data.data()+18,22);return true;
    }
    static bool prepare(byte party,byte enemies,byte category,bool boostEnemy,bool preserveEnemyLevel,BattleRecords &records,QuickBattleState &state) {
        if (!party || party>6 || !enemies || enemies>16) return false;
        BattleRecords next=records;QuickBattleState prepared;prepared.partyCount=party;prepared.enemyCount=enemies;prepared.category=category;
        uint living=0;
        for (uint i=0;i<party;++i) {
            byte *member=next.stats+i*128;prepared.partyLevels[i]=member[1];
            if ((member[21]&15)>=2 || (member[21]>>4)>=2) member[1]=MIN<byte>(10,byte(member[1]+1));
            if (next.compact[i*4+3]!=255 && next.compact[i*4+2]) prepared.living[living++]=byte(i*4);
        }
        for (uint i=0;i<enemies;++i) {
            byte *member=next.stats+768+i*32;prepared.enemyMetadata[i*2]=member[0];prepared.enemyMetadata[i*2+1]=member[1];member[0]=member[1];
            if (boostEnemy) {
                if (!member[4]) { member[1]=byte(member[1]+3);if (enemies>=7) goto preparedLevel; }
                member[1]=byte(member[1]+2);
            }
preparedLevel:
            if (enemies!=1) {
                if (member[1]<3) {
                    member[2]=byte(member[2]-member[2]/4);
                    if (!member[2] && !preserveEnemyLevel) member[1]=byte(member[1]-2);
                } else if (!preserveEnemyLevel) member[1]=byte(member[1]-2);
                member[1]=MIN<byte>(member[1],10);
            }
            prepared.living[6+i]=byte(i*8);
        }
        records=next;state=prepared;return true;
    }
    static bool restore(BattleRecords &records,const QuickBattleState &state) {
        if (!state.partyCount || state.partyCount>6 || !state.enemyCount || state.enemyCount>16) return false;
        for (uint i=0;i<state.partyCount;++i) records.stats[i*128+1]=state.partyLevels[i];
        for (uint i=0;i<state.enemyCount;++i) memcpy(records.stats+768+i*32,state.enemyMetadata+i*2,2);
        return true;
    }
    bool step(GameplayRandom &random,BattleRecords &records,QuickBattleState &state,bool protectedTargets=false) const {
        if (state.phase || !state.partyCount || !state.enemyCount || state.side>2 || state.side==1) return false;
        QuickBattleState next=state;BattleRecords battle=records;GameplayRandom rng=random;
        const uint count=next.side?next.enemyCount:next.partyCount;
        if (next.cursor>=count) return false;
        const uint member=next.cursor,index=member*(next.side?8:4);
        const bool skipped=next.side?(battle.extended[index+7]&128)!=0:battle.compact[index+3]==255;
        if (!skipped) {
            const byte cell=actorCell(battle,next.side,byte(index));VortexVictim caster;if (!battle.actor(cell,caster)) return false;
            const uint ownBase=_policy[3+next.side];uint ordinal=0;
            for (uint i=0;i<(next.side?16U:6U);++i) if (next.living[ownBase+i]==index) { ordinal=i+1;break; }
            if (!ordinal) return false;
            const bool rear=ordinal>=_policy[next.side];byte selected=0,entry=255,targetCell=255;
            if (!select(next.side,rear?1:0,next,rng,battle,entry,targetCell)) return false;
            bool magic=next.side?!rear:false;
            if (!next.side && rear) { if (!rng.next(1,255,selected)) return false;magic=(selected&128)!=0; }
            bool killed=false;
            if (magic) {
                const byte level=caster.stats[1]&127;if (!level || level>10) return false;
                const byte power=_power[level*2],cost=_power[level*2+1];
                const byte mana=next.side?byte(MIN<uint>(255,uint(caster.stats[4])*2)):caster.stats[4];
                if (mana>=cost) {
                    caster.stats[4]=byte(mana-cost);if (!battle.store(caster)) return false;
                    SpellDamageIntent intent;intent.casterCell=cell;intent.targetCell=targetCell;intent.category=next.category;
                    // Admission retains selected member1 throughout Q. The
                    // original shared credit helper reads that level even
                    // when another party member performs the Quick action.
                    intent.basePower=power;intent.threshold=level;intent.quick=true;intent.quickPartyLevel=next.partyLevels[0];intent.protectedTargets=protectedTargets;
                    SpellDamageOutcome result;if (!_damage.resolve(intent,rng,battle,result)) return false;killed=result.killed;
                } else magic=false;
            }
            if (!magic) {
                const uint hands=next.side?1:2;
                for (uint hand=0;hand<hands;++hand) {
                    if (!next.side && ((hand?caster.stats[21]&15:caster.stats[21]>>4)==1)) continue;
                    PhysicalIntent intent;intent.casterCell=cell;intent.targetCell=targetCell;intent.category=next.category;
                    intent.offHand=hand!=0;intent.quick=true;intent.quickRear=rear;intent.quickPartyLevel=next.partyLevels[0];intent.protectedTargets=protectedTargets;
                    PhysicalOutcome result;if (!_physical.resolve(intent,rng,battle,result)) return false;
                    if (result.killed) { killed=true;break; }
                }
            }
            ++next.actions;
            if (killed) next.living[entry]=1;
            if (BattleTerminal::defeated(battle,next.partyCount)) next.phase=2;
            bool enemyAlive=false;for (uint i=6;i<22;++i) if (next.living[i]!=1) enemyAlive=true;
            if (!enemyAlive) next.phase=1;
            if (!next.phase) compact(next.side,next);
        }
        if (++next.cursor==count) { next.cursor=0;next.side=byte(next.side^2);if (!next.side) ++next.rounds; }
        records=battle;random=rng;state=next;return true;
    }
private:
    static byte actorCell(const BattleRecords &battle,byte side,byte index) {
        const byte *actor=(side?battle.extended:battle.compact)+index;return byte(actor[1]*17+actor[0]);
    }
    bool select(byte side,byte segment,const QuickBattleState &state,GameplayRandom &rng,const BattleRecords &battle,byte &entry,byte &cell) const {
        for (uint fallback=0;fallback<2;++fallback) {
            const uint key=side+segment,base=_policy[7+key],length=_policy[11+key];byte draw;
            if (!rng.next(1,255,draw)) return false;
            byte start=draw>>6;
            if (!base) { start=byte(start*2-2);if (start&128) { start=byte(start*4);if (start>=11) start=0; } }
            else if (base==1) { start=byte(start*4);if (start>=11) start=0; }
            else { start=byte(start-1);if (!start) start=0; }
            for (uint i=0;i<length;++i) {
                ++start;if (start>=length) start=0;
                const byte value=state.living[base+start];if (value==1) continue;
                entry=byte(base+start);cell=actorCell(battle,side?0:2,value);return true;
            }
            if (!segment) return false;segment=0;
        }
        return false;
    }
    void compact(byte side,QuickBattleState &state) const {
        const uint front=_policy[7+side],frontLength=_policy[11+side],rear=_policy[8+side],rearLength=_policy[12+side];
        uint hole=0;for (;hole<frontLength && state.living[front+hole]!=1;++hole) {}
        if (hole==frontLength) return;
        for (uint i=0;i<rearLength;++i) if (state.living[rear+i]!=1) { state.living[front+hole]=state.living[rear+i];state.living[rear+i]=1;return; }
    }
    byte _policy[18],_power[22];BattlePhysical _physical;BattleSpellDamage _damage;
};
} }
#endif
