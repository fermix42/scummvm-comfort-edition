/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_ADVANCEMENT_H
#define NOX_NATIVE_BATTLE_ADVANCEMENT_H
#include "nox/native_vortex_cast.h"
#include "nox/native_battle_advancement_state.h"
namespace Nox { namespace Native {
class BattleAdvancement {
public:
    BattleAdvancement() : _ready(false) {}
    bool load(HdvArchive &archive) {
        Common::Array<byte> levels,ranges,thresholds,hp,points;
        if (!archive.readRange("NA/SRTN.COMBAT.S",0x441a,22,levels) || levels.size()!=22 ||
            !archive.readRange("NA/SRTN.COMBAT.S",0x4446,20,ranges) || ranges.size()!=20 ||
            !archive.readRange("NA/SRTN.COMBAT.S",0x445a,80,thresholds) || thresholds.size()!=80 ||
            !archive.readRange("NA/SRTN.COMBAT.S",0x4402,24,hp) || hp.size()!=24 ||
            !archive.readRange("NA/SRTN.COMBAT.S",0x4430,22,points) || points.size()!=22) return false;
        for (uint i=0;i<12;++i) _hp[i]=READ_LE_UINT16(hp.data()+i*2);
        for (uint i=0;i<11;++i) _points[i]=READ_LE_UINT16(points.data()+i*2);
        for (uint i=0;i<11;++i) _levels[i]=READ_LE_UINT16(levels.data()+i*2);
        for (uint i=0;i<10;++i) {
            _ranges[i]=READ_LE_UINT16(ranges.data()+i*2);
            for (uint group=0;group<4;++group) {
                _thresholds[group][i]=READ_LE_UINT16(thresholds.data()+group*20+i*2);
                if (!_thresholds[group][i] || _thresholds[group][i]>255) return false;
            }
        }
        _ready=true;return true;
    }
    bool requiresLevelAllocation(const byte *member) const {
        return _ready && member && member[0]!=255 && member[1]<10 && READ_LE_UINT16(member+5)>=_levels[member[1]];
    }
    bool begin(BattleRecords &battle,byte count,BattleAdvancementState &state) const {
        if (!_ready || !count || count>6 || state.started) return false;
        BattleRecords next=battle;BattleAdvancementState pending;pending.started=true;
        if (!advance(next,count,pending)) return false;battle=next;state=pending;return true;
    }
    // Numeric input is collected by the host; values above the remaining budget
    // are a normal refusal and leave the proposal exactly as it was.
    bool allocate(byte amount,BattleAdvancementState &state) const {
        if (!_ready || state.phase!=1 || state.ability>=3 || amount>99) return false;
        if (amount>state.remaining) return true;
        const byte ability=state.ability;state.points[ability]=amount;
        state.proposal[16+ability]=byte(state.proposal[16+ability]+amount);
        if (ability==2) state.proposal[4]=byte(state.proposal[4]+amount);
        state.remaining-=amount;state.proposal[71]=state.remaining;state.digits=state.value=0;++state.ability;
        if (!state.remaining || state.ability==3) {
            const uint total=uint(state.points[0])+state.points[1]+state.points[2];state.phase=total>=12?2:3;
        }
        return true;
    }
    bool confirm(bool accept,BattleRecords &battle,byte count,BattleAdvancementState &state) const {
        if (!_ready || state.phase!=2 || state.cursor>=count) return false;
        BattleRecords next=battle;BattleAdvancementState pending=state;
        if (!accept) { if (!prepare(next.stats+pending.cursor*128,pending)) return false; }
        else {
            byte *member=pending.proposal;const uint level=member[1];
            if (level<2 || level>10 || !_ranges[level-1]) return false;
            const uint baseline=byte(byte(_hp[level+1])-byte(_hp[level]))/2;
            const byte percent=byte(uint(member[16])*100/_ranges[level-1]);
            const uint increment=baseline+baseline*percent/100;
            WRITE_LE_UINT16(member+2,uint16(READ_LE_UINT16(member+2)+increment));
            WRITE_LE_UINT16(member+72,uint16(READ_LE_UINT16(member+72)+increment));
            for (uint field=43;field<=48;++field) member[field]=byte(member[field]+6);
            settleMember(member);memcpy(next.stats+pending.cursor*128,member,128);++pending.cursor;pending.phase=0;
            if (!advance(next,count,pending)) return false;
        }
        battle=next;state=pending;return true;
    }
    bool restart(BattleRecords &battle,byte count,BattleAdvancementState &state) const {
        if (!_ready || state.phase!=3 || state.cursor>=count) return false;
        return prepare(battle.stats+state.cursor*128,state);
    }
    // Settle skill practice with the authored, sometimes nonmonotonic costs.
    // Level allocation/HP growth is a pending UI operation when required; do
    // not silently auto-allocate ability points to make a result complete.
    bool settleSkills(BattleRecords &battle,byte partyCount) const {
        if (!_ready || !partyCount || partyCount>6) return false;
        for (uint i=0;i<partyCount;++i) if (requiresLevelAllocation(battle.stats+i*128)) return false;
        BattleRecords next=battle;const uint fields[]={25,28,31,34,37},groups[]={0,0,1,2,3};
        for (uint i=0;i<partyCount;++i) {
            byte *member=next.stats+i*128;if (member[0]==255) continue;
            for (uint skill=0;skill<5;++skill) {
                const uint field=fields[skill];uint bucket=0;
                while (bucket+1<10 && member[field]>=_ranges[bucket+1]) ++bucket;
                const byte cost=byte(_thresholds[groups[skill]][bucket]);
                while (member[field+1]>=cost) { member[field+1]-=cost;if (member[field]!=255) ++member[field]; }
                member[field+2]=cost;
            }
        }
        battle=next;return true;
    }
private:
    bool prepare(const byte *member,BattleAdvancementState &state) const {
        if (!requiresLevelAllocation(member) || !member[1] || member[1]>=10 || _points[member[1]+1]>99) return false;
        memcpy(state.proposal,member,128);const uint level=++state.proposal[1];
        WRITE_LE_UINT16(state.proposal+67,_levels[level]);state.proposal[71]=byte(_points[level]);
        state.remaining=state.proposal[71];state.phase=state.remaining?1:3;state.ability=state.digits=state.value=0;memset(state.points,0,3);return true;
    }
    bool advance(BattleRecords &battle,byte count,BattleAdvancementState &state) const {
        for (;state.cursor<count;++state.cursor) {
            byte *member=battle.stats+state.cursor*128;if (member[0]==255) continue;
            if (requiresLevelAllocation(member)) return prepare(member,state);
            settleMember(member);
        }
        state.phase=0;state.finished=true;return true;
    }
    void settleMember(byte *member) const {
        const uint fields[]={25,28,31,34,37},groups[]={0,0,1,2,3};
        for (uint skill=0;skill<5;++skill) {
            const uint field=fields[skill];uint bucket=0;while (bucket+1<10 && member[field]>=_ranges[bucket+1]) ++bucket;
            const byte cost=byte(_thresholds[groups[skill]][bucket]);
            while (member[field+1]>=cost) { member[field+1]-=cost;if (member[field]!=255) ++member[field]; }
            member[field+2]=cost;
        }
    }
    bool _ready;uint16 _levels[11],_ranges[10],_thresholds[4][10],_hp[12],_points[11];
};
} }
#endif
