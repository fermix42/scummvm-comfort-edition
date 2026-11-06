/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_TARGETS_H
#define NOX_NATIVE_BATTLE_TARGETS_H
#include "nox/native_battle_roster.h"
namespace Nox { namespace Native {
// Target geometry and scan policy are separate from hit/damage arithmetic.
// The source grid has17-byte rows but the tactical cursor scans11x11 cells.
class BattleTargets {
public:
    static bool hand(const VortexVictim &caster,bool off,byte &kind,byte &range) {
        if (caster.kind!=3) return false;
        kind=byte(off?caster.stats[21]&15:caster.stats[21]>>4);
        range=kind?byte(off?caster.stats[22]&15:caster.stats[22]>>4):1;
        return kind!=1;
    }
    static bool permitted(const BattleRecords &battle,byte casterCell,byte targetCell,byte range,bool partyTargeting,bool &allowed) {
        allowed=false;VortexVictim caster,target;
        if (!battle.actor(casterCell,caster)) return false;
        if (targetCell>=187 || battle.indices[targetCell]==255) return true;
        if (!battle.actor(targetCell,target)) return false;
        if (caster.kind==target.kind && caster.index==target.index) return true;
        if (target.kind==3 && !partyTargeting) return true;
        const byte classification=footprintClass(battle,target);if (classification==2) return true;
        if ((target.kind==3?target.actor[3]:target.actor[7])==255) return true;
        allowed=axisInRange(caster.actor[0],targetCell%17,range,classification) &&
                axisInRange(caster.actor[1],targetCell/17,range,classification);return true;
    }
    static bool initial(const BattleRecords &battle,byte casterCell,byte range,bool partyTargeting,byte rememberedKind,byte rememberedIndex,byte &target) {
        target=255;
        for (uint cell=0;rememberedIndex!=255 && cell<187;++cell) {
            if (battle.indices[cell]!=rememberedIndex || (battle.kinds[cell]&135)!=(rememberedKind&135)) continue;
            VortexVictim remembered;if (!battle.actor(byte(cell),remembered)) return false;
            const uint origin=uint(remembered.actor[1])*17+remembered.actor[0];if (origin>=187) return false;
            // Source remembered-target reacquisition enters after the PvP
            // filter; toggling PvP off does not discard an existing party aim.
            bool allowed;if (!permitted(battle,casterCell,byte(origin),range,true,allowed)) return false;
            if (allowed) target=byte(origin);break;
        }
        if (target==255 && !nearest(battle,casterCell,range,true,false,target)) return false;
        if (target==255 && partyTargeting && !nearest(battle,casterCell,range,true,true,target)) return false;
        return true;
    }
    static bool nearest(const BattleRecords &battle,byte casterCell,byte range,bool enforceRange,bool partyTargeting,byte &selected,byte *alternative=nullptr) {
        selected=255;VortexVictim caster;if (!battle.actor(casterCell,caster)) return false;
        if (alternative) *alternative=255;
        uint best=256;
        for (uint cell=0;cell<187;++cell) {
            if (battle.indices[cell]==255 || (battle.kinds[cell]&128)) continue;
            VortexVictim target;if (!battle.actor(byte(cell),target)) return false;
            // Player initial selection tries enemies first, then the wildcard
            // pass only when no enemy exists and party targeting is enabled.
            const byte wanted=partyTargeting?255:caster.kind==3?1:caster.actor[5];
            if (target.kind!=wanted && target.kind!=2 && wanted!=255) continue;
            if (target.kind==2 && target.actor[4]!=wanted) continue;
            if (caster.kind==target.kind && caster.index==target.index) continue;
            if (footprintClass(battle,target)==2 || (target.kind==3?target.actor[3]:target.actor[7])==255) continue;
            if (enforceRange) {
                bool allowed;if (!permitted(battle,casterCell,byte(cell),range,partyTargeting,allowed)) return false;
                if (!allowed) continue;
            }
            const uint offsets[]={0,1,18,17},origin=uint(caster.actor[1])*17+caster.actor[0];
            for (uint foot=0;foot<((caster.kind!=3 && (caster.actor[3]&8))?4U:1U);++foot) {
                const uint position=origin+offsets[foot];
                const uint distance=ABS(int(position%17)-int(cell%17))+ABS(int(position/17)-int(cell/17));
                if (distance<best) { best=distance;selected=byte(cell); }
                else if (alternative) *alternative=byte(cell);
            }
        }
        return true;
    }
    // Horizontal input follows row-major wrap. Vertical input chooses the
    // nearest eligible column in the next eligible row, preserving scan ties.
    static bool cycle(const BattleRecords &battle,byte caster,byte range,bool partyTargeting,byte direction,byte &target) {
        if (direction>3 || target>=187 || target%17>10) return false;
        const uint oldX=target%17,oldY=target/17;uint x=oldX,y=oldY;
        if (direction>=2) {
            for (uint i=0;i<121;++i) {
                if (direction==2) { if (++x>10) { x=0;y=(y+1)%11; } }
                else { if (!x) { x=10;y=(y+10)%11; } else --x; }
                bool allowed;if (!permitted(battle,caster,byte(y*17+x),range,partyTargeting,allowed)) return false;
                if (allowed) { target=byte(y*17+x);return true; }
            }
            return true;
        }
        for (uint row=0;row<11;++row) {
            y=(y+(direction==0?10:1))%11;uint best=256;byte candidate=255;
            const uint start=row?10:oldX;
            for (uint step=0;step<=start+10;++step) {
                x=step<=start?start-step:step-start;
                bool allowed;if (!permitted(battle,caster,byte(y*17+x),range,partyTargeting,allowed)) return false;
                const uint distance=ABS(int(x)-int(oldX))+ABS(int(y)-int(oldY));
                if (allowed && distance<best) { best=distance;candidate=byte(y*17+x); }
            }
            if (candidate!=255) { target=candidate;return true; }
        }
        return true;
    }
    static uint distanceFromFootprint(const VortexVictim &caster,byte cell) {
        const uint offsets[]={0,1,18,17};uint best=256;
        const uint origin=uint(caster.actor[1])*17+caster.actor[0];
        for (uint i=0;i<((caster.kind!=3 && (caster.actor[3]&8))?4U:1U);++i) {
            const uint foot=origin+offsets[i];
            best=MIN<uint>(best,ABS(int(foot%17)-int(cell%17))+ABS(int(foot/17)-int(cell/17)));
        }
        return best;
    }
    static bool engager(const BattleRecords &battle,byte casterCell,byte &found) {
        found=0;
        for (uint cell=0;cell<187;++cell) {
            if (battle.indices[cell]==255 || ABS(int(cell%17)-int(casterCell%17))>1 || ABS(int(cell/17)-int(casterCell/17))>1) continue;
            VortexVictim actor;if (!battle.actor(byte(cell),actor)) return false;
            if (actor.stats[12]==casterCell) { found=byte(cell);break; }
        }
        return true;
    }
private:
    static byte footprintClass(const BattleRecords &battle,const VortexVictim &target) {
        if (!(target.actor[3]&8)) return 1;
        return battle.kinds[target.cell]&64?0:2;
    }
    static bool axisInRange(uint caster,uint target,byte range,byte classification) {
        const uint distance=ABS(int(caster)-int(target));if (distance<=range) return true;
        return !classification && caster>=(target?target-1:0) && distance==uint(range)+1;
    }
};
} }
#endif
