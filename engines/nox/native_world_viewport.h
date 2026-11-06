/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_VIEWPORT_H
#define NOX_NATIVE_WORLD_VIEWPORT_H
#include "nox/native_world_scene_phase.h"
#include "nox/native_occlusion.h"
#include "nox/native_tile_paint.h"
namespace Nox { namespace Native {
// Full viewport refresh through visibility repaint, before party/menu UI.
// Actual raster ordering is owned here; AI turns and frame-history scheduling
// remain separate. The caller supplies the source's prior painted-mask state.
class WorldViewport {
public:
    // Normal world entry derives its light-region selector from owned position.
    // Explicit-region refresh below remains useful for isolated rule contexts.
    static bool refreshAtWorld(World &world, WorldStory &window, const StoryMapPatches &story,
            const ActorPaintRules &actors, const TerrainLighting &lighting,
            const OcclusionRules &occlusion, const TilePainter &painter,
            byte appearance, byte extraLight, byte animation, const byte *paintedPrevious,
            PresentationState &presentation, LightState &light, VisibilityMask &visibility, HgrFrame &frame,
            WorldScene *preparedScene = nullptr) {
        return refresh(world,window,story,actors,lighting,occlusion,painter,world.coordinateSector(),appearance,extraLight,animation,
            paintedPrevious,presentation,light,visibility,frame,preparedScene);
    }
    static bool refresh(World &world, WorldStory &window, const StoryMapPatches &story,
            const ActorPaintRules &actors, const TerrainLighting &lighting,
            const OcclusionRules &occlusion, const TilePainter &painter,
            byte sector, byte appearance, byte extraLight, byte animation,
            const byte *paintedPrevious, PresentationState &presentation,
            LightState &light, VisibilityMask &visibility, HgrFrame &frame, WorldScene *preparedScene = nullptr) {
        if (!paintedPrevious || animation > 3) return false;
        World nextWorld(world); WorldStory nextWindow(window); PresentationState nextPresentation = presentation;
        LightState nextLight = light; VisibilityMask nextVisibility = visibility; HgrFrame nextFrame = frame; WorldScene scene;
        if (!WorldScenePhase::run(nextWorld,nextWindow,story,actors,lighting,sector,appearance,extraLight,
                nextPresentation,nextLight,nextVisibility,scene)) return false;
        Common::Array<byte> terrain; if (!nextWindow.project(nextWorld,terrain)) return false;
        // Full background is drawn before the source's actor pass and before
        // lighting preparation changes mode/mask. Actor records are now owned
        // results; replay only their ordered raster requests, never their logic.
        for (uint cell = 0; cell < 187; ++cell)
            if (!painter.draw(nextFrame,0,terrain[cell],light.mode,animation,cell,16)) return false;
        for (uint i = 0; i < scene.draws.size(); ++i) {
            const ActorDraw &draw = scene.draws[i];
            if (!painter.draw(nextFrame,draw.tile,scene.actors.terrain[draw.cell],light.mode,animation,draw.cell,draw.height)) return false;
        }
        OcclusionContext context; context.direction = 4; context.presentation = true; context.ignoreBlockers = false;
        memcpy(context.terrain,scene.actors.terrain,187); memcpy(context.objects,scene.objects.lookup,187);
        memcpy(context.records,nextWorld.currentTables()->data() + 1024,256); memcpy(context.previous,paintedPrevious,187);
        Common::Array<CellPaint> actions;
        if (!occlusion.apply(context,nextVisibility,actions)) return false;
        for (uint i = 0; i < actions.size(); ++i) {
            const CellPaint &action = actions[i];
            if (action.kind == kCellClear) { if (!nextFrame.clearCell(action.cell)) return false; }
            else if (!painter.draw(nextFrame,0,scene.actors.terrain[action.cell],nextLight.mode,animation,action.cell,16)) return false;
        }
        world.adoptState(nextWorld); window = nextWindow; presentation = nextPresentation;
        light = nextLight; visibility = nextVisibility; frame = nextFrame;
        // Hand back the exact accepted scene. Re-running its preparation would
        // settle objects and advance actor appearance a second time.
        if (preparedScene) *preparedScene = scene;
        return true;
    }
};
} }
#endif
