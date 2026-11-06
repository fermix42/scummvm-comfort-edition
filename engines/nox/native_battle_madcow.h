/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_MADCOW_H
#define NOX_NATIVE_BATTLE_MADCOW_H
#include "nox/native_battle_spell_damage.h"
#include "nox/native_battle_summon_outcome.h"
namespace Nox { namespace Native {
// Mad Cow is a replacement transaction,not eight ordinary summons or damage.
// Honor allocator refusal and search exhaustion instead of the source caller's
// stale-index/forced-placement writes,which can violate actor/pool invariants.
class BattleMadCow {
public:
    BattleMadCow() : _ready(false) {}
    bool load(HdvArchive &archive) {
        Common::Array<byte> data;if (!archive.readRange("NA/SRTN.COMBAT.S",0x680c,32,data)) return false;
        memcpy(_record,data.data(),32);_ready=true;return true;
    }
    bool resolve(byte cost,byte cell,const byte *terrain,uint terrainSize,const byte *objects,uint objectSize,const byte *blocking,uint blockingSize,
            byte *health,uint healthSize,bool protectedTargets,GameplayRandom &random,BattleRecords &records,SummonOutcome &output) const {
        VortexVictim caster;if (!_ready || !terrain || terrainSize!=187 || !objects || objectSize!=187 || !blocking || blockingSize!=4 ||
            !health || healthSize!=32 || !records.actor(cell,caster) || caster.stats[4]<cost) return false;
        BattleRecords next=records;GameplayRandom rng=random;byte snapshots[32];memcpy(snapshots,health,32);SummonOutcome result;result.requested=8;
        const bool protectedHit=protectedTargets && caster.kind!=1;
        caster.stats[4]-=cost;if (!next.store(caster)) return false;
        for (uint attempt=0;attempt<8;++attempt) {
            uint slot=0;for (;slot<16;++slot) {
                const byte *record=next.stats+1280+slot*32;if (record[0]==255 || !READ_LE_UINT16(record+2)) break;
            }
            if (slot==16) { ++result.refused;continue; }
            memcpy(next.stats+1280+slot*32,_record,32);memcpy(snapshots+slot*2,_record+2,2);++result.allocated;
            byte target=255;bool found=false;
            for (uint retry=0;retry<256-attempt;++retry) {
                byte x,y;if (!rng.next(1,10,x) || !rng.next(1,10,y)) return false;target=byte(y*17+x);
                if (next.indices[target]!=255) {
                    if ((next.kinds[target]&128) || (next.kinds[target]&135)==3) continue;
                    VortexVictim victim;if (!next.actor(target,victim)) return false;
                    if ((victim.actor[3]&8) || protectedHit) continue;
                    if (!BattleRoster::dismiss(next,victim)) return false;++result.dismissed;found=true;break;
                }
                const byte tile=terrain[target];
                if (objects[target]==255 && !(tile>=blocking[0] && tile<blocking[1]) && !(tile>=blocking[2] && tile<blocking[3])) { found=true;break; }
            }
            if (!found) { ++result.failedPlacement;continue; }
            const byte index=byte(128+slot*8);byte *actor=next.extended+index;
            actor[0]=target%17;actor[1]=target/17;actor[2]=238;actor[3]=1;actor[4]=3;actor[5]=1;actor[7]=0;
            next.indices[target]=index;next.kinds[target]=2;++result.placed;result.slots.push_back(index);
        }
        // An NPC caster may itself have been transformed; reload by the actual
        // retained cell,as the source does,instead of overwriting that new actor.
        if (!next.actor(cell,caster)) return false;caster.stats[12]=255;if (!next.store(caster)) return false;
        result.spentMana=true;records=next;random=rng;memcpy(health,snapshots,32);output=result;return true;
    }
private:
    bool _ready;byte _record[32];
};
} }
#endif
