/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_ACTOR_TURN_H
#define NOX_NATIVE_WORLD_ACTOR_TURN_H
#include "nox/native_compact_world_action.h"
#include "nox/native_npc_world_action.h"
#include "nox/native_world_actor_phase.h"
namespace Nox { namespace Native {
struct WorldActorTurnState {
    CompactWorldActionState compact;NpcWorldActionState npc;ActorMovementEffectState effects;PresentationState presentation;
    bool save(Common::WriteStream &out) const {
        out.writeUint32BE(MKTAG('N','W','A','T'));out.writeUint16LE(effects.extended()?3:2);
        if (!compact.save(out) || !npc.save(out) || !effects.save(out)) return false;
        out.writeByte(presentation.facing);out.writeByte(presentation.partyObject);out.writeByte(presentation.hidden);
        out.writeByte(presentation.retainedCell);out.writeByte(presentation.retainedCellKnown);return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        const int64 size=in.size()-in.pos();
        if (in.readUint32BE()!=MKTAG('N','W','A','T')) return false;
        const uint16 version=in.readUint16LE();if (version<1 || version>3 || size!=(version==1?117:version==2?119:122)) return false;
        const uint effectSize=version==3?12:9;byte a[37],b[62],c[12];if (in.read(a,37)!=37 || in.read(b,62)!=62 || in.read(c,effectSize)!=effectSize) return false;
        Common::MemoryReadStream ai(a,37),bi(b,62),ci(c,effectSize);WorldActorTurnState next;
        if (!next.compact.load(ai) || !next.npc.load(bi) || !next.effects.load(ci)) return false;
        next.presentation.facing=in.readByte();next.presentation.partyObject=in.readByte();const byte hidden=in.readByte();
        if (next.presentation.facing>3 || hidden>1 || in.err() || (next.presentation.partyObject!=255 && (next.presentation.partyObject&3))) return false;
        if (version>=2) {
            next.presentation.retainedCell=in.readByte();const byte known=in.readByte();if (known>1 || in.err()) return false;
            next.presentation.retainedCellKnown=known!=0;
        }
        next.presentation.hidden=hidden!=0;*this=next;return true;
    }
};
struct WorldActorTurnEvent {
    bool battle;byte bank,slot,direction,cell;
    WorldActorTurnEvent() : battle(false),bank(255),slot(255),direction(255),cell(255) {}
};
// Ordered wait-turn actor phase. It retains prior occupancy and updates it
// in source order as actors move. World input, time and light are
// separate phases. Unsupported contexts reject the complete transaction.
class WorldActorTurn {
public:
    WorldActorTurn() : _type(255) {}
    bool load(HdvArchive &archive,byte type) { if (!_paint.load(archive,type) || !_compact.load(archive,type) || !_npc.load(archive,type)) return false;_type=type;return true; }
    bool run(World &world,const WorldScene &prior,const byte *visibility,uint count,WorldActorTurnState &state,
            NpcScheduleState &schedule,NpcRouteStore &store,GameplayRandom &random,const FixedMessageCatalog &catalog,
            TextGlyphSurface &surface,WorldScene &output,WorldActorTurnEvent &event,byte scroll=4,const WorldStory *window=nullptr) const {
        if (!world.ready() || world.mapType()!=_type || world._onFoot!=(state.presentation.partyObject==255) || !visibility || count!=187 || state.presentation.facing>3 || scroll>4) return false;
        World candidate(world);WorldActorTurnState next=state;NpcScheduleState nextSchedule=schedule;NpcRouteStore nextStore=store;
        GameplayRandom after=random;TextGlyphSurface text=surface;WorldScene scene=prior;WorldActorTurnEvent raised;
        scene.draws.clear();scene.actors.draws.clear();scene.objects.draws.clear();scene.actors.hiddenByTurn=state.presentation.hidden;candidate.syncObjectFields(scene.objects.lookup);
        for (uint slot=0;slot<=252 && !raised.battle;slot+=4) {
            const byte *object=candidate.currentTables()->data()+1024+slot;
            const byte ox=byte(object[0]-candidate.x()+128),oy=byte(object[1]-candidate.y()+128);
            if (object[2] && WorldActorPhaseRules::visible(ox,oy)) {
                next.presentation.retainedCell=WorldActorPhaseRules::cell(ox,oy);next.presentation.retainedCellKnown=true;
            }
            const uint objectCount=scene.objects.draws.size();
            if (!candidate.applyObjectPhase(slot,visibility,scene.actors.lookup,next.presentation.partyObject,119,scene.objects)) return false;
            for (uint i=objectCount;i<scene.objects.draws.size();++i) scene.draws.push_back(ActorDraw(scene.objects.draws[i].cell,scene.objects.draws[i].tile,16));
            candidate.syncObjectFields(scene.objects.lookup);
            for (byte bank=0;bank<2 && !raised.battle;++bank) {
                if (bank && (slot&4)) continue;WorldActor actor;const ActorAdmission admission=candidate.admitActor(bank,slot,false,actor);
                if (admission==kActorInvalid) return false;if (admission!=kActorPresent) continue;
                byte x=byte(actor.x-candidate.x()+128),y=byte(actor.y-candidate.y()+128);
                const byte oldX=byte(x+(scroll==2?1:scroll==3?-1:0));
                const byte oldY=byte(y+(scroll==0?-1:scroll==1?1:0));
                byte bounds[]={0,255,0,255};WorldActorPhase phase;
                if (window && !window->actorBounds(world,bounds)) return false;
                if (!WorldActorPhaseRules::plan(oldX,oldY,actor,scroll,false,world.mapType(),bounds,phase)) return false;
                if (phase.route==kActorSkipPhase || phase.route==kActorUnchangedPhase) continue;
                if (phase.route==kActorProjectilePhase) return false;
                if (phase.face) { candidate.faceActor(bank,slot,next.presentation.facing,actor);next.presentation.facing=byte((next.presentation.facing+1)&3); }
                if (WorldActorPhaseRules::samplesViewport(x,y)) {
                    next.presentation.retainedCell=WorldActorPhaseRules::cell(x,y);next.presentation.retainedCellKnown=true;
                }
                if (!next.presentation.retainedCellKnown) return false;
                const byte oldCell=next.presentation.retainedCell;
                const byte sampledTerrain=oldCell<187?scene.actors.terrain[oldCell]:visibility[oldCell-187];
                if (!world.mapId() && sampledTerrain==17) {
                    // The original writes the compact tile bank even for this
                    // shared NPC caller; retain its actual record side effect.
                    candidate._states[candidate._current].tables[770+slot]=0;continue;
                }
                byte direction=4;
                if (phase.oldOutside && !phase.outside) {
                    // A newly entering actor paints without AI, freeze-counter
                    // retirement or RNG. It acts on the following world turn.
                } else if (!bank) {
                    next.compact.hidden=scene.actors.hiddenByTurn;next.compact.appearanceChanged=false;
                    const CompactWorldActionResult result=_compact.advance(candidate,slot,scene,next.compact,after,next.effects,direction,window,phase.outside,oldCell,sampledTerrain,visibility,next.presentation.partyObject);
                    if (result==kCompactWorldUnsupported || result==kCompactWorldAbsent) return false;
                    if (result==kCompactWorldBattle) { raised.battle=true;raised.bank=bank;raised.slot=byte(slot);raised.direction=next.compact.contactDirection;raised.cell=next.compact.contactCell;continue; }
                    scene.actors.hiddenByTurn=next.compact.hidden;
                } else {
                    next.npc.spatial=next.compact.spatial;next.npc.freezeTimer=next.compact.freezeTimer;next.npc.enabled=next.compact.enabled;
                    next.npc.action.navigation.motion.hidden=scene.actors.hiddenByTurn;next.npc.action.pattern=next.compact.pattern;
                    bool message=false;NpcBattleContact contact;const NpcWorldActionResult result=_npc.advanceWithEffects(candidate,nextSchedule,nextStore,slot,scene,scroll,next.presentation.partyObject,next.npc,after,catalog,text,next.effects,direction,message,&contact,window,phase.outside,oldCell,sampledTerrain,visibility);
                    if (result==kNpcWorldUnsupported || result==kNpcWorldAbsent) return false;
                    next.compact.spatial=next.npc.spatial;next.compact.freezeTimer=next.npc.freezeTimer;next.compact.pattern=next.npc.action.pattern;
                    if (result==kNpcWorldBattle) {
                        if (contact.direction>3 || contact.cell>=187) return false;
                        raised.battle=true;raised.bank=bank;raised.slot=byte(slot);raised.direction=contact.direction;raised.cell=contact.cell;continue;
                    }
                    scene.actors.hiddenByTurn=next.npc.action.navigation.motion.hidden;
                }
                if (candidate.admitActor(bank,slot,false,actor)!=kActorPresent) return false;
                x=byte(actor.x-candidate.x()+128);y=byte(actor.y-candidate.y()+128);
                next.presentation.retainedCell=WorldActorPhaseRules::movedCell(oldCell,x,y,direction);
                if (!WorldActorPhaseRules::visible(x,y)) continue;
                if (direction<4 && !phase.outside) {
                    for (uint part=0;part<(actor.large?4U:1U);++part) {
                        const uint at=oldCell+(part&1)+(part/2)*17;
                        if (at>=187 || (actor.large && (part&1) && phase.column==136)) continue;
                        if (!visibility[at] && (actor.large || candidate.questFields()[20]<6)) {
                            const byte objectSlot=actor.large?scene.objects.lookup[at]:255;
                            const byte tile=objectSlot==255?scene.actors.terrain[at]:(*candidate.currentTables())[1024+objectSlot+2];
                            scene.draws.push_back(ActorDraw(byte(at),tile,16));
                        }
                        scene.actors.lookup[at]=scene.actors.kinds[at]=255;
                    }
                }
                ActorPaintContext context;context.slot=byte(slot);context.kind=actor.kind;
                context.cell=byte((y-123)*17+x-120);context.column=x;context.tile=actor.tile;context.capability=actor.capability;context.flags=actor.flags;
                context.large=actor.large;context.hiddenByTurn=scene.actors.hiddenByTurn;memcpy(context.visibility,visibility,187);
                const uint drawCount=scene.actors.draws.size();if (!_paint.paint(context,scene.actors)) return false;
                for (uint i=drawCount;i<scene.actors.draws.size();++i) scene.draws.push_back(scene.actors.draws[i]);
            }
        }
        next.presentation.hidden=scene.actors.hiddenByTurn;world.adoptState(candidate);state=next;schedule=nextSchedule;store=nextStore;
        random=after;surface=text;output=scene;event=raised;return true;
    }
private:
    byte _type;ActorPaintRules _paint;CompactWorldAction _compact;NpcWorldAction _npc;
};
} }
#endif
