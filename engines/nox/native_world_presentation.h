/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_PRESENTATION_H
#define NOX_NATIVE_WORLD_PRESENTATION_H
#include "nox/native_world.h"
#include "nox/native_world_story.h"
#include "nox/native_actor_paint.h"
namespace Nox { namespace Native {
struct PresentationState {
    byte facing, partyObject;
    bool hidden;
    byte retainedCell;bool retainedCellKnown;
    PresentationState() : facing(0), partyObject(255), hidden(false),retainedCell(0),retainedCellKnown(false) {}
};
struct WorldScene {
    ObjectScene objects;
    ActorScene actors;
    Common::Array<ActorDraw> draws;
};
// Complete quiescent world presentation ordering: objects, compact actors and
// extended actors. Actor AI/turn movement and the party/UI renderer are separate.
class WorldPresentation {
public:
    // Scrolling retains stamped portions whose actor origin left the viewport.
    // The original raster scroll keeps those pixels; native background repaint
    // must explicitly restore them without re-running actor admission or AI.
    static bool retainedLargeFootprints(const World &world,const byte *visibility,WorldScene &scene) {
        if (!world.ready() || !visibility) return false;
        for (uint cell=0;cell<187;++cell) {
            const byte slot=scene.actors.lookup[cell],kind=scene.actors.kinds[cell];
            if (slot==255 || kind==255 || visibility[cell] || !(kind&120)) continue;
            const byte actorKind=kind&7,foot=kind&120;
            if ((actorKind!=0 && actorKind!=4) || (slot&3) || (actorKind==4 && (slot&7))) return false;
            const byte *record=world.currentTables()->data()+(actorKind==4?1280:768)+slot;
            if (!record[2] || record[3]==255 || !(record[3]&(actorKind==4?16:8))) continue;
            const byte x=byte(record[0]-world.x()+128),y=byte(record[1]-world.y()+128);
            if (x>=120 && x<137 && y>=123 && y<134) continue;
            const byte part=foot==64?0:foot==32?1:foot==16?2:foot==8?3:255;
            if (part==255) return false;
            scene.draws.push_back(ActorDraw(byte(cell),byte(record[2]+part),16));
            // Preserve the source object/actor slot order for overlays that
            // actually painted after this retained actor in the same pass.
            for (uint i=0;i<scene.objects.draws.size();++i) {
                const ObjectDraw &object=scene.objects.draws[i];
                if (object.cell==cell && object.slot!=255 && object.slot>slot)
                    scene.draws.push_back(ActorDraw(byte(cell),object.tile,16));
            }
        }
        return true;
    }
    static bool run(World &world, const ActorPaintRules &rules, const byte *visibility,
            uint count, PresentationState &state, WorldScene &output, const WorldStory *window = nullptr) {
        if (!world.ready() || rules.mapType() != world.mapType() || !visibility || count != 187 || state.facing > 3 ||
            (state.partyObject != 255 && (state.partyObject > 252 || (state.partyObject & 3)))) return false;
        World candidate(world); PresentationState after = state; WorldScene scene; Common::Array<byte> terrain;
        if (window ? !window->project(candidate,terrain) : !candidate.projectTerrain(terrain)) return false;
        memcpy(scene.actors.terrain, terrain.data(), 187); scene.actors.hiddenByTurn = state.hidden;
        candidate.syncObjectFields(scene.objects.lookup);
        for (uint slot = 0; slot <= 252; slot += 4) {
            const byte *object=candidate.currentTables()->data()+1024+slot;
            const byte ox=byte(object[0]-candidate.x()+128),oy=byte(object[1]-candidate.y()+128);
            if (object[2] && ox>=120 && ox<137 && oy>=123 && oy<134) {
                after.retainedCell=byte((oy-123)*17+ox-120);after.retainedCellKnown=true;
            }
            const uint objectDraws = scene.objects.draws.size();
            if (!candidate.applyObjectPhase(slot, visibility, scene.actors.lookup, state.partyObject, 119, scene.objects)) return false;
            for (uint i = objectDraws; i < scene.objects.draws.size(); ++i)
                scene.draws.push_back(ActorDraw(scene.objects.draws[i].cell, scene.objects.draws[i].tile, 16));
            candidate.syncObjectFields(scene.objects.lookup);
            for (byte bank = 0; bank < 2; ++bank) {
                if (bank && (slot & 4)) continue;
                WorldActor actor;
                const ActorAdmission admitted = candidate.admitActor(bank, slot, true, actor);
                if (admitted == kActorInvalid) return false;
                if (admitted != kActorPresent) continue;
                const byte column = byte(byte(actor.x - candidate.x()) ^ 128), row = byte(byte(actor.y - candidate.y()) ^ 128);
                if (column < 120 || column >= 137 || row < 123 || row >= 134) continue;
                after.retainedCell=byte((row-123)*17+column-120);after.retainedCellKnown=true;
                candidate.faceActor(bank, slot, after.facing, actor); after.facing = byte((after.facing + 1) & 3);
                ActorPaintContext context; context.slot = byte(slot); context.kind = actor.kind;
                context.cell = byte(93 + int(column) - 128 + (int(row) - 128) * 17); context.column = column;
                context.tile = actor.tile; context.capability = actor.capability; context.flags = actor.flags;
                context.large = actor.large; context.hiddenByTurn = scene.actors.hiddenByTurn;
                memcpy(context.visibility, visibility, sizeof(context.visibility));
                const uint actorDraws = scene.actors.draws.size();
                if (!rules.paint(context, scene.actors)) return false;
                for (uint i = actorDraws; i < scene.actors.draws.size(); ++i) scene.draws.push_back(scene.actors.draws[i]);
            }
        }
        after.hidden = scene.actors.hiddenByTurn;
        world.adoptState(candidate); state = after; output = scene; return true;
    }
};
} }
#endif
