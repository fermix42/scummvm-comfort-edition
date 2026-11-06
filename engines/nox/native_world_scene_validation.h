/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_SCENE_VALIDATION_H
#define NOX_NATIVE_WORLD_SCENE_VALIDATION_H
#include "nox/native_world_scene_phase.h"
#include "common/memstream.h"
namespace Nox {
inline bool validateNativeWorldScene(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','S','P','H') || input.readUint32LE() != 1 || input.readUint32LE() != 2) return false;
    Native::StoryMapPatches story; Native::ActorPaintRules actors; Native::TerrainLighting lighting;
    if (!story.load(archive) || !actors.load(archive,0) || !lighting.load(archive,0)) return false;
    uint draws = 0;
    for (uint profile = 0; profile < 2; ++profile) {
        byte fields[256],spawns[256],mask[187];
        if (input.read(fields,256) != 256 || input.read(spawns,256) != 256 || input.read(mask,187) != 187) return false;
        Native::World world(archive); Native::WorldStory window; Native::PresentationState presentation;
        Native::LightState light; Native::VisibilityMask visibility;
        if (!world.initialize(0,0,120,184,fields,256) || !window.initialize(world,spawns,256) || !visibility.loadRules(archive) || !visibility.importMask(mask,187)) return false;
        for (uint step = 0; step < 2; ++step) {
            byte expected[5549]; if (input.read(expected,sizeof(expected)) != sizeof(expected)) return false;
            const uint count = input.readUint16LE(); Native::WorldScene scene;
            if (!Native::WorldScenePhase::run(world,window,story,actors,lighting,0,246,0,presentation,light,visibility,scene)) {
                error = Common::String::format("Native composed scene rejected profile%u step%u",profile,step); return false;
            }
            const Native::StoryMapState *state = window.state(); Common::MemoryWriteStreamDynamic actual(DisposeAfterUse::YES);
            actual.write(world.currentTables()->data(),1792); actual.write(state->terrain,2304); actual.write(state->spawns,256);
            actual.write(scene.actors.terrain,187); actual.write(scene.objects.lookup,187); actual.write(scene.actors.lookup,187); actual.write(scene.actors.kinds,187);
            actual.write(world.questFields(),256); actual.write(visibility.cells(),187);
            actual.writeByte(light.mode); actual.writeByte(light.level); actual.writeByte(light.transitionTicks);
            actual.writeByte(presentation.facing); actual.writeByte(presentation.hidden ? 1 : 0); actual.writeByte(scene.objects.portal);
            if (actual.size() != sizeof(expected)) return false;
            for (uint i = 0; i < sizeof(expected); ++i) if (actual.getData()[i] != expected[i]) {
                error = Common::String::format("Native composed scene differs at profile%u step%u byte%u native%u original%u",profile,step,i,actual.getData()[i],expected[i]); return false;
            }
            if (scene.draws.size() != count) return false;
            for (uint i = 0; i < count; ++i) {
                const byte cell = input.readByte(),tile = input.readByte(),height = input.readByte();
                if (scene.draws[i].cell != cell || scene.draws[i].tile != tile || scene.draws[i].height != height) return false;
            }
            draws += count;
        }
    }
    if (draws != 1 || input.err() || input.pos() != input.size()) return false;
    report += "native-world-scene\tchains=2\tphases=4\tprior_visibility_retained=1\tstory_world_window_objects_actors_light=1\tfull_state_bytes=5549\tordered_draws=1\tcomplete_turn=0\tcpu_execution=0\n"; return true;
}
}
#endif
