/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_VIEWPORT_MAPS_VALIDATION_H
#define NOX_NATIVE_VIEWPORT_MAPS_VALIDATION_H
#include "nox/native_world_viewport.h"
namespace Nox {
inline bool sameNativePreparedScene(const Native::WorldScene &a,const Native::WorldScene &b) {
    if (memcmp(a.objects.lookup,b.objects.lookup,187) || a.objects.portal!=b.objects.portal ||
        memcmp(a.actors.terrain,b.actors.terrain,187) || memcmp(a.actors.lookup,b.actors.lookup,187) ||
        memcmp(a.actors.kinds,b.actors.kinds,187) || a.actors.hiddenByTurn!=b.actors.hiddenByTurn ||
        a.draws.size()!=b.draws.size() || a.objects.draws.size()!=b.objects.draws.size() || a.actors.draws.size()!=b.actors.draws.size()) return false;
    for (uint i=0;i<a.draws.size();++i) if (a.draws[i].cell!=b.draws[i].cell || a.draws[i].tile!=b.draws[i].tile || a.draws[i].height!=b.draws[i].height) return false;
    for (uint i=0;i<a.objects.draws.size();++i) if (a.objects.draws[i].cell!=b.objects.draws[i].cell || a.objects.draws[i].tile!=b.objects.draws[i].tile) return false;
    for (uint i=0;i<a.actors.draws.size();++i) if (a.actors.draws[i].cell!=b.actors.draws[i].cell || a.actors.draws[i].tile!=b.actors.draws[i].tile || a.actors.draws[i].height!=b.actors.draws[i].height) return false;
    return true;
}
inline bool validateNativeViewportMaps(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','V','P','M') || input.readUint32LE() != 1 || input.readUint32LE() != 152) return false;
    Native::StoryMapPatches story; Native::ActorPaintRules actors; Native::TerrainLighting lighting; Native::OcclusionRules occlusion; Native::TilePainter painter;
    if (!story.load(archive)) return false;
    for (uint profile = 0; profile < 152; ++profile) {
        const byte type = input.readByte(),map = input.readByte(),x = input.readByte(),y = input.readByte();
        if (!actors.load(archive,type) || !lighting.load(archive,type) || !occlusion.load(archive,type) || !painter.load(archive,type)) return false;
        byte fields[256],spawns[256],mask[187],seed[8192],previous[187] = {};
        if (input.read(fields,256) != 256 || input.read(spawns,256) != 256 || input.read(mask,187) != 187) return false;
        Native::LightState light; light.mode = input.readByte(); light.level = input.readByte(); light.transitionTicks = input.readByte(); const byte animation = input.readByte();
        if (input.read(seed,8192) != 8192) return false;
        Native::World world(archive); Native::WorldStory window; Native::PresentationState presentation; Native::VisibilityMask visibility; Native::HgrFrame frame;
        if (!world.initialize(type,map,x,y,fields,256) || !window.initialize(world,spawns,256) || !visibility.loadRules(archive) || !visibility.importMask(mask,187) || !frame.importImage(seed,8192)) return false;
        for (uint step = 0; step < 2; ++step) {
            byte plane[8192],tables[1792],expectedMask[187],expectedFields[256],state[5];
            if (input.read(plane,8192) != 8192 || input.read(tables,1792) != 1792 || input.read(expectedMask,187) != 187 || input.read(expectedFields,256) != 256 || input.read(state,5) != 5) return false;
            Native::World comparisonWorld(world); Native::WorldStory comparisonWindow=window;
            Native::PresentationState comparisonPresentation=presentation; Native::LightState comparisonLight=light;
            Native::VisibilityMask comparisonVisibility=visibility; Native::WorldScene expectedScene,prepared;
            if (!Native::WorldScenePhase::run(comparisonWorld,comparisonWindow,story,actors,lighting,world.coordinateSector(),246,0,
                    comparisonPresentation,comparisonLight,comparisonVisibility,expectedScene)) return false;
            if (!Native::WorldViewport::refreshAtWorld(world,window,story,actors,lighting,occlusion,painter,246,0,animation,previous,presentation,light,visibility,frame,&prepared)) return false;
            if (!sameNativePreparedScene(prepared,expectedScene) || memcmp(world.currentTables()->data(),comparisonWorld.currentTables()->data(),1792)) {
                error=Common::String::format("Prepared scene handoff differs at profile%u step%u",profile,step); return false;
            }
            // Rejected refresh must not replace the caller's accepted scene.
            if (Native::WorldViewport::refreshAtWorld(world,window,story,actors,lighting,occlusion,painter,246,0,4,previous,presentation,light,visibility,frame,&prepared) ||
                !sameNativePreparedScene(prepared,expectedScene)) return false;
            for (uint i = 0; i < 8192; ++i) if (frame.data()[i] != plane[i]) {
                error = Common::String::format("Native map viewport pixel plane differs at context%u step%u byte%u native%u original%u",profile,step,i,frame.data()[i],plane[i]); return false;
            }
            if (memcmp(world.currentTables()->data(),tables,1792) || memcmp(visibility.cells(),expectedMask,187) || memcmp(world.questFields(),expectedFields,256) ||
                light.mode != state[0] || light.level != state[1] || light.transitionTicks != state[2] || presentation.facing != state[3] || byte(presentation.hidden) != state[4]) {
                error = Common::String::format("Native viewport retained state differs at profile%u step%u",profile,step); return false;
            }
        }
    }
    if (input.err() || input.pos() != input.size()) return false;
    report += "native-viewport-maps\tmaps=76\tchains=152\trefreshes=304\tprepared_scene_handoff=304\trejected_handoff_unchanged=304\tcoordinate_sector_owned=1\tfull_canvas_bytes=8192\toriginal_raster_including_cache_hits=1\tstory_objects_actors_light_occlusion=1\tUI=0\tAI=0\tcpu_execution=0\n"; return true;
}
}
#endif
