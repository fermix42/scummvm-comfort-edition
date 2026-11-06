/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_SUMMON_H
#define NOX_NATIVE_BATTLE_SUMMON_H
#include "nox/native_battle_spell_damage.h"
#include "nox/native_battle_summon_outcome.h"
#include "nox/native_battle_madcow.h"
namespace Nox { namespace Native {
// Standard summon family transaction. Pool records, actor placement and original
// HP snapshots have separate ownership; allocation may succeed without placement.
// Special replacement ID18 delegates to its separate transactional owner.
class BattleSummon {
public:
    BattleSummon() : _ready(false) {}
    bool load(HdvArchive &archive) {
        Common::Array<byte> data;if (!archive.readRange("NA/SRTN.COMBAT.S",0x6733,0x986,data)) return false;
        const uint offsets[]={0x205,0x225,0x245,0x1e5,0x265};
        for (uint i=0;i<5;++i) memcpy(_templates[i],data.data()+offsets[i],32);
        memcpy(_friendly,data.data()+0x4c3,31);memcpy(_hostile,data.data()+0x4a4,31);
        if (_friendly[30]!=255 || _hostile[30]!=255 || !_madCow.load(archive)) return false;_ready=true;return true;
    }
    bool resolve(byte spell,byte cost,byte casterCell,const byte *terrain,uint terrainSize,const byte *objects,uint objectSize,
            const byte *blocking,uint blockingSize,byte *originalHealth,uint healthSize,GameplayRandom &random,
            BattleRecords &records,SummonOutcome &outcome,bool protectedTargets=false) const {
        if (spell==18) return _madCow.resolve(cost,casterCell,terrain,terrainSize,objects,objectSize,blocking,blockingSize,originalHealth,healthSize,protectedTargets,random,records,outcome);
        const int choice=spell==14?0:spell==16?1:spell==17?2:spell==19?3:spell==20?4:-1;
        VortexVictim caster;if (!_ready || choice<0 || !terrain || terrainSize!=187 || !objects || objectSize!=187 ||
            !blocking || blockingSize!=4 || !originalHealth || healthSize!=32 || !records.actor(casterCell,caster) || caster.stats[4]<cost) return false;
        BattleRecords next=records;GameplayRandom rng=random;byte health[32];memcpy(health,originalHealth,32);SummonOutcome result;
        result.requested=choice?1:MAX<uint>(1,MIN<uint>(6,caster.stats[1]&127));
        const byte shape=byte(choice==2 || choice==3?9:1),sprite[]={233,243,225,234,252};
        const byte affiliation=caster.kind==3?3:caster.actor[4],opponent=caster.kind==3?1:caster.actor[5];
        // MP commits before placement/allocation, including a full-pool refusal.
        caster.stats[4]-=cost;if (!next.store(caster)) return false;
        for (uint attempt=0;attempt<result.requested;++attempt) {
            byte cell=255;const byte *preferred=affiliation==3?_friendly:_hostile;
            for (uint n=0;n<30;++n) if (available(next,preferred[n],shape,terrain,objects,blocking)) { cell=preferred[n];break; }
            if (cell==255) {
                for (uint retry=0;retry<256;++retry) {
                    byte x,y;if (!rng.next(1,9,x) || !rng.next(1,9,y)) return false;cell=byte(y*17+x);
                    if (available(next,cell,shape,terrain,objects,blocking)) break;
                }
            }
            uint slot=0;for (;slot<16;++slot) {
                const byte *record=next.stats+1280+slot*32;
                if (record[0]==255 || !READ_LE_UINT16(record+2)) break;
            }
            if (slot==16) { ++result.refused;continue; }
            memcpy(next.stats+1280+slot*32,_templates[choice],32);memcpy(health+slot*2,_templates[choice]+2,2);++result.allocated;
            if (!available(next,cell,shape,terrain,objects,blocking)) { ++result.failedPlacement;continue; }
            const byte index=byte(128+slot*8);byte *actor=next.extended+index;
            actor[0]=cell%17;actor[1]=cell/17;actor[2]=sprite[choice];actor[3]=shape;
            actor[4]=affiliation;actor[5]=opponent;actor[7]=0;
            const uint delta[]={0,1,17,18};const byte flags[]={64,32,16,8};
            for (uint foot=0;foot<(shape==9?4U:1U);++foot) {
                next.indices[cell+delta[foot]]=index;next.kinds[cell+delta[foot]]=byte(2|(shape==9?flags[foot]:0));
            }
            ++result.placed;result.slots.push_back(index);
        }
        // Reload: future callers may cast from the same pooled record bank.
        if (!next.actor(casterCell,caster)) return false;caster.stats[12]=255;if (!next.store(caster)) return false;
        result.spentMana=true;records=next;random=rng;memcpy(originalHealth,health,32);outcome=result;return true;
    }
private:
    static bool available(const BattleRecords &records,byte cell,byte shape,const byte *terrain,const byte *objects,const byte *ranges) {
        const uint delta[]={0,1,17,18};
        for (uint n=0;n<(shape==9?4U:1U);++n) {
            const uint at=uint(cell)+delta[n];if (at>=187 || records.indices[at]!=255 || objects[at]!=255) return false;
            const byte tile=terrain[at];if ((tile>=ranges[0] && tile<ranges[1]) || (tile>=ranges[2] && tile<ranges[3])) return false;
        }
        return true;
    }
    bool _ready;byte _templates[5][32],_friendly[31],_hostile[31];BattleMadCow _madCow;
};
} }
#endif
