/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_CLOCK_PIPELINE_VALIDATION_H
#define NOX_NATIVE_NPC_CLOCK_PIPELINE_VALIDATION_H
#include "nox/native_npc_navigation.h"
#include "nox/native_npc_schedule_validation.h"
namespace Nox {
inline bool validateNativeNpcClockPipeline(HdvArchive &archive, Common::SeekableReadStream &input, Common::SaveFileManager &files,
        bool cold, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','C','L','K') || input.readUint32LE() != 1 || input.readUint32LE() != 1) return false;
    const byte type = input.readByte(),map = input.readByte(),x = input.readByte(),y = input.readByte(),hour = input.readByte(),minute = input.readByte(),policyByte = input.readByte(),profile = input.readByte();
    byte tables[1792],goals[320],requests[56],recent[60],counters[2],selected[160],fields[256] = {},spawns[256] = {};
    if (type != 6 || map != 1 || hour != 0x21 || minute != 0x59 || policyByte || profile || input.read(tables,1792) != 1792 || input.read(goals,320) != 320 || input.read(requests,56) != 56 || input.read(recent,60) != 60 || input.read(counters,2) != 2 || input.read(selected,160) != 160 || input.pos() != input.size()) return false;
    Native::World world(archive); Native::WorldStory window; Native::NpcScheduleState schedule; Native::NpcRouteStore store; Native::NpcNavigation navigation; Native::GameplayRandom random;
    const char *names[6] = {"nox-native-pipeline-world.bin","nox-native-pipeline-window.bin","nox-native-pipeline-schedule.bin","nox-native-pipeline-store.bin","nox-native-pipeline-search.bin","nox-native-pipeline-rng.bin"};
    if (cold) {
        Common::InSaveFile *in[6] = {}; bool ok = true;
        for (uint i = 0; i < 6; ++i) { in[i] = files.openForLoading(names[i]); ok = ok && in[i] != nullptr; }
        ok = ok && world.load(*in[0]) && window.load(*in[1],world) && schedule.load(*in[2]) && store.load(*in[3]) && navigation.load(*in[4],archive,world,schedule) && random.load(*in[5]);
        for (uint i = 0; i < 6; ++i) delete in[i];
        if (!ok || !navigation.active() || schedule.pathBusy() != 1) return false;
    } else {
        if (!world.initialize(type,map,x,y,fields,256) || !schedule.initialize(archive) || !random.initializeEntropy(119)) return false;
        Common::MemoryWriteStreamDynamic npc(DisposeAfterUse::YES); if (!schedule.save(npc)) return false;
        byte setup[572]; memcpy(setup,npc.getData(),572); memset(setup + 454,0,4); memset(setup + 510,0,4); setup[570] = 3; setup[571] = 5;
        Common::MemoryReadStream initial(setup,572); if (!schedule.load(initial)) return false;
        const Native::NpcSchedulePolicy policy = {false,false};
        if (Native::NpcScheduler::apply(world,0x21,0x58,policy,schedule) != Native::kNpcScheduleApplied || Native::NpcScheduler::activateAtClock(world,hour,minute) != Native::kNpcScheduleApplied || Native::NpcScheduler::prefetchAtClock(world,hour,minute,schedule) != Native::kNpcScheduleApplied ||
            memcmp(world.currentTables()->data(),tables,1792) || !compareNativeNpcState(schedule,goals,requests,recent,counters,selected) || !window.initialize(world,spawns,256)) return false;
    }
    uint total = 0,busySkips = 0;
    for (uint phase = 0; phase < 2; ++phase) {
        const byte hh = phase ? 0x22 : hour,mm = phase ? 0 : minute;
        if (phase && Native::NpcScheduler::activateAtClock(world,hh,mm) != Native::kNpcScheduleApplied) return false;
        Native::NpcNavigationResult result = Native::kNpcNavigationSearching;
        for (uint tick = 0; result == Native::kNpcNavigationSearching; ++tick) {
            if (tick >= 2048) return false;
            const bool busy = schedule.pathBusy() != 0;
            Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES),after(DisposeAfterUse::YES); if (!schedule.save(before)) return false;
            const Native::NpcScheduleResult prefetched = Native::NpcScheduler::prefetchAtClock(world,hh,mm,schedule);
            if (prefetched != (busy ? Native::kNpcScheduleSkipped : Native::kNpcScheduleApplied) || !schedule.save(after)) return false;
            if (busy) { if (before.size() != after.size() || memcmp(before.getData(),after.getData(),before.size())) return false; ++busySkips; }
            result = navigation.advance(archive,world,window,schedule,store,random); ++total;
            if (result != Native::kNpcNavigationSearching && result != Native::kNpcNavigationStored && result != Native::kNpcNavigationIdle) {
                error = Common::String::format("Native temporal pipeline phase%u tick%u result%u",phase,tick,uint(result)); return false;
            }
            if (schedule.pathBusy() != byte(result == Native::kNpcNavigationSearching)) return false;
            Common::MemoryWriteStreamDynamic s(DisposeAfterUse::YES),r(DisposeAfterUse::YES),n(DisposeAfterUse::YES),g(DisposeAfterUse::YES);
            if (!schedule.save(s) || !store.save(r) || !navigation.save(n) || !random.save(g)) return false;
            Common::MemoryReadStream ss(s.getData(),s.size()),rs(r.getData(),r.size()),ns(n.getData(),n.size()),gs(g.getData(),g.size());
            if (!schedule.load(ss) || !store.load(rs) || !navigation.load(ns,archive,world,schedule) || !random.load(gs)) return false;
            if (!cold && !phase && !tick) {
                if (!navigation.active()) return false;
                for (uint i = 0; i < 6; ++i) {
                    Common::OutSaveFile *out = files.openForSaving(names[i],false); if (!out) return false;
                    bool ok = i == 0 ? world.save(*out) : i == 1 ? window.save(*out) : i == 2 ? schedule.save(*out) : i == 3 ? store.save(*out) : i == 4 ? navigation.save(*out) : random.save(*out);
                    out->finalize(); ok = ok && !out->err(); delete out; if (!ok) return false;
                }
            }
        }
        if (navigation.active() || schedule.pathBusy()) return false;
        for (uint i = 0; i < 10; ++i) if (schedule.recent()[i * 6 + 5] == 0) return false;
    }
    report += cold ? "native-npc-clock-pipeline\tphase=cold\tactual_result_restored=1\tcontinued_to_next_minute=1\tactor_turn=0\tcpu_execution=0\n" :
        Common::String::format("native-npc-clock-pipeline\tphase=warm\tTown1_2159_to_2200=1\tsource_initial_state=1\tretained_generated_routes=1\tbusy_prefetch_skips=%u\tsearch_ticks=%u\tactor_turn=0\tpipeline88_all_maps=0\tcpu_execution=0\n",busySkips,total);
    return true;
}
}
#endif
