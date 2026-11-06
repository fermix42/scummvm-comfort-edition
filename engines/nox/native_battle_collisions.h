/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_COLLISIONS_H
#define NOX_NATIVE_BATTLE_COLLISIONS_H
#include "nox/native_actor_step.h"
namespace Nox { namespace Native {
// Full visible11x11 board geometry over the source17-byte row storage.
// Large actors probe the two leading cells in original direction/cell order.
class BattleCollisions {
public:
    bool load(HdvArchive &archive) {
        for (uint category=0;category<9;++category) if (category!=1 && !_steps[category].load(archive,byte(category))) return false;
        return true;
    }
    bool probe(byte category,byte x,byte y,byte sprite,bool large,bool restricted,const byte *terrain,const byte *actors,
            const byte *objects,const byte *objectRecords,byte *blocked) const {
        if (category>8 || category==1 || x>(large?9:10) || y>(large?9:10) || !terrain || !actors || !blocked ||
            ((objects==nullptr)!=(objectRecords==nullptr))) return false;
        const int dx[]={0,0,1,-1},dy[]={-1,1,0,0};const int origin=int(y)*17+x;
        const int largeCells[4][2]={{-17,-16},{34,35},{2,19},{-1,16}};byte previous=0;
        for (uint direction=0;direction<4;++direction) {
            const int nextX=int(x)+dx[direction],nextY=int(y)+dy[direction];
            if (nextX<0 || nextX>(large?9:10) || nextY<0 || nextY>(large?9:10)) { blocked[direction]=2;continue; }
            blocked[direction]=0;
            for (uint foot=0;foot<(large?2U:1U);++foot) {
                const uint cell=uint(large?origin+largeCells[direction][foot]:nextY*17+nextX);
                const bool object=objects && objects[cell]!=255;ActorStepDecision decision;
                if (large) {
                    if (!_steps[category].probeTacticalLarge(terrain[cell],sprite,actors[cell]!=255,object,decision)) return false;
                } else {
                    ActorStepProbe input;input.terrain=terrain[cell];input.actorTile=sprite;input.occupied=actors[cell]!=255;
                    input.objectPresent=object;input.large=input.unrestricted=false;input.restrictedTerrain=restricted;
                    input.objectTile=object?objectRecords[uint(objects[cell])+2]:0;input.objectState=object?objectRecords[uint(objects[cell])+3]:0;
                    input.previousTerrain=previous;if (!_steps[category].probe(input,decision,true)) return false;previous=decision.effectiveTerrain;
                }
                if (decision.blocked) { blocked[direction]=1;break; }
            }
        }
        return true;
    }
private:
    ActorStepRules _steps[9];
};
} }
#endif
