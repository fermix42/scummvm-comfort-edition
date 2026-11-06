/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_ACTOR_PHASE_H
#define NOX_NATIVE_WORLD_ACTOR_PHASE_H
#include "nox/native_actors.h"
namespace Nox { namespace Native {
enum WorldActorPhaseRoute { kActorRawCachePhase,kActorViewportPhase,kActorProjectilePhase,kActorUnchangedPhase,kActorSkipPhase };
struct WorldActorPhase {
    byte column,row;
    bool oldOutside,outside,rawCache,skipStatic,face;
    WorldActorPhaseRoute route;
};
// Select the complete world actor entry/exit phase before spatial/action work.
// Coordinates are source byte-relative coordinates (party128,128). Cache bounds
// use that same coordinate domain; they describe the owned loaded map window.
class WorldActorPhaseRules {
public:
    static bool visible(byte x,byte y) { return x>=120 && x<137 && y>=123 && y<134; }
    static byte cell(byte x,byte y) { return byte(93+int(x)-128+(int(y)-128)*17); }
    static bool samplesViewport(byte x,byte y) {
        return visible(x,y) || ((x>=120 && x<137) && (y==121 || y==122 || y==134 || y==135)) ||
            ((y>=123 && y<134) && (x==118 || x==119 || x==137 || x==138));
    }
    static byte movedCell(byte previous,byte column,byte row,byte direction) {
        if (direction==0 && row>=123) return byte(previous-17);
        if (direction==1 && row<134) return byte(previous+17);
        if (direction==2 && column<137) return byte(previous+1);
        if (direction==3 && column>=120) return byte(previous-1);
        return previous;
    }
    static bool plan(byte oldColumn,byte oldRow,const WorldActor &actor,byte scroll,bool presentation,
            byte parent,const byte *cacheBounds,WorldActorPhase &output) {
        if (scroll>4 || !cacheBounds) return false;
        WorldActorPhase next;
        next.column=oldColumn;next.row=oldRow;
        next.oldOutside=!presentation && !visible(oldColumn,oldRow);
        next.skipStatic=next.oldOutside && actor.kind>=4 && !actor.capability;
        if (!presentation) {
            if (scroll==0) ++next.row;
            else if (scroll==1) --next.row;
            else if (scroll==2) --next.column;
            else if (scroll==3) ++next.column;
        }
        next.outside=!visible(next.column,next.row);next.rawCache=false;
        next.face=!next.outside && (presentation || next.oldOutside);
        if (!next.outside) next.route=kActorViewportPhase;
        else if (next.skipStatic) next.route=kActorSkipPhase;
        else if (parent==255 && actor.tile==225 && !presentation) next.route=kActorProjectilePhase;
        else {
            next.rawCache=true;
            const bool eligible=actor.kind<3 ? (actor.state&2)!=0 : actor.kind==4 && !actor.large;
            const bool cached=parent>=4 || (next.column>=cacheBounds[0] && next.column<cacheBounds[1] &&
                next.row>=cacheBounds[2] && next.row<cacheBounds[3]);
            next.route=presentation || !eligible || !cached ? kActorUnchangedPhase:kActorRawCachePhase;
        }
        output=next;return true;
    }
};
} }
#endif
