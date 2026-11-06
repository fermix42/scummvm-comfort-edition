/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_WAIT_H
#define NOX_NATIVE_WORLD_WAIT_H
#include "nox/native_world_actor_turn.h"
#include "nox/native_world_viewport.h"
#include "nox/native_clock.h"
#include "nox/native_world_hazards.h"
#include "nox/native_world_projectile.h"
namespace Nox { namespace Native {
enum WorldWaitResult { kWorldWaitAdvanced,kWorldWaitBattle,kWorldWaitPendingEffect,kWorldWaitUnsupported,kWorldWaitDefeat };
// Stationary on-foot Keep wait transaction through lighting, ordered actors,
// occlusion and clock. Damage/projectile and subsequent input-loop schedule
// services are explicit boundaries, never silently skipped.
class WorldWait {
public:
    WorldWait() : _ready(false),_type(255) {}
    bool load(HdvArchive &archive,byte type=8) {
        Common::Array<byte> game,footer;MapAssetRange range;
        if (!_turn.load(archive,type) || !_hazardRules.load(archive,type) || !_lighting.load(archive,type) || !_occlusion.load(archive,type) || !_painter.load(archive,type) ||
            !archive.read("NA/GAME",game) || !resolveMapAssets(game,type,range) || !archive.readRange(range.resource,range.offset+range.size,72,footer)) return false;
        memcpy(_hazards,footer.data()+46,4);_type=type;_ready=true;return true;
    }
    WorldWaitResult run(World &world,WorldStory &window,WorldScene &scene,WorldActorTurnState &state,
            TurnClock &clock,LightState &light,VisibilityMask &visibility,NpcScheduleState &schedule,NpcRouteStore &store,
            GameplayRandom &random,const FixedMessageCatalog &catalog,TextGlyphSurface &surface,HgrFrame &frame,
            byte appearance,byte animation,WorldActorTurnEvent &event,bool completeClock=true,byte direction=4,const byte *paintedPrevious=nullptr,Party *party=nullptr) const {
        if (!_ready || !world.ready() || world.mapType()!=_type || animation>3 || !window.matchesWorld(world)) return kWorldWaitUnsupported;
        if (state.effects.active || state.effects.queued) return kWorldWaitPendingEffect;
        const byte ground=scene.actors.terrain[93];
        // Source F6DC skips swimming/river damage while any transport is
        // owned. Other environmental/actor effects still run below.
        if (!party && ((world.onFoot() && ground>=229 && ground<232) || (ground>=_hazards[0] && ground<_hazards[1]) || (ground>=_hazards[2] && ground<_hazards[3]))) return kWorldWaitPendingEffect;
        World candidate(world);WorldStory nextWindow=window;WorldScene nextScene;WorldActorTurnState next=state;
        TurnClock nextClock=clock;LightState nextLight=light;VisibilityMask nextVisibility=visibility;NpcScheduleState nextSchedule=schedule;
        NpcRouteStore nextStore=store;GameplayRandom after=random;TextGlyphSurface text=surface;HgrFrame nextFrame=frame;WorldActorTurnEvent raised;Party nextParty;if (party) nextParty=*party;bool defeated=false;
        byte cleared[187]={};if (!nextVisibility.importMask(cleared,187) || !prepareSceneLighting(candidate,_lighting,candidate.coordinateSector(),appearance,clock.lightActive,nextLight,nextVisibility,&nextWindow) ||
            !_turn.run(candidate,scene,nextVisibility.cells(),187,next,nextSchedule,nextStore,after,catalog,text,nextScene,raised,direction,&nextWindow)) return kWorldWaitUnsupported;
        if (!raised.battle) {
            if (next.effects.active) return kWorldWaitPendingEffect;
            bool projectileHit=false;
            if (next.effects.queued) {
                if (!party) return kWorldWaitPendingEffect;
                WorldProjectileOutcome projectile;
                if (!next.effects.originKnown || !WorldProjectile::hostile(next.effects.originX,next.effects.originY,next.effects.code,next.effects.projectileMode,after,projectile)) return kWorldWaitUnsupported;
                projectileHit=projectile.hit;next.effects.queued=0;next.effects.projectileMode=true;
            }
            const byte slot=nextScene.actors.lookup[93];
            if (!party && slot!=255 && nextScene.actors.kinds[93]==0 && (*candidate.currentTables())[768+slot+2]==225) return kWorldWaitPendingEffect;
            if (party && !_hazardRules.apply(candidate,nextScene,nextParty,catalog,text,nextFrame,defeated,projectileHit)) return kWorldWaitUnsupported;
            if (completeClock && !defeated) {
                ClockAdvance advance;if (!advanceTurnClock(clock,advance)) return kWorldWaitUnsupported;nextClock=advance.after;
                candidate._fields[20]=nextClock.hourlyCounter;
            }
            if (!WorldPresentation::retainedLargeFootprints(candidate,nextVisibility.cells(),nextScene)) return kWorldWaitUnsupported;
            for (uint i=0;i<nextScene.draws.size();++i) { const ActorDraw &draw=nextScene.draws[i];
                if (!_painter.draw(nextFrame,draw.tile,nextScene.actors.terrain[draw.cell],nextLight.mode,animation,draw.cell,draw.height)) return kWorldWaitUnsupported;
            }
            OcclusionContext occlusion;occlusion.direction=direction;occlusion.presentation=false;occlusion.ignoreBlockers=false;
            memcpy(occlusion.terrain,nextScene.actors.terrain,187);memcpy(occlusion.objects,nextScene.objects.lookup,187);memcpy(occlusion.records,candidate.currentTables()->data()+1024,256);memcpy(occlusion.previous,paintedPrevious?paintedPrevious:visibility.cells(),187);
            Common::Array<CellPaint> actions;if (!_occlusion.apply(occlusion,nextVisibility,actions)) return kWorldWaitUnsupported;
            for (uint i=0;i<actions.size();++i) { const CellPaint &paint=actions[i];
                if (paint.kind==kCellClear) { if (!nextFrame.clearCell(paint.cell)) return kWorldWaitUnsupported; }
                else if (!_painter.draw(nextFrame,0,nextScene.actors.terrain[paint.cell],nextLight.mode,animation,paint.cell,16)) return kWorldWaitUnsupported;
            }
        }
        if (!nextWindow.captureRecords(candidate)) return kWorldWaitUnsupported;
        world.adoptState(candidate);window=nextWindow;scene=nextScene;state=next;clock=nextClock;light=nextLight;visibility=nextVisibility;
        schedule=nextSchedule;store=nextStore;random=after;surface=text;frame=nextFrame;event=raised;if (party) *party=nextParty;return raised.battle?kWorldWaitBattle:defeated?kWorldWaitDefeat:kWorldWaitAdvanced;
    }
private:
    bool _ready;byte _type,_hazards[4];WorldActorTurn _turn;WorldHazards _hazardRules;TerrainLighting _lighting;OcclusionRules _occlusion;TilePainter _painter;
};
} }
#endif
