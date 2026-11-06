/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_COMMAND_H
#define NOX_NATIVE_WORLD_COMMAND_H
#include "nox/native_world_presentation.h"
#include "common/events.h"
namespace Nox { namespace Native {
// Directional input's position/collision phase. Actor and object occupancy are
// the naturally computed scene lookups; terrain is its changed-window view.
// Clock policy, AI turns, encounters, scrolling and presentation are separate.
class WorldCommand {
public:
    static PositionMoveResult move(const Common::Event &event, World &world, const WorldStory &window,
            const WorldScene &scene, const TraversalRules &rules, TraversalDecision &decision) {
        if (event.type != Common::EVENT_KEYDOWN || !world.ready() || !world._onFoot || rules.mapType() != world.mapType()) return kPositionUnsupported;
        byte x = world.x(),y = world.y(); uint cell;
        switch (event.kbd.keycode) {
        case Common::KEYCODE_UP: --y; cell = 76; break;
        case Common::KEYCODE_DOWN: ++y; cell = 110; break;
        case Common::KEYCODE_RIGHT: ++x; cell = 94; break;
        case Common::KEYCODE_LEFT: --x; cell = 92; break;
        default: return kPositionUnsupported;
        }
        const MapDefinition *map = world._resources[world._states[world._current].family].find(world.mapId());
        if (!map || (!map->compressed && (x >= 48 || y >= 48))) return kPositionOutsideMap;
        Common::Array<byte> projected; if (!window.project(world,projected)) return kPositionUnsupported;
        TraversalProbe probe; probe.terrain = scene.actors.terrain[cell];
        probe.actorPresent = scene.actors.lookup[cell] != 255;
        const int object = scene.objects.lookup[cell] == 255 ? -1 : scene.objects.lookup[cell];
        return world.commitPositionProbe(x,y,probe,object,rules,decision);
    }
};
} }
#endif
