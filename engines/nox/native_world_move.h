/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_MOVE_H
#define NOX_NATIVE_WORLD_MOVE_H
#include "nox/native_world_command.h"
#include "nox/native_world_wait.h"
#include "nox/native_world_transport_motion.h"
#include "nox/native_party.h"
#include "nox/native_praevoc.h"
#include "nox/native_party_move_completion.h"
namespace Nox { namespace Native {
enum WorldMoveResult { kWorldMoveAdvanced,kWorldMoveNotice,kWorldMoveBlocked,kWorldMoveBoundary,kWorldMoveUnsupported,kWorldMoveEncounter,kWorldMoveArrival,kWorldMoveLeave,kWorldMoveInterrupted,kWorldMoveDefeat };
// Interior on-foot Keep movement: retain shifted occupancy, run the actual
// ordered actor phase, then suspend time for a notice or finish one turn.
// Encounters, special terrain and other arrival effects are explicit boundaries.
class WorldMove {
public:
    WorldMove() : _type(255),_quietGround(255),_sounds(nullptr),_effectsEnabled(true) {}
    void setSoundEvents(SoundEvents *sounds) {_sounds=sounds;_turn.setSoundEvents(sounds);}
    void setEffectsEnabled(bool enabled) {_effectsEnabled=enabled;}
    bool load(HdvArchive &archive,byte type=8) {
        Common::Array<byte> game,quiet;MapAssetRange assets;
        if (!archive.read("NA/GAME",game) || !resolveMapAssets(game,type,assets) || !archive.readRange(assets.resource,assets.offset+assets.size+67,1,quiet) || quiet.size()!=1) return false;
        _quietGround=quiet[0];if (!_praevoc.load(archive) || !_story.load(archive) || !_rules.load(archive,type) || !_transportRules.load(archive,type) || !_barrierRules.load(archive,type) || !_turn.load(archive,type) || !_painter.load(archive,type)) return false;_type=type;return true; }
    static bool completeClock(World &world,TurnClock &clock) {
        ClockAdvance advance;if (!advanceTurnClock(clock,advance)) return false;
        clock=advance.after;world._fields[20]=clock.hourlyCounter;return true;
    }
    bool completeEffects(const World &world,const WorldScene &scene,WorldActorTurnState &state,GameplayRandom &random,
            const FixedMessageCatalog &catalog,TextGlyphSurface &text,HgrFrame &frame,Party &party,bool &defeated) const {
        return _turn.completeEffects(world,scene,state,random,catalog,text,frame,&party,defeated);
    }
    WorldMoveResult run(const Common::Event &input,World &world,WorldStory &window,WorldScene &scene,WorldActorTurnState &state,
            TurnClock &clock,LightState &light,VisibilityMask &visibility,NpcScheduleState &schedule,NpcRouteStore &routes,
            GameplayRandom &random,const FixedMessageCatalog &catalog,TextGlyphSurface &text,HgrFrame &frame,Common::Array<byte> &notice,WorldActorTurnEvent &encounter,byte animation=0,
            WorldTransportState *transport=nullptr,Party *party=nullptr,bool forcedAlignment=false,byte partyAppearance=246,bool deferCompletion=false) const {
        if (!world.ready() || world.mapType()!=_type || animation>3 || state.effects.active || state.effects.queued) return kWorldMoveUnsupported;
        int dx=0,dy=0;byte direction;
        switch (input.kbd.keycode) {
        case Common::KEYCODE_UP:dy=-1;direction=0;break;case Common::KEYCODE_DOWN:dy=1;direction=1;break;
        case Common::KEYCODE_RIGHT:dx=1;direction=2;break;case Common::KEYCODE_LEFT:dx=-1;direction=3;break;
        default:return kWorldMoveUnsupported;
        }
        World candidate(world);WorldStory nextWindow=window;WorldScene shifted=scene;WorldActorTurnState turn=state;
        TurnClock nextClock=clock;LightState nextLight=light;VisibilityMask mask=visibility;NpcScheduleState nextSchedule=schedule;
        NpcRouteStore nextRoutes=routes;GameplayRandom after=random;TextGlyphSurface nextText=text;HgrFrame nextFrame=frame;
        const bool riding=!world.onFoot();WorldTransportState controls;Party nextParty;
        if (transport) controls=*transport;if (party) nextParty=*party;
        const auto transferControls=[&]() {if (transport) *transport=controls;if (party) *party=nextParty;};
        TraversalDecision decision;PositionMoveResult position=kPositionUnsupported;
        if (riding) {
            if (!transport || !party || !party->ready() || !controls.valid() || !controls.used || controls.object==255 || controls.object!=state.presentation.partyObject) return kWorldMoveUnsupported;
            byte *object=candidate._states[candidate._current].tables.data()+1024+controls.object;
            if (object[2]!=controls.appearance) return kWorldMoveUnsupported;
            TransportMotionContext context;context.picture=controls.appearance;context.speed=controls.speed;context.currentTerrain=scene.actors.terrain[93];context.phase=state.terrainRecovery;
            context.barrierProtection=(world.field(57)&2)!=0;context.jumpRequested=controls.jumpRequested;context.jumpBusy=controls.moveCount!=0;context.seaProtection=(world.field(57)&4)!=0;
            const uint near[]={76,110,94,92},far[]={59,127,95,91},lead[]={76,77,127,128,95,112,92,109};
            const auto cell=[&](uint at,TransportMoveCell &value) {
                value.terrain=scene.actors.terrain[at];value.actor=scene.actors.lookup[at]!=255;
                const byte slot=scene.objects.lookup[at];value.object=slot!=255;
                if (value.object) {const byte *record=candidate.currentTables()->data()+1024+slot;value.objectTile=record[2];value.objectState=record[3];}
            };
            cell(near[direction],context.near);cell(far[direction],context.far);cell(lead[direction*2],context.leading[0]);cell(lead[direction*2+1],context.leading[1]);
            TransportMotionOutcome motion;
            if (!forcedAlignment && !_transportRules.probe(context,controls,after,motion)) return kWorldMoveUnsupported;
            if (motion.barrierTouched) {
                bool dead=false;if (!_barrierRules.barrier(candidate,nextParty,catalog,nextText,nextFrame,dead,_sounds)) return kWorldMoveUnsupported;
                if (dead) {controls.used=controls.defeated=true;controls.pendingSteps=0;controls.pendingDirection=255;transferControls();random=after;text=nextText;frame=nextFrame;return kWorldMoveDefeat;}
                if (motion.requiresInteraction) {if (_sounds) _sounds->push_back(kSpeakerImpact);transferControls();random=after;text=nextText;frame=nextFrame;return kWorldMoveBlocked;}
            } else if (motion.requiresInteraction) return kWorldMoveBoundary;
            if (motion.message!=255 && !showTransportEffect(catalog,motion.message,nextText,nextFrame)) return kWorldMoveUnsupported;
            if (motion.interrupted || motion.blocked) {
                if (motion.blocked && _sounds) _sounds->push_back(kSpeakerImpact);
                transferControls();random=after;text=nextText;frame=nextFrame;return motion.interrupted?kWorldMoveInterrupted:kWorldMoveBlocked;
            }
            if (motion.seaEvent) {
                if (!showTransportEffect(catalog,motion.seaImpact?31:32,nextText,nextFrame)) return kWorldMoveUnsupported;
                if (motion.damage) for (uint i=0;i<nextParty.memberCount();++i) if (!nextParty.damage(i,motion.damage)) return kWorldMoveUnsupported;
                bool alive=false;for (uint i=0;i<nextParty.memberCount();++i) alive|=nextParty.member(i)->bytes[0]!=255;
                if (!alive) {controls.defeated=true;controls.pendingSteps=0;controls.pendingDirection=255;transferControls();random=after;text=nextText;frame=nextFrame;return kWorldMoveDefeat;}
            }
            const byte x=byte(candidate.x()+dx),y=byte(candidate.y()+dy);
            const MapDefinition *map=candidate._resources[candidate._states[candidate._current].family].find(candidate.mapId());
            if (!map || (!map->compressed && (x>=48 || y>=48))) return kWorldMoveLeave;
            candidate._x=x;candidate._y=y;object[0]=byte(object[0]+dx);object[1]=byte(object[1]+dy);position=kPositionMoved;
        } else {
            TransportMotionContext context;context.picture=partyAppearance;context.currentTerrain=scene.actors.terrain[93];context.phase=state.terrainRecovery;
            TransportMotionOutcome motion;if (!_transportRules.currentTerrain(context,after,motion)) return kWorldMoveUnsupported;
            if (motion.message!=255 && !showTransportEffect(catalog,motion.message,nextText,nextFrame)) return kWorldMoveUnsupported;
            if (motion.interrupted) {random=after;text=nextText;frame=nextFrame;return kWorldMoveInterrupted;}
            position=WorldCommand::move(input,candidate,nextWindow,scene,_rules,decision);
        }
        if (!riding && position==kPositionNeedsInteraction) {
            bool dead=false;if (!party || !_barrierRules.barrier(candidate,nextParty,catalog,nextText,nextFrame,dead,_sounds)) return kWorldMoveUnsupported;
            if (dead) {controls.used=controls.defeated=true;controls.pendingSteps=0;controls.pendingDirection=255;transferControls();random=after;text=nextText;frame=nextFrame;return kWorldMoveDefeat;}
            if (!(world.field(57)&2)) {if (_sounds) _sounds->push_back(kSpeakerImpact);transferControls();random=after;text=nextText;frame=nextFrame;return kWorldMoveBlocked;}
            // Source F22E branches straight to allowed after protected touch;
            // do not repeat the probe or apply later collision ranges.
            candidate._x=byte(world.x()+dx);candidate._y=byte(world.y()+dy);position=kPositionMoved;
        }
        if (position==kPositionBlocked) {random=after;text=nextText;frame=nextFrame;if (_sounds) _sounds->push_back(kSpeakerImpact);return kWorldMoveBlocked;}
        if (position!=kPositionMoved) return position==kPositionNeedsInteraction?kWorldMoveBoundary:kWorldMoveUnsupported;
        bool needsExit=false;if (!nextWindow.advanceWindow(candidate,direction,_story,needsExit)) return kWorldMoveUnsupported;
        if (needsExit) return kWorldMoveLeave;
        Common::Array<byte> terrain;if (!nextWindow.project(candidate,terrain)) return kWorldMoveUnsupported;
        byte cells[187];shifted.objects.portal=255;
        for (int row=0;row<11;++row) for (int column=0;column<17;++column) {
            const uint cell=row*17+column;const int x=column+dx,y=row+dy;const bool inside=x>=0 && x<17 && y>=0 && y<11;
            const uint prior=inside?uint(y*17+x):0;
            shifted.actors.terrain[cell]=terrain[cell];cells[cell]=inside?visibility.cells()[prior]:0;
            // M:EDF4/EEB5/EF6A/F026 clears $04AD after moving
            // the owned record and before the original viewport scroll.
            // Keep every other retained object cell in source order.
            shifted.objects.lookup[cell]=inside && !(riding && prior==93)?scene.objects.lookup[prior]:255;
            shifted.actors.lookup[cell]=inside?scene.actors.lookup[prior]:255;
            shifted.actors.kinds[cell]=inside?scene.actors.kinds[prior]:255;
            if (!_painter.draw(nextFrame,0,terrain[cell],light.mode,animation,cell,16)) return kWorldMoveUnsupported;
        }
        if (!mask.importMask(cells,187)) return kWorldMoveUnsupported;
        // The background above is freshly painted, so none of its hidden
        // pixels are already black. Retain shifted logical visibility for
        // actors while giving the raster pass its actual painted history.
        WorldActorTurnEvent raised;byte painted[187]={};
        const WorldWaitResult result=_turn.run(candidate,nextWindow,shifted,turn,nextClock,nextLight,mask,nextSchedule,nextRoutes,after,catalog,nextText,nextFrame,riding?controls.appearance:partyAppearance,animation,raised,false,direction,painted,party?&nextParty:nullptr,!deferCompletion);
        if ((result==kWorldWaitAdvanced || result==kWorldWaitBattle) &&
                !PartyMoveCompletion::apply(shifted.actors.terrain[93],_quietGround,turn.terrainRecovery,frame,nextFrame)) return kWorldMoveUnsupported;
        if (result==kWorldWaitDefeat) {
            controls.used=controls.defeated=true;controls.pendingSteps=0;controls.pendingDirection=255;
            world.adoptState(candidate);window=nextWindow;scene=shifted;state=turn;clock=nextClock;light=nextLight;visibility=mask;
            schedule=nextSchedule;routes=nextRoutes;random=after;text=nextText;frame=nextFrame;transferControls();return kWorldMoveDefeat;
        }
        if (result!=kWorldWaitAdvanced && result!=kWorldWaitBattle) return result==kWorldWaitPendingEffect?kWorldMoveBoundary:kWorldMoveUnsupported;
        const byte arrivedGround=shifted.actors.terrain[93];
        if (_sounds && arrivedGround!=_quietGround && (arrivedGround<229 || arrivedGround>=233)) _sounds->push_back(kSpeakerNotification);
        const StoryMapState *map=nextWindow.state();if (!map) return kWorldMoveUnsupported;
        bool arrival=false;
        for (uint offset=0;offset<256;offset+=4) {
            if (map->spawns[offset+2]&128) break;
            if (map->spawns[offset]==candidate.x() && map->spawns[offset+1]==candidate.y()) {arrival=true;break;}
        }
        Common::Array<byte> authored;SoundEvents spoken;bool pending=false;const byte slot=shifted.objects.lookup[93];
        if (slot!=255) {
            if (slot>252 || (slot&3)) return kWorldMoveUnsupported;
            const byte *object=candidate.currentTables()->data()+1024+slot;const byte kind=object[3];
            if (kind==55) {
                if (slot>248) return kWorldMoveUnsupported;
                const byte value=byte(candidate.field(object[4])^((object[7]&128)?255:0));
                if (!(value&object[5])) {
                    if (candidate.mapId()==69) {
                        if (!_praevoc.trigger(candidate._fields[12],turn.praevocCursor,turn.praevocUsed,_effectsEnabled,spoken)) return kWorldMoveUnsupported;
                    } else {
                        if (!candidate.notice(object[7]&127,authored)) return kWorldMoveUnsupported;
                        pending=true;
                    }
                }
            }
            // M:F0F4..F114 branches on55 only. Every other object already
            // passed traversal/turn admission and resumes the ordinary clock;
            // a second type whitelist incorrectly rejects static decorations.
        }
        if (!pending && !deferCompletion) {
            if (!completeClock(candidate,nextClock)) return kWorldMoveUnsupported;
            raised.clockPending=false;turn.population.arm(direction);
        }
        world.adoptState(candidate);window=nextWindow;scene=shifted;state=turn;clock=nextClock;light=nextLight;visibility=mask;
        schedule=nextSchedule;routes=nextRoutes;random=after;text=nextText;frame=nextFrame;notice=authored;encounter=raised;
        transferControls();if (_sounds) for (uint i=0;i<spoken.size();++i) _sounds->push_back(spoken[i]);return pending?kWorldMoveNotice:arrival?kWorldMoveArrival:raised.battle?kWorldMoveEncounter:kWorldMoveAdvanced;
    }
private:
    static bool showTransportEffect(const FixedMessageCatalog &catalog,byte id,TextGlyphSurface &text,HgrFrame &frame) {
        if (!text.importPages(frame.data(),frame.data(),8192) || !showFixedMessageStrip(catalog,id,text)) return false;
        for (uint y=184;y<192;++y) for (uint x=7;x<32;++x) if (!frame.putByte(x,y,text.page(0)[HgrFrame::rowOffset(y)+x])) return false;return true;
    }
    byte _type,_quietGround;SoundEvents *_sounds;bool _effectsEnabled;PraevocSpeechRules _praevoc;StoryMapPatches _story;TraversalRules _rules;WorldTransportMotion _transportRules;WorldHazards _barrierRules;WorldWait _turn;TilePainter _painter;
};
} }
#endif
