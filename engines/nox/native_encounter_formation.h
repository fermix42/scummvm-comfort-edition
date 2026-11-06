/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ENCOUNTER_FORMATION_H
#define NOX_NATIVE_ENCOUNTER_FORMATION_H
#include "nox/native_encounter_catalog.h"
#include "nox/native_random.h"
#include "nox/native_vortex_cast.h"
namespace Nox { namespace Native {
// Native group/count/formation construction. The caller owns descriptor/context
// overrides, battlefield selection, mode preparation and eventual world return.
class EncounterFormation {
public:
    static bool group(HdvArchive &archive,byte groupId,byte fixedTemplate,uint partyCount,
            GameplayRandom &random,BattleRecords &records,byte &enemyCount,byte &groupFlags,byte contextFlags=0,byte spriteOverride=0) {
        if (groupId>=128 || !partyCount || partyCount>6 || (fixedTemplate!=255 && fixedTemplate>=142)) return false;
        if (contextFlags&31) return false;
        const uint layout=(contextFlags&128)?((contextFlags&64)?3:4):(contextFlags&64)?((contextFlags&32)?1:2):0;
        const bool fixedLayout=(contextFlags&128)!=0;
        Common::Array<byte> entries,bounds,positions,fixedPositions,monsters,directory;
        if (!archive.readRange("NA/SRTN.COMBAT.S",0x10b6,20,directory)) return false;
        const uint boundsAddress=READ_LE_UINT16(directory.data()+layout*4),partyAddress=READ_LE_UINT16(directory.data()+layout*4+2);
        if (boundsAddress<0x9600 || partyAddress<0x9600 ||
            !archive.readRange("NA/SRTN.COMBAT.S",0x113c+uint(groupId)*42,42,entries) ||
            !archive.readRange("NA/SRTN.COMBAT.S",1+boundsAddress-0x9600,fixedLayout?20:6,bounds) ||
            !archive.readRange("NA/SRTN.COMBAT.S",1+partyAddress-0x9600,12,positions) ||
            !archive.readRange("NA/SRTN.COMBAT.S",0x10e8,20,fixedPositions) ||
            !archive.readRange("NA/SRTN.COMBAT.S",0x263c,142*32,monsters)) return false;
        if (fixedTemplate!=255) entries[2]=entries[3]=fixedTemplate;
        const uint maximum=entries[0]&127;
        // Zero-count descriptors require their earlier context construction;
        // this phase must not turn them into an empty victorious encounter.
        if (!maximum || maximum>10) return false;
        GameplayRandom rng=random;BattleRecords next=records;
        const uint percent=10*(entries[1]>>4)+(entries[1]&15);
        const uint limit=maximum*percent/100;uint count=maximum;
        if (limit) {
            if (limit>=255) return false;
            byte draw;if (!rng.next(1,255,draw)) return false;
            const uint width=255/(limit+1);
            const uint decrement=limit-MIN<uint>(limit,(255-uint(draw))/width);
            if (decrement<=count) count-=decrement;
        }
        if (!count || count>10) return false;
        byte reservedX[17],reservedY[17];memset(reservedX,255,17);memset(reservedY,255,17);
        for (uint i=0;i<count;++i) {
            const uint entry=2+i*4,index=i*8;const byte placement=entries[entry+2];
            byte x=255,y=255;
            if (placement&128) {
                x=fixedPositions[i*2];y=fixedPositions[i*2+1];
            } else if (fixedLayout) {
                x=bounds[i*2];y=bounds[i*2+1];
            } else {
                const byte mode=placement&127;
                if (mode>1) y=bounds[4];
                else {
                    byte rowDraw;if (!rng.next(1,255,rowDraw)) return false;
                    const uint band=mode==1?2:0;
                    y=(rowDraw&128)?bounds[band+1]:(bounds[band]?bounds[band]:bounds[band+1]);
                    if (mode==1 && !y) {
                        if (!rng.next(1,255,rowDraw)) return false;
                        y=(rowDraw&128)?bounds[1]:(bounds[0]?bounds[0]:bounds[1]);
                    }
                }
                reservedY[i+1]=y;
                // Original collision rerolls retain the chosen row. A bounded
                // rejection is atomic; it never commits a partial roster/RNG.
                bool found=false;
                for (uint attempt=0;attempt<65536 && !found;++attempt) {
                    byte columnDraw;if (!rng.next(1,255,columnDraw)) return false;
                    x=byte((columnDraw>>5)+1);found=true;
                    for (uint j=0;j<17;++j) if (reservedX[j]==x && reservedY[j]==y) { found=false;break; }
                }
                if (!found) return false;reservedX[i+1]=x;
            }
            if (x>=17 || y>=11) return false;
            byte kindDraw;if (!rng.next(1,255,kindDraw)) return false;
            const byte monster=entries[entry+((kindDraw&128)?1:0)];if (monster>=142) return false;
            const byte *stats=monsters.data()+uint(monster)*32;
            memcpy(next.stats+768+i*32,stats,32);
            next.extended[index]=x;next.extended[index+1]=y;next.extended[index+2]=spriteOverride?spriteOverride:stats[16];
            next.extended[index+3]=entries[entry+3];next.extended[index+4]=1;
            next.extended[index+5]=3;next.extended[index+7]=0;
        }
        for (uint i=0;i<partyCount;++i) {
            if (positions[i*2]>=17 || positions[i*2+1]>=11) return false;
            next.compact[i*4]=positions[i*2];next.compact[i*4+1]=positions[i*2+1];
            next.compact[i*4+2]=next.stats[i*128+24];next.compact[i*4+3]=next.stats[i*128];
        }
        memset(next.stats+1280,255,511);
        records=next;random=rng;enemyCount=byte(count);groupFlags=entries[0]&128;return true;
    }
    static bool fixedSingle(HdvArchive &archive, const EncounterSelection &enemy,
            uint partyCount, GameplayRandom &random, BattleRecords &records) {
        if (!enemy.fixedMonster() || enemy.data[2]>=142 || partyCount!=1) return false;
        byte count=0,flags=0;
        return group(archive,124,enemy.data[2],partyCount,random,records,count,flags) && count==1 && !flags;
    }
};
} }
#endif
