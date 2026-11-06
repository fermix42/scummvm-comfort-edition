/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_WORLD_ACTION_H
#define NOX_NATIVE_NPC_WORLD_ACTION_H
#include "nox/native_actor_spatial.h"
#include "nox/native_npc_action_dispatch.h"
#include "nox/native_actor_movement_effects.h"
#include "nox/native_actor_outside.h"
namespace Nox { namespace Native {
struct NpcWorldActionState {
    NpcActionState action; ActorSpatialState spatial; NpcContactState contact;
    byte freezeTimer, enabled; bool stalled;
    NpcWorldActionState() : freezeTimer(0),enabled(1),stalled(false) {}
    bool save(Common::WriteStream &out) const {
        out.writeUint32BE(MKTAG('N','N','W','A')); out.writeUint16LE(1);
        if (!action.save(out) || !spatial.save(out) || !contact.save(out)) return false;
        out.writeByte(freezeTimer); out.writeByte(enabled); out.writeByte(stalled); return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.size()-in.pos()!=62 || in.readUint32BE()!=MKTAG('N','N','W','A') || in.readUint16LE()!=1) return false;
        byte a[29],s[15],c[9]; if (in.read(a,29)!=29 || in.read(s,15)!=15 || in.read(c,9)!=9) return false;
        Common::MemoryReadStream ai(a,29),si(s,15),ci(c,9); NpcWorldActionState next;
        if (!next.action.load(ai) || !next.spatial.load(si) || !next.contact.load(ci)) return false;
        next.freezeTimer=in.readByte(); next.enabled=in.readByte(); const byte stalledValue=in.readByte(); if (stalledValue>1 || in.err()) return false;
        next.stalled=stalledValue!=0; *this=next; return true;
    }
};
enum NpcWorldActionResult { kNpcWorldAdvanced,kNpcWorldForcedWait,kNpcWorldBattle,kNpcWorldAbsent,kNpcWorldUnsupported };
struct NpcBattleContact {
    byte direction,cell;
    NpcBattleContact() : direction(255),cell(255) {}
};
// Actual small-NPC action phase from an owned prepared scene. Position,
// collision and preview are derived here, never supplied as expected decisions.
// Ordered scene preparation/repaint, large/offscreen actors and battle execution
// remain explicit caller responsibilities.
class NpcWorldAction {
public:
    NpcWorldAction() : _ready(false),_type(255),_waitLow(0),_waitHigh(0) {}
    bool load(HdvArchive &archive,byte type) {
        Common::Array<byte> game,footer; MapAssetRange range;
        if (!_spatial.load(archive,type) || !_outside.load(archive,type) || !archive.read("NA/GAME",game) || !resolveMapAssets(game,type,range) ||
            !archive.readRange(range.resource,range.offset+range.size,72,footer)) return false;
        _waitLow=footer[34]; _waitHigh=footer[35]; _type=type; _ready=true; return true;
    }
    NpcWorldActionResult advance(World &world,NpcScheduleState &schedule,NpcRouteStore &store,uint slot,const WorldScene &scene,
            byte playerDirection,byte partyObject,NpcWorldActionState &state,GameplayRandom &random,
            const FixedMessageCatalog &catalog,TextGlyphSurface &surface,byte &direction,bool &message,ActorMovePhase *movement=nullptr,NpcBattleContact *battleContact=nullptr,
            const WorldStory *window=nullptr,bool outside=false,byte retainedCell=0,byte retainedTerrain=0,const byte *visibility=nullptr) const {
        if (!_ready || !world.ready() || world.mapType()!=_type || slot>=256 || (slot&7) || playerDirection>4) return kNpcWorldUnsupported;
        World nextWorld(world); WorldActor actor; const ActorAdmission admission=nextWorld.admitActor(1,slot,false,actor);
        if (admission==kActorAbsent) { direction=4; message=false; return kNpcWorldAbsent; }
        if (admission!=kActorPresent || world._onFoot!=(partyObject==255)) return kNpcWorldUnsupported;
        const byte x=byte(actor.x-world.x()+128),y=byte(actor.y-world.y()+128);
        if (outside ? (!window || actor.large || (actor.capability && actor.capability<128)) :
            (x<120 || x>=137 || y<123 || y>=134)) return kNpcWorldUnsupported;
        const byte cell=outside?retainedCell:byte((y-123)*17+x-120);
        NpcWorldActionState next=state; NpcScheduleState nextSchedule=schedule; NpcRouteStore nextStore=store;
        GameplayRandom after=random; TextGlyphSurface nextSurface=surface; next.stalled=false;
        next.action.navigation.motion.relativeX=x; next.action.navigation.motion.relativeY=y; next.action.navigation.motion.movementPhase=false;
        next.action.navigation.screenCell=cell; memset(next.action.navigation.preferences,0,4);
        bool wait=false; const byte terrain=outside?retainedTerrain:scene.actors.terrain[cell];
        if (terrain==1 || (terrain>=_waitLow && terrain<_waitHigh)) {
            byte roll; if (!after.next(1,255,roll)) return kNpcWorldUnsupported;
            if (roll>=192) { wait=true; next.stalled=true; }
        }
        if (!wait && (next.freezeTimer&127)) { next.freezeTimer=byte((next.freezeTimer&127)-1); wait=true; }
        if (!wait && !next.enabled) wait=true;
        byte selected=4; bool emitted=false; NpcWorldActionResult result=kNpcWorldForcedWait;NpcBattleContact raised;
        if (!wait) {
            if (actor.tile==225) return kNpcWorldUnsupported;
            ActorSpatialContext context; context.column=x; context.row=y; context.tile=actor.tile; context.kind=4; context.capability=actor.capability;
            context.partyObject=partyObject; context.previousTerrain=77; context.actorState=actor.state; context.actorFlags=actor.flags; context.actorSlot=byte(slot);
            context.large=actor.large; context.restrictedTerrain=false; context.unrestricted=false; context.partyContact=next.contact.availability!=0;
            context.visibility=visibility;
            ActorSpatialResult spatial;
            if (outside) {
                ActorOutsideDecision probe;
                if (!_outside.fromWindow(nextWorld,*window,actor.x,actor.y,actor.tile,false,probe)) return kNpcWorldUnsupported;
                memcpy(spatial.blocked,probe.blocked,4);spatial.status=kSpatialComplete;
            } else if (!_spatial.run(nextWorld,scene,context,next.spatial,spatial,&next.contact,actor.large)) return kNpcWorldUnsupported;
            if (spatial.status==kSpatialBattleContact) { result=kNpcWorldBattle;raised.direction=spatial.contactDirection;raised.cell=spatial.contactCell; }
            else if (spatial.status!=kSpatialComplete && spatial.status!=kSpatialStationaryConversation) return kNpcWorldUnsupported;
            else {
                NpcRouteRecoveryInputs input; input.partyX=world.x(); input.partyY=world.y(); input.suppressApproach=playerDirection==4; input.previewEmpty=true;
                if (actor.capability==2) {
                    const byte goal=(*world.currentTables())[1280+slot+4];
                    for (uint i=0;i<14;++i) {
                        const byte *request=schedule.requests()+i*4;
                        if ((request[2]&128) || request[0]!=goal || request[1]!=slot) continue;
                        if (!store.hasPage(request[3])) return kNpcWorldUnsupported;
                        const byte *payload=store.data()+uint(request[3])*256; const byte cursor=payload[0];
                        if (cursor<2 || (cursor&1)) return kNpcWorldUnsupported;
                        const byte tx=payload[cursor],ty=payload[cursor+1];
                        const byte desired=actor.x<tx?2:actor.x>tx?3:actor.y>ty?0:1;
                        const int preview=int(cell)+(desired==0?-17:desired==1?17:desired==2?1:-1);
                        if (preview>=0 && preview<187) input.previewEmpty=scene.actors.lookup[preview]==255 && scene.objects.lookup[preview]==255;
                        break;
                    }
                }
                const NpcActionResult moved=NpcActionDispatcher::advance(nextWorld,nextSchedule,nextStore,slot,spatial.blocked,input,next.action,after,catalog,nextSurface,selected,emitted,movement);
                if (moved!=kNpcActionAdvanced && moved!=kNpcActionWaitingForRoute) return kNpcWorldUnsupported;
                result=kNpcWorldAdvanced;
            }
        }
        if (wait && movement && !movement->apply(nextWorld,slot,selected)) return kNpcWorldUnsupported;
        // Adopt only after the whole phase accepts its context.
        world.adoptState(nextWorld); schedule=nextSchedule; store=nextStore; state=next; random=after; surface=nextSurface; direction=selected; message=emitted;
        if (result==kNpcWorldBattle && battleContact) *battleContact=raised;return result;
    }
    NpcWorldActionResult advanceWithEffects(World &world,NpcScheduleState &schedule,NpcRouteStore &store,uint slot,const WorldScene &scene,
            byte playerDirection,byte partyObject,NpcWorldActionState &state,GameplayRandom &random,
            const FixedMessageCatalog &catalog,TextGlyphSurface &surface,ActorMovementEffectState &effects,byte &direction,bool &message,NpcBattleContact *battleContact=nullptr,
            const WorldStory *window=nullptr,bool outside=false,byte retainedCell=0,byte retainedTerrain=0,const byte *visibility=nullptr) const {
        class PreparedMovement : public ActorMovePhase {
        public:
            PreparedMovement(const WorldScene &scene,ActorMovementEffectState &effects,bool outside,byte cell) : _scene(scene),_effects(effects),_outside(outside),_cell(cell) {}
            bool apply(World &candidate,uint actorSlot,byte &selected) override {
                const byte *actor=candidate.currentTables()->data()+1280+actorSlot;
                const byte x=byte(actor[0]-candidate.x()+128),y=byte(actor[1]-candidate.y()+128);
                if (!_outside && (x<120 || x>=137 || y<123 || y>=134)) return false;
                ActorMovementEffectContext input={candidate.mapType(),actor[2],actor[6],_outside?_cell:byte((y-123)*17+x-120),selected,_outside};
                return ActorMovementEffects::apply(candidate,_scene,input,_effects,selected);
            }
        private:
            const WorldScene &_scene; ActorMovementEffectState &_effects;bool _outside;byte _cell;
        };
        ActorMovementEffectState after=effects; PreparedMovement phase(scene,after,outside,retainedCell);
        const NpcWorldActionResult result=advance(world,schedule,store,slot,scene,playerDirection,partyObject,state,random,catalog,surface,direction,message,&phase,battleContact,window,outside,retainedCell,retainedTerrain,visibility);
        if (result!=kNpcWorldUnsupported) effects=after;
        return result;
    }
private:
    ActorSpatialRules _spatial;ActorOutsideRules _outside; bool _ready; byte _type,_waitLow,_waitHigh;
};
} }
#endif
