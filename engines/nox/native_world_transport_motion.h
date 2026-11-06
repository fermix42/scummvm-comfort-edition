/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_TRANSPORT_MOTION_H
#define NOX_NATIVE_WORLD_TRANSPORT_MOTION_H
#include "nox/native_world_transport.h"
#include "nox/native_traversal.h"
#include "nox/native_random.h"
namespace Nox { namespace Native {
struct TransportMoveCell {
    byte terrain,objectTile,objectState;bool actor,object;
    TransportMoveCell() : terrain(0),objectTile(0),objectState(0),actor(false),object(false) {}
};
struct TransportMotionContext {
    byte picture,speed,currentTerrain,phase,movementFlag;bool ignoreCollision,jumpRequested,jumpBusy,seaProtection;
    TransportMoveCell near,far,leading[2];
    TransportMotionContext() : picture(246),speed(0),currentTerrain(0),phase(16),movementFlag(0),ignoreCollision(false),jumpRequested(false),jumpBusy(false),seaProtection(false) {}
};
struct TransportMotionOutcome {
    bool blocked,interrupted,requiresInteraction,checkedFar,seaEvent,seaImpact;byte message;uint16 damage;
    TransportMotionOutcome() : blocked(false),interrupted(false),requiresInteraction(false),checkedFar(false),seaEvent(false),seaImpact(false),message(255),damage(0) {}
};
// Source player transport collision/sea-exposure transaction. A caller must
// commit its returned party effect and world/actor/clock continuation together;
// this owner does not move coordinates or silently execute a partial world turn.
class WorldTransportMotion {
public:
    WorldTransportMotion() : _ready(false),_type(255) {}
    bool load(HdvArchive &archive,byte type) {
        Common::Array<byte> game,footer;MapAssetRange range;
        if (!_walking.load(archive,type) || !archive.read("NA/GAME",game) || !resolveMapAssets(game,type,range) ||
            !archive.readRange(range.resource,range.offset+range.size,72,footer) || footer.size()!=72 || game.size()<0x158) return false;
        memcpy(_footer,footer.data(),72);_seaDamage[0]=game[0x156];_seaDamage[1]=game[0x157];_type=type;_ready=true;return true;
    }
    bool probe(const TransportMotionContext &c,WorldTransportState &state,GameplayRandom &random,TransportMotionOutcome &outcome) const {
        if (!_ready || !state.valid() || state.object==255) return false;
        WorldTransportState next=state;GameplayRandom rng=random;TransportMotionOutcome out;
        if (c.ignoreCollision) {outcome=out;return true;}
        if (c.speed>=4) {out.blocked=!_type && c.near.terrain==17;outcome=out;return true;}
        if (in(c.currentTerrain,34) || in(c.currentTerrain,14)) {
            if (c.currentTerrain!=_footer[67]) out.message=11;
            else if (c.picture!=128) {
                bool stop=c.phase>14;
                if (c.phase<13) {byte roll;if (!rng.next(1,255,roll)) return false;stop=roll>=64;}
                if (stop) {out.interrupted=true;out.message=26;random=rng;outcome=out;return true;}
            }
        }
        switch (c.picture) {
        case 124:
            out.blocked=water(c.near,1,c.seaProtection,next,out) && !(c.near.object && c.near.objectTile==119);break;
        case 123:out.blocked=water(c.near,2,c.seaProtection,next,out);break;
        case 119:
            // Original tests the farther leading cell first. Exposure is not
            // affected by this rough-seas-only frigate mode.
            out.blocked=water(c.leading[1],128,c.seaProtection,next,out);
            if (!out.blocked) out.blocked=water(c.leading[0],128,c.seaProtection,next,out);
            break;
        case 128:
            if (!walk(c.near,c.movementFlag,out.blocked,out.requiresInteraction)) return false;
            if (!out.blocked && !out.requiresInteraction) out.blocked=(c.near.terrain>=229 && c.near.terrain<233) || c.near.terrain==_footer[67];
            break;
        case 127:case 238: {
            const TransportMoveCell *cell=&c.near;
            for (uint attempt=0;attempt<2;++attempt) {
                bool blocked=false,pending=false;if (!walk(*cell,c.movementFlag,blocked,pending)) return false;
                if (pending) {out.requiresInteraction=true;break;}
                if (!blocked && cell->object) {out.blocked=in(cell->objectTile,52);break;}
                const bool waterTile=cell->terrain>=229 && cell->terrain<233;
                if (blocked || waterTile) {
                    if (!c.jumpRequested || !in(cell->terrain,12) || c.jumpBusy || attempt) {out.blocked=true;break;}
                    cell=&c.far;out.checkedFar=true;continue;
                }
                out.blocked=cell->terrain==_footer[67]?water(*cell,1,c.seaProtection,next,out) && !(cell->object && cell->objectTile==119):false;
                break;
            }
            break;
        }
        default:return false;
        }
        state=next;random=rng;outcome=out;return true;
    }
private:
    bool in(byte value,uint at) const {return value>=_footer[at] && value<_footer[at+1];}
    bool walk(const TransportMoveCell &cell,byte flag,bool &blocked,bool &pending) const {
        TraversalProbe probe;probe.terrain=cell.terrain;probe.actorPresent=cell.actor;probe.objectPresent=cell.object;
        probe.objectTile=cell.objectTile;probe.objectState=cell.objectState;probe.movementFlag=flag;
        TraversalDecision result;if (!_walking.probe(probe,result)) return false;
        blocked=result.blocked;pending=result.requiresInteraction;return true;
    }
    bool water(const TransportMoveCell &cell,byte mode,bool protection,WorldTransportState &state,TransportMotionOutcome &out) const {
        if (cell.object || cell.actor) return true;
        const byte tile=cell.terrain;
        if (tile>=205 && tile<211 && mode==1) return false;
        if (tile<229 || tile>=233) return true;
        if (mode&128) return tile!=229;
        if (tile==229) {
            if (mode==2) {state.waterExposure=byte(state.waterExposure+1);if (state.waterExposure<7) return false;}
            out.seaEvent=true;out.seaImpact=!protection;out.damage=protection?0:_seaDamage[mode-1];
            if (mode==2) return false;
        }
        state.waterExposure=0;return false;
    }
    bool _ready;byte _type,_footer[72],_seaDamage[2];TraversalRules _walking;
};
} }
#endif
