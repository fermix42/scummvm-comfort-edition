/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_SCHEDULE_VALIDATION_H
#define NOX_NATIVE_NPC_SCHEDULE_VALIDATION_H
#include "nox/native_new_game.h"
#include "common/memstream.h"
#include "common/savefile.h"
namespace Nox {
inline bool compareNativeNpcState(const Native::NpcScheduleState &state, const byte *goals, const byte *requests,
        const byte *recent, const byte *counters, const byte *selected) {
    if (memcmp(state.goalX(),goals,160) || memcmp(state.goalY(),goals + 160,160) || memcmp(state.requests(),requests,56) ||
        memcmp(state.recent(),recent,60) || state.approachCount() != counters[0] || state.pathBusy() != counters[1]) return false;
    for (uint i = 0; i < 32; ++i) {
        const Native::NpcScheduleSelection *entry = state.selection(i); const byte *expected = selected + i * 5;
        if (!entry || byte(entry->minute != 255) != expected[0] || entry->hour != expected[1] || entry->minute != expected[2] ||
            (entry->minute == 255 ? 65535 : entry->offset) != READ_LE_UINT16(expected + 3)) return false;
    }
    return true;
}
inline bool validateNativeNpcSchedule(HdvArchive &archive, Common::SeekableReadStream &input, Common::SaveFileManager &files, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','S','C','H') || input.readUint32LE() != 1) return false;
    const uint32 count = input.readUint32LE(); if (count != 17352 && count != 17928) return false;
    uint applied = 0,skipped = 0;
    for (uint i = 0; i < count; ++i) {
        const byte type = input.readByte(),map = input.readByte(),x = input.readByte(),y = input.readByte(),hour = input.readByte(),minute = input.readByte(),policyByte = input.readByte(),capProfile = input.readByte();
        byte tables[1792],goals[320],requests[56],recent[60],counters[2],selected[160],fields[256] = {};
        if (input.read(tables,1792) != 1792 || input.read(goals,320) != 320 || input.read(requests,56) != 56 || input.read(recent,60) != 60 || input.read(counters,2) != 2 || input.read(selected,160) != 160) return false;
        Native::World world(archive); Native::NpcScheduleState state;
        if (!world.initialize(type,map,x,y,fields,256) || !state.initialize(archive)) return false;
        if (capProfile) {
            Common::MemoryWriteStreamDynamic encoded(DisposeAfterUse::YES); if (!world.save(encoded)) return false;
            Common::Array<byte> setup; setup.resize(encoded.size()); memcpy(setup.data(),encoded.getData(),encoded.size());
            const byte *records = world.currentTables()->data() + 1280;
            for (uint slot = 0; slot < 256; slot += 8) if (records[slot + 2] && records[slot + 3] != 255) setup[271 + 1280 + slot + 6] = 255;
            Common::MemoryReadStream before(setup.data(),setup.size()); if (!world.load(before)) return false;
        }
        Common::MemoryWriteStreamDynamic encoded(DisposeAfterUse::YES); if (!state.save(encoded) || encoded.size() != 572) return false;
        byte setup[572]; memcpy(setup,encoded.getData(),572); memset(setup + 454,0,4); memset(setup + 510,0,4); setup[570] = 3; setup[571] = 5;
        Common::MemoryReadStream before(setup,572); if (!state.load(before)) return false;
        Native::NpcSchedulePolicy policy = {(policyByte & 1) != 0,(policyByte & 2) != 0};
        const Native::NpcScheduleResult result = Native::NpcScheduler::apply(world,hour,minute,policy,state);
        const bool skip = type < 4 || map == 69;
        if (result != (skip ? Native::kNpcScheduleSkipped : Native::kNpcScheduleApplied) || memcmp(world.currentTables()->data(),tables,1792) ||
            !compareNativeNpcState(state,goals,requests,recent,counters,selected)) {
            error = Common::String::format("Native NPC schedule differs at case%u type%u map%u time%02X:%02X result%u",i,uint(type),uint(map),uint(hour),uint(minute),uint(result)); return false;
        }
        Common::MemoryWriteStreamDynamic savedWorld(DisposeAfterUse::YES),savedNpc(DisposeAfterUse::YES);
        if (!world.save(savedWorld) || !state.save(savedNpc)) return false;
        Native::World restored(archive); Native::NpcScheduleState restoredNpc;
        Common::MemoryReadStream worldInput(savedWorld.getData(),savedWorld.size()),npcInput(savedNpc.getData(),savedNpc.size());
        if (!restored.load(worldInput) || !restoredNpc.load(npcInput) || memcmp(restored.currentTables()->data(),tables,1792) ||
            !compareNativeNpcState(restoredNpc,goals,requests,recent,counters,selected)) return false;
        if (type == 7 && map == 15 && hour == 0 && minute == 0 && !policyByte && !capProfile) {
            if (state.selection(0)->offset != Native::kNpcCastleMarkerData) return false;
            Common::OutSaveFile *w = files.openForSaving("nox-native-castle15-world.bin",false),*n = files.openForSaving("nox-native-castle15-npc.bin",false);
            bool ok = w && n && world.save(*w) && state.save(*n);
            if (w) { w->finalize(); ok = ok && !w->err(); } if (n) { n->finalize(); ok = ok && !n->err(); } delete w; delete n;
            if (!ok) return false;
        }
        applied += !skip; skipped += skip;
    }
    if (input.err() || input.pos() != input.size()) return false;
    report += Common::String::format("native-npc-schedule\tcases=%u\tcovered_maps=%u\tapplied=%u\tsource_caller_skipped=%u\tcanonical_UM_local_type4=1\tlogical_data_offsets=1\tunsupported_castle_padding_maps=%u\tcpu_execution=0\n",count,count == 17352 ? 74 : 75,applied,skipped,count == 17352 ? 2 : 1); return true;
}
inline bool validateNativeNpcScheduleCold(HdvArchive &archive, Common::SeekableReadStream &input, Common::SaveFileManager &files,
        Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','S','C','H') || input.readUint32LE() != 1) return false;
    const uint32 count = input.readUint32LE(); if (count == 17352) return true; if (count != 17928) return false;
    Native::World world(archive); Native::NpcScheduleState npc;
    Common::InSaveFile *w = files.openForLoading("nox-native-castle15-world.bin"),*n = files.openForLoading("nox-native-castle15-npc.bin");
    const bool loaded = w && n && world.load(*w) && npc.load(*n); delete w; delete n; if (!loaded) return false;
    bool restored = false,continued = false;
    for (uint32 i = 0; i < count; ++i) {
        byte context[8],tables[1792],goals[320],requests[56],recent[60],counters[2],selected[160];
        if (input.read(context,8) != 8 || input.read(tables,1792) != 1792 || input.read(goals,320) != 320 ||
            input.read(requests,56) != 56 || input.read(recent,60) != 60 || input.read(counters,2) != 2 || input.read(selected,160) != 160) return false;
        if (context[0] != 7 || context[1] != 15 || context[4] || context[5] > 1 || context[6] || context[7]) continue;
        if (context[5] == 1) {
            if (!restored) return false;
            Native::NpcSchedulePolicy policy = {false,false};
            if (Native::NpcScheduler::apply(world,0,1,policy,npc) != Native::kNpcScheduleApplied) return false;
            continued = true;
        } else restored = true;
        if (world.mapType() != 7 || world.mapId() != 15 || memcmp(world.currentTables()->data(),tables,1792) ||
            !compareNativeNpcState(npc,goals,requests,recent,counters,selected)) {
            error = "Native castle15 cold restore/continued schedule differs from source"; return false;
        }
    }
    if (!restored || !continued || input.err() || input.pos() != input.size()) return false;
    report += "native-castle15-schedule\tphase=cold\tactual_result_restored=1\tcontinued_schedule=1\tsource_next_result=1\ttyped_data_reference=1\tpartial_world_npc_only=1\tcpu_execution=0\n";
    return true;
}
inline bool validateNativeNewGameWorld(HdvArchive &archive, Common::SeekableReadStream &input, Common::SaveFileManager &files,
        bool cold, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','N','W','I') || input.readUint32LE() != 1 || input.readUint32LE() != 1) return false;
    byte tables[1536],goals[320],requests[56],recent[60],counters[2],selected[160];
    if (input.read(tables,1536) != 1536 || input.read(goals,320) != 320 || input.read(requests,56) != 56 || input.read(recent,60) != 60 || input.read(counters,2) != 2 || input.read(selected,160) != 160 || input.err() || input.pos() != input.size()) return false;
    Native::NewGame game(archive); Native::World restored(archive); Native::NpcScheduleState restoredState;
    if (cold) {
        Common::InSaveFile *world = files.openForLoading("nox-native-new-game-world.bin"),*state = files.openForLoading("nox-native-new-game-npc.bin");
        const bool loaded = world && state && restored.load(*world) && restoredState.load(*state); delete world; delete state;
        if (!loaded) return false;
    } else if (!game.create("TRACE",0,0)) return false;
    Native::World &world = cold ? restored : game.world; Native::NpcScheduleState &state = cold ? restoredState : game.schedule;
    if (world.mapType() != 8 || world.mapId() != 7 || world.x() != 24 || world.y() != 38 || memcmp(world.currentTables()->data(),tables,1536) ||
        !compareNativeNpcState(state,goals,requests,recent,counters,selected)) {
        error = "Native new-game world/NPC state differs from actual original first world"; return false;
    }
    if (cold) {
        Native::NpcSchedulePolicy policy = {false,false};
        if (Native::NpcScheduler::apply(world,0,1,policy,state) != Native::kNpcScheduleApplied || memcmp(world.currentTables()->data(),tables,1536) ||
            !compareNativeNpcState(state,goals,requests,recent,counters,selected)) return false;
    } else {
        Common::OutSaveFile *w = files.openForSaving("nox-native-new-game-world.bin",false),*s = files.openForSaving("nox-native-new-game-npc.bin",false);
        bool saved = w && s && world.save(*w) && state.save(*s);
        if (w) { w->finalize(); saved = saved && !w->err(); } if (s) { s->finalize(); saved = saved && !s->err(); } delete w; delete s; if (!saved) return false;
    }
    report += cold ? "native-new-game-world\tphase=cold\tactual_result_restored=1\tcontinued_schedule=1\tpartial_world_npc_only=1\tcpu_execution=0\n" :
        "native-new-game-world\tphase=warm\tactual_NewGame_create=1\tactual_original_first_world=1\tworld_bytes=1536\tgoals_requests_logical_selections=1\tNPC_at_10_8=1\tUI=0\tcpu_execution=0\n"; return true;
}
}
#endif
