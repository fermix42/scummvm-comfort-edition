/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_STORY_PATCHES_VALIDATION_H
#define NOX_NATIVE_STORY_PATCHES_VALIDATION_H
#include "nox/native_story_patches.h"
namespace Nox {
inline bool validateNativeStoryPatches(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','S','M','P') || input.readUint32LE() != 1 || input.readUint32LE() != 297) return false;
    Native::StoryMapPatches rules; if (!rules.load(archive)) return false;
    uint changed = 0;
    for (uint i = 0; i < 297; ++i) {
        const byte type = input.readByte(), map = input.readByte(), sector = input.readByte();
        byte fields[256], expected[3072]; Native::StoryMapState state;
        if (input.read(fields,256) != 256 || input.read(state.terrain,2304) != 2304 || input.read(state.objects,256) != 256 ||
            input.read(state.actors,256) != 256 || input.read(state.spawns,256) != 256 || input.read(expected,3072) != 3072) return false;
        Native::StoryMapState before = state;
        if (!rules.apply(type,map,sector,fields,sizeof(fields),state)) return false;
        if (memcmp(state.terrain,expected,2304) || memcmp(state.objects,expected + 2304,256) ||
            memcmp(state.actors,expected + 2560,256) || memcmp(state.spawns,expected + 2816,256)) {
            error = Common::String::format("Native story map patch differs at case%u map%u type%u",i,map,type); return false;
        }
        changed += memcmp(before.terrain,state.terrain,2304) || memcmp(before.objects,state.objects,256) ||
            memcmp(before.actors,state.actors,256) || memcmp(before.spawns,state.spawns,256);
    }
    if (changed != 126 || input.err() || input.pos() != input.size()) return false;
    report += "native-story-patches\tcases=297\tsource_rules=11\tchanged=126\tfull_terrain_objects_actors_spawns=1\tworld_integration=0\tcpu_execution=0\n"; return true;
}
}
#endif
