/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_COMPACT_WORLD_ACTION_H
#define NOX_NATIVE_COMPACT_WORLD_ACTION_H
#include "nox/native_actor_spatial.h"
#include "nox/native_actor_pursuit.h"
#include "nox/native_actor_movement_effects.h"
#include "nox/native_actor_outside.h"
#include "nox/native_world_actor_phase.h"
namespace Nox { namespace Native {
struct CompactWorldActionState {
    ActorSpatialState spatial;
    byte relativeX,relativeY,cell,freezeTimer,enabled,pattern,preferences[4],contactDirection,contactCell;
    bool hidden,appearanceChanged,movementPhase,stalled;
    CompactWorldActionState() : relativeX(128),relativeY(128),cell(93),freezeTimer(0),enabled(1),pattern(0),
        contactDirection(255),contactCell(255),hidden(false),appearanceChanged(false),movementPhase(false),stalled(false) { memset(preferences,0,4); }
    bool save(Common::WriteStream &out) const {
        out.writeUint32BE(MKTAG('N','C','W','A')); out.writeUint16LE(1); if (!spatial.save(out)) return false;
        out.writeByte(relativeX);out.writeByte(relativeY);out.writeByte(cell);out.writeByte(freezeTimer);out.writeByte(enabled);out.writeByte(pattern);
        out.write(preferences,4);out.writeByte(contactDirection);out.writeByte(contactCell);
        out.writeByte(hidden);out.writeByte(appearanceChanged);out.writeByte(movementPhase);out.writeByte(stalled);return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.size()-in.pos()!=37 || in.readUint32BE()!=MKTAG('N','C','W','A') || in.readUint16LE()!=1) return false;
        CompactWorldActionState next;byte spatialBytes[15];if (in.read(spatialBytes,15)!=15) return false;
        Common::MemoryReadStream s(spatialBytes,15);if (!next.spatial.load(s)) return false;
        next.relativeX=in.readByte();next.relativeY=in.readByte();next.cell=in.readByte();next.freezeTimer=in.readByte();next.enabled=in.readByte();next.pattern=in.readByte();
        if (in.read(next.preferences,4)!=4) return false;next.contactDirection=in.readByte();next.contactCell=in.readByte();
        byte flags[4];if (in.read(flags,4)!=4 || in.err()) return false;for (uint i=0;i<4;++i) if (flags[i]>1) return false;
        next.hidden=flags[0];next.appearanceChanged=flags[1];next.movementPhase=flags[2];next.stalled=flags[3];*this=next;return true;
    }
};
enum CompactWorldActionResult { kCompactWorldAdvanced,kCompactWorldForcedWait,kCompactWorldBattle,kCompactWorldAbsent,kCompactWorldUnsupported };
// Visible small compact actors on ordinary maps. A contact is a typed battle
// handoff; encounter execution and the ordered repaint/actor loop remain callers.
class CompactWorldAction {
public:
    CompactWorldAction() : _ready(false),_type(255),_waitLow(0),_waitHigh(0) {}
    bool load(HdvArchive &archive,byte type) {
        Common::Array<byte> game,footer;MapAssetRange range;
        if (type==255 || !_spatial.load(archive,type) || !_outside.load(archive,type) || !archive.read("NA/GAME",game) || !resolveMapAssets(game,type,range) ||
            !archive.readRange(range.resource,range.offset+range.size,72,footer)) return false;
        _waitLow=footer[34];_waitHigh=footer[35];_type=type;_ready=true;return true;
    }
    CompactWorldActionResult advance(World &world,uint slot,const WorldScene &scene,CompactWorldActionState &state,
            GameplayRandom &random,ActorMovementEffectState &effects,byte &direction,const WorldStory *window=nullptr,
            bool outside=false,byte retainedCell=0,byte retainedTerrain=0,const byte *visibility=nullptr,byte partyObject=255) const {
        if (!_ready || !world.ready() || world.mapType()!=_type || slot>252 || (slot&3) || world._onFoot!=(partyObject==255)) return kCompactWorldUnsupported;
        World candidate(world);WorldActor actor;const ActorAdmission admission=candidate.admitActor(0,slot,false,actor);
        if (admission==kActorAbsent) { direction=4;return kCompactWorldAbsent; }
        if (admission!=kActorPresent) return kCompactWorldUnsupported;
        const byte x=byte(actor.x-world.x()+128),y=byte(actor.y-world.y()+128);
        if (outside ? !window : !WorldActorPhaseRules::visible(x,y)) return kCompactWorldUnsupported;
        CompactWorldActionState next=state;GameplayRandom after=random;ActorMovementEffectState effect=effects;
        next.relativeX=x;next.relativeY=y;next.cell=outside?retainedCell:WorldActorPhaseRules::cell(x,y);next.movementPhase=false;next.stalled=false;
        next.contactDirection=next.contactCell=255;memset(next.preferences,0,4);
        bool wait=false;const byte terrain=outside?retainedTerrain:scene.actors.terrain[next.cell];
        if (terrain==1 || (terrain>=_waitLow && terrain<_waitHigh)) {
            byte roll;if (!after.next(1,255,roll)) return kCompactWorldUnsupported;
            if (roll>=192) { wait=true;next.stalled=true; }
        }
        if (!wait && (next.freezeTimer&127)) { next.freezeTimer=byte((next.freezeTimer&127)-1);wait=true; }
        if (!wait && !next.enabled) wait=true;
        byte selected=4;CompactWorldActionResult result=wait?kCompactWorldForcedWait:kCompactWorldAdvanced;
        if (!wait) {
            if (actor.tile==225) return kCompactWorldUnsupported;
            ActorSpatialContext context;context.column=x;context.row=y;context.tile=actor.tile;context.kind=0;context.capability=0;
            context.partyObject=partyObject;context.previousTerrain=terrain;context.actorState=actor.state;context.actorFlags=0;context.actorSlot=byte(slot);
            context.large=actor.large;context.restrictedTerrain=(actor.state&32)!=0;context.unrestricted=false;context.partyContact=true;
            context.visibility=visibility;
            ActorSpatialResult spatial;
            if (outside) {
                ActorOutsideDecision probe;
                if (!_outside.fromWindow(candidate,*window,actor.x,actor.y,actor.tile,false,probe)) return kCompactWorldUnsupported;
                memcpy(spatial.blocked,probe.blocked,4);spatial.status=kSpatialComplete;
            } else if (!_spatial.run(candidate,scene,context,next.spatial,spatial)) return kCompactWorldUnsupported;
            if (spatial.status==kSpatialBattleContact) {
                result=kCompactWorldBattle;next.contactDirection=spatial.contactDirection;next.contactCell=spatial.contactCell;
            } else {
                if (spatial.status!=kSpatialComplete) return kCompactWorldUnsupported;
                ActorIntentState shared;shared.movementPhase=false;shared.hidden=next.hidden;shared.appearanceChanged=next.appearanceChanged;
                shared.pattern=next.pattern;memcpy(shared.preferences,next.preferences,4);ActorIntentPlan plan;
                if (!ActorIntentRules::plan(actor,shared,after,plan)) return kCompactWorldUnsupported;
                if (plan.intent==kActorNeedsChase) {
                    ActorPursuitChoice choice;if (!ActorPursuitRules::choose(x,y,(actor.state&1)?kPursuitPreferredThenOtherAxes:kPursuitSingleAxisThenWander,spatial.blocked,after,choice)) return kCompactWorldUnsupported;
                    selected=choice.direction;memcpy(next.preferences,choice.preferences,4);
                } else {
                    ActorMotionChoice choice;if (!ActorMotionRules::choose(plan,actor.x,actor.y,0,0,spatial.blocked,after,choice)) return kCompactWorldUnsupported;
                    selected=choice.direction;memcpy(next.preferences,plan.state.preferences,4);
                }
                next.hidden=plan.state.hidden;next.appearanceChanged=plan.state.appearanceChanged;next.pattern=plan.state.pattern;
            }
        }
        if (result!=kCompactWorldBattle) {
            ActorMovementEffectContext input={world.mapType(),actor.tile,0,next.cell,selected,outside};
            if (!ActorMovementEffects::apply(candidate,scene,input,effect,selected)) return kCompactWorldUnsupported;
            byte *record=candidate._states[candidate._current].tables.data()+768+slot;
            record[0]=byte(actor.x+(selected==2?1:selected==3?-1:0));record[1]=byte(actor.y+(selected==1?1:selected==0?-1:0));
            next.relativeX=byte(x+(selected==2?1:selected==3?-1:0));next.relativeY=byte(y+(selected==1?1:selected==0?-1:0));
            next.cell=WorldActorPhaseRules::movedCell(next.cell,next.relativeX,next.relativeY,selected);
        }
        world.adoptState(candidate);state=next;random=after;effects=effect;direction=selected;return result;
    }
private:
    ActorSpatialRules _spatial;ActorOutsideRules _outside;bool _ready;byte _type,_waitLow,_waitHigh;
};
} }
#endif
