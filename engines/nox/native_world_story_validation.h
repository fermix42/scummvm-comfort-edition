/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_STORY_VALIDATION_H
#define NOX_NATIVE_WORLD_STORY_VALIDATION_H
#include "nox/native_world_story.h"
#include "nox/native_world_travel.h"
#include "common/savefile.h"
#include "common/memstream.h"
namespace Nox {
inline bool validateNativeWorldCache(HdvArchive &archive,Common::SeekableReadStream &input,Common::SaveFileManager &files,
        bool cold,Common::String &report,Common::String &error) {
    if (input.readUint32LE()!=1 || input.readUint32LE()!=32) return false;
    Native::StoryMapPatches rules;if (!rules.load(archive)) return false;
    for (uint profile=0;profile<32;++profile) {
        const byte type=input.readByte(),map=input.readByte(),x=input.readByte(),y=input.readByte(),direction=input.readByte();
        byte fields[256],tables[1792],before[3083],expected[3083],expectedTables[1536];
        if (input.read(fields,256)!=256 || input.read(tables,1792)!=1792 || input.read(before,3083)!=3083 ||
                input.read(expected,3083)!=3083 || input.read(expectedTables,1536)!=1536) return false;
        Native::World world(archive);Native::WorldStory window;
        if (cold && !profile) {
            Common::InSaveFile *w=files.openForLoading("nox-native-story-world.bin"),*v=files.openForLoading("nox-native-story-window.bin");
            const bool loaded=w && v && world.load(*w) && window.load(*v,world);delete w;delete v;if (!loaded) return false;
        } else {
            Common::MemoryReadStream initial(before,sizeof(before));
            if (!world.initialize(type,map,x,y,fields,256) || memcmp(world.currentTables()->data(),tables,1536) || !window.load(initial,world)) return false;
        }
        bool needsExit=false;if (!window.advanceWindow(world,direction,rules,needsExit) || needsExit) return false;
        Common::MemoryWriteStreamDynamic serialized(DisposeAfterUse::YES); if (!window.save(serialized) || serialized.size()!=sizeof(expected) ||
                memcmp(serialized.getData(),expected,sizeof(expected)) || memcmp(world.currentTables()->data(),expectedTables,1536)) {
            error=Common::String::format("Native cache shift differs at case%u direction%u cold%u",profile,uint(direction),uint(cold));return false;
        }
        Common::Array<byte> projection;if (!window.project(world,projection) || projection.size()!=187) return false;
        // Each complete source result must survive the same production serializers.
        Common::MemoryWriteStreamDynamic worldBytes(DisposeAfterUse::YES); if (!world.save(worldBytes)) return false;
        Common::MemoryReadStream wr(worldBytes.getData(),worldBytes.size()),sr(serialized.getData(),serialized.size());
        Native::World restored(archive);Native::WorldStory restoredWindow;
        if (!restored.load(wr) || !restoredWindow.load(sr,restored)) return false;
        Common::MemoryWriteStreamDynamic again(DisposeAfterUse::YES);if (!restoredWindow.save(again) || again.size()!=serialized.size() || memcmp(again.getData(),serialized.getData(),again.size())) return false;
        if (!cold && !profile) {
            Common::OutSaveFile *w=files.openForSaving("nox-native-story-world.bin",false),*v=files.openForSaving("nox-native-story-window.bin",false);
            bool saved=w && v && world.save(*w) && window.save(*v);
            if (w) {w->finalize();saved=saved && !w->err();}if (v) {v->finalize();saved=saved && !v->err();}delete w;delete v;if (!saved) return false;
        }
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report+=cold?"native-world-story\tphase=cold\tactual_result_restored=1\tcontinued_operation=1\tcache_cases=32\tcpu_execution=0\n":
        "native-world-story\tphase=warm\tactual_World_owners=1\tcache_cases=32\tfull_mutable_overlap_spawns=1\tstate_roundtrips=32\tcpu_execution=0\n";
    return true;
}
inline bool validateNativeWorldTravel(HdvArchive &archive,Common::SeekableReadStream &input,Common::SaveFileManager &files,
        bool cold,Common::String &report,Common::String &error) {
    if (input.readUint32LE()!=2 || input.readUint32LE()!=1396) return false;
    for (uint i=0;i<1396;++i) {
        const byte type=input.readByte(),map=input.readByte(),x=input.readByte(),y=input.readByte(),flag=input.readByte();
        byte spawns[256],expected[6];if (input.read(spawns,256)!=256 || input.read(expected,6)!=6) return false;
        Native::WorldTravelPlan plan;const Native::WorldTravelResult result=Native::WorldTravelRules::enter(type,map,x,y,flag,spawns,256,plan);
        const byte actual[]={byte(result),plan.type,plan.map,plan.x,plan.y,plan.slot};
        if (memcmp(actual,expected,6)) {
            error=Common::String::format("Native travel selection differs at case%u source%u/%u position%u/%u result%u/%u target%u/%u slot%u/%u",i,uint(type),uint(map),uint(x),uint(y),uint(result),uint(expected[0]),uint(plan.type),uint(plan.map),uint(plan.slot),uint(expected[5]));return false;
        }
    }
    if (input.readUint32LE()!=532) return false;
    Common::Array<byte> game;if (!archive.read("NA/GAME",game)) return false;
    for (uint i=0;i<532;++i) {
        const byte type=input.readByte(),map=input.readByte(),hour=input.readByte(),minute=input.readByte(),expected=input.readByte();byte mode;
        if (!Native::WorldTravelRules::lightMode(type,map,hour,minute,game.data()+0x10b1,game.size()-0x10b1,mode) || mode!=expected) {
            error=Common::String::format("Native travel light differs at case%u map%u hour%u minute%u",i,uint(map),uint(hour),uint(minute));return false;
        }
    }
    Native::World world(archive);Native::WorldStory window;
    if (cold) {
        Common::InSaveFile *w=files.openForLoading("nox-native-story-world.bin"),*v=files.openForLoading("nox-native-story-window.bin");
        const bool loaded=w && v && world.load(*w) && window.load(*v,world);delete w;delete v;if (!loaded) return false;
        Native::StoryMapPatches rules;bool exit=false;if (!rules.load(archive) || !window.advanceWindow(world,0,rules,exit) || exit) return false;
    } else {
        byte fields[256]={};if (!world.initialize(8,7,24,38,fields,256) || !window.initialize(world,world.currentTables()->data()+1536,256)) return false;
        Common::OutSaveFile *w=files.openForSaving("nox-native-story-world.bin",false),*v=files.openForSaving("nox-native-story-window.bin",false);
        bool saved=w && v && world.save(*w) && window.save(*v);if (w) {w->finalize();saved=saved && !w->err();}if (v) {v->finalize();saved=saved && !v->err();}delete w;delete v;if (!saved) return false;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report+=cold?"native-world-story\tphase=cold\tactual_result_restored=1\tcontinued_operation=1\ttravel_cases=1396\tcpu_execution=0\n":
        "native-world-story\tphase=warm\tactual_World_owners=1\ttravel_cases=1396\tauthored_maps=76\tcpu_execution=0\n";
    return true;
}
inline bool validateNativeWorldStory(HdvArchive &archive, Common::SeekableReadStream &input, Common::SaveFileManager &files,
        bool cold, Common::String &report, Common::String &error) {
    const uint32 tag=input.readUint32BE();
    if (tag==MKTAG('N','W','T','R')) return validateNativeWorldTravel(archive,input,files,cold,report,error);
    if (tag==MKTAG('N','W','S','C')) return validateNativeWorldCache(archive,input,files,cold,report,error);
    if (tag != MKTAG('N','S','W','S') || input.readUint32LE() != 1 || input.readUint32LE() != 2) return false;
    Native::StoryMapPatches rules; if (!rules.load(archive)) return false;
    for (uint profile = 0; profile < 2; ++profile) {
        byte fields[256],spawns[256]; if (input.read(fields,256) != 256 || input.read(spawns,256) != 256) return false;
        Native::World world(archive); Native::WorldStory window;
        if (cold && !profile) {
            Common::InSaveFile *w = files.openForLoading("nox-native-story-world.bin"), *s = files.openForLoading("nox-native-story-window.bin");
            const bool restored = w && s && world.load(*w) && window.load(*s,world); delete w; delete s;
            if (!restored) return false;
        } else if (!world.initialize(0,0,120,184,fields,256) || !window.initialize(world,spawns,256)) return false;
        for (uint step = 0; step < 2; ++step) {
            byte expected[4352]; if (input.read(expected,sizeof(expected)) != sizeof(expected)) return false;
            if (!(cold && !profile && !step) && !window.apply(world,rules)) return false;
            const Native::StoryMapState *state = window.state();
            if (!state || memcmp(state->terrain,expected,2304) || memcmp(world.currentTables()->data(),expected + 2304,1792) ||
                memcmp(state->spawns,expected + 4096,256)) {
                error = Common::String::format("Native world story differs at profile%u step%u cold%u",profile,step,uint(cold)); return false;
            }
            byte tile;
            if (!profile && (!window.terrainAt(115,182,tile) || tile != 181)) return false;
            Common::Array<byte> projected;
            if (!window.project(world,projected) || projected.size() != 187) return false;
            if (!cold && !profile && !step) {
                Common::OutSaveFile *w = files.openForSaving("nox-native-story-world.bin",false), *s = files.openForSaving("nox-native-story-window.bin",false);
                bool saved = w && s && world.save(*w) && window.save(*s);
                if (w) { w->finalize(); saved = saved && !w->err(); } if (s) { s->finalize(); saved = saved && !s->err(); } delete w; delete s;
                if (!saved) return false;
            }
        }
    }
    if (input.err() || input.pos() != input.size()) return false;
    report += cold ? "native-world-story\tphase=cold\tactual_result_restored=1\tcontinued_operation=1\tfull_world_window_spawns=1\tcpu_execution=0\n" :
        "native-world-story\tphase=warm\tchains=2\toperations=4\tactual_World_owners=1\tterrain_projection=1\tfull_world_window_spawns=1\tcpu_execution=0\n"; return true;
}
}
#endif
