/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_VIEWPORT_VALIDATION_H
#define NOX_NATIVE_WORLD_VIEWPORT_VALIDATION_H
#include "nox/native_world_viewport.h"
namespace Nox {
inline bool validateNativeWorldViewport(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','V','P','R') || input.readUint32LE() != 1 || input.readUint32LE() != 2) return false;
    Native::StoryMapPatches story; Native::ActorPaintRules actors; Native::TerrainLighting lighting; Native::OcclusionRules occlusion; Native::TilePainter painter;
    if (!story.load(archive) || !actors.load(archive,0) || !lighting.load(archive,0) || !occlusion.load(archive,0) || !painter.load(archive,0)) return false;
    for (uint profile = 0; profile < 2; ++profile) {
        byte fields[256],spawns[256],mask[187],seed[8192],previous[187] = {};
        if (input.read(fields,256) != 256 || input.read(spawns,256) != 256 || input.read(mask,187) != 187) return false;
        Native::LightState light; light.mode = input.readByte(); light.level = input.readByte(); light.transitionTicks = input.readByte(); const byte animation = input.readByte();
        if (input.read(seed,8192) != 8192) return false;
        Native::World world(archive); Native::WorldStory window; Native::PresentationState presentation; Native::VisibilityMask visibility; Native::HgrFrame frame;
        if (!world.initialize(0,0,120,184,fields,256) || !window.initialize(world,spawns,256) || !visibility.loadRules(archive) || !visibility.importMask(mask,187) || !frame.importImage(seed,8192)) return false;
        for (uint step = 0; step < 2; ++step) {
            byte plane[8192],tables[1792],expectedMask[187],expectedFields[256],state[5];
            if (input.read(plane,8192) != 8192 || input.read(tables,1792) != 1792 || input.read(expectedMask,187) != 187 || input.read(expectedFields,256) != 256 || input.read(state,5) != 5) return false;
            if (!Native::WorldViewport::refresh(world,window,story,actors,lighting,occlusion,painter,0,246,0,animation,previous,presentation,light,visibility,frame)) return false;
            for (uint i = 0; i < 8192; ++i) if (frame.data()[i] != plane[i]) {
                error = Common::String::format("Native viewport pixel plane differs at profile%u step%u byte%u native%u original%u",profile,step,i,frame.data()[i],plane[i]); return false;
            }
            if (memcmp(world.currentTables()->data(),tables,1792) || memcmp(visibility.cells(),expectedMask,187) || memcmp(world.questFields(),expectedFields,256) ||
                light.mode != state[0] || light.level != state[1] || light.transitionTicks != state[2] || presentation.facing != state[3] || byte(presentation.hidden) != state[4]) {
                error = Common::String::format("Native viewport retained state differs at profile%u step%u",profile,step); return false;
            }
        }
    }
    if (input.err() || input.pos() != input.size()) return false;
    report += "native-world-viewport\tchains=2\trefreshes=4\tfull_canvas_bytes=8192\toriginal_raster_including_cache_hits=1\tstory_objects_actors_light_occlusion=1\tUI=0\tAI=0\tcpu_execution=0\n"; return true;
}
}
#endif
