/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_SCENE_PHASE_H
#define NOX_NATIVE_WORLD_SCENE_PHASE_H
#include "nox/native_world_presentation.h"
#include "nox/native_lighting.h"
namespace Nox { namespace Native {
// Load/refresh scene phase: conditional story changes, projection and ordered
// objects/actors use the PRIOR mask, then visibility preparation advances it.
// This is not an AI turn, encounter scheduler or ordinary command loop.
class WorldScenePhase {
public:
    static bool run(World &world, WorldStory &window, const StoryMapPatches &story,
            const ActorPaintRules &actors, const TerrainLighting &lighting,
            byte sector, byte appearance, byte extraLight, PresentationState &presentation,
            LightState &light, VisibilityMask &visibility, WorldScene &output) {
        World nextWorld(world); WorldStory nextWindow(window); PresentationState nextPresentation = presentation;
        LightState nextLight = light; VisibilityMask nextVisibility = visibility; WorldScene scene;
        if (!nextWindow.apply(nextWorld,story) ||
            !WorldPresentation::run(nextWorld,actors,visibility.cells(),187,nextPresentation,scene,&nextWindow) ||
            !prepareSceneLighting(nextWorld,lighting,sector,appearance,extraLight,nextLight,nextVisibility,&nextWindow) ||
            !nextWindow.captureRecords(nextWorld)) return false;
        world.adoptState(nextWorld); window = nextWindow; presentation = nextPresentation;
        light = nextLight; visibility = nextVisibility; output = scene; return true;
    }
};
} }
#endif
