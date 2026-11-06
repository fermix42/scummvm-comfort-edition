/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_ROSTER_H
#define NOX_NATIVE_BATTLE_ROSTER_H
#include "nox/native_vortex_cast.h"
namespace Nox { namespace Native {
// Shared semantic occupancy for the complete supported record populations.
// Stat/actor records remain lossless; no CPU address or bank participates.
class BattleRoster {
public:
    static bool rebuild(BattleRecords &records, uint party, uint enemies, uint allies) {
        if (!party || party>6 || enemies>16 || allies>16) return false;
        BattleRecords next=records;
        memset(next.indices,255,187);memset(next.kinds,255,187);
        for (uint group=0;group<3;++group) {
            const uint count=group==0?party:group==1?enemies:allies;
            for (uint i=0;i<count;++i) {
                const byte kind=group==0?3:group==1?1:2;
                const uint index=group==0?i*4:group==1?i*8:128+i*8;
                const byte *actor=(group==0?next.compact:next.extended)+index;
                const byte *stats=next.stats+(group==0?i*128:group==1?768+i*32:1280+i*32);
                if (!READ_LE_UINT16(stats+2) || actor[3]==255 || !actor[2]) continue;
                const uint x=actor[0],y=actor[1];const bool large=group!=0 && (actor[3]&8);
                if (x>=17 || y>=11 || (large && (x>=16 || y>=10))) return false;
                const uint origin=y*17+x,offsets[]={0,1,17,18};const byte flags[]={64,32,16,8};
                for (uint j=0;j<(large?4U:1U);++j) {
                    const uint cell=origin+offsets[j];if (next.indices[cell]!=255) return false;
                    next.indices[cell]=byte(index);next.kinds[cell]=byte(kind|(large?flags[j]:0));
                }
            }
        }
        records=next;return true;
    }
    static bool practice(VortexVictim &actor, byte field) {
        if (actor.kind!=3 || field<1 || field+1>=128) return false;
        StatProgress progress={actor.stats[field-1],actor.stats[field],actor.stats[field+1]};
        progress.advance();actor.stats[field-1]=progress.value;actor.stats[field]=progress.practice;return true;
    }
    static bool remove(BattleRecords &records,VortexVictim &actor) {
        VortexVictim next=actor;next.stats[2]=next.stats[3]=0;if (next.kind==3) next.stats[0]=255;
        if (!dismiss(records,next)) return false;actor=next;return true;
    }
    // Original actor removal is also used for transformation/dismissal. It
    // clears spatial/action ownership without inventing HP death or XP credit.
    static bool dismiss(BattleRecords &records,VortexVictim &actor) {
        if (actor.cell>=187 || records.indices[actor.cell]!=actor.index ||
            (records.kinds[actor.cell]&135)!=actor.kind) return false;
        const bool large=actor.kind!=3 && (actor.actor[3]&8);
        const uint origin=uint(actor.actor[1])*17+actor.actor[0];const uint offsets[]={0,1,17,18};
        if (large) {
            if (actor.actor[0]>=16 || origin+18>=187) return false;
            for (uint i=0;i<4;++i) if (records.indices[origin+offsets[i]]!=actor.index || (records.kinds[origin+offsets[i]]&135)!=actor.kind) return false;
        }
        VortexVictim next=actor;
        next.actor[3]=255;if (next.kind!=3) next.actor[7]=255;
        if (!records.store(next)) return false;
        for (uint i=0;i<(large?4U:1U);++i) records.indices[(large?origin:actor.cell)+offsets[i]]=records.kinds[(large?origin:actor.cell)+offsets[i]]=255;
        actor=next;return true;
    }
};
} }
#endif
