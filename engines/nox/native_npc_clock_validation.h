/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_CLOCK_VALIDATION_H
#define NOX_NATIVE_NPC_CLOCK_VALIDATION_H
#include "nox/native_npc_schedule_validation.h"
namespace Nox {
inline bool validateNativeNpcClock(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error, bool reenter = false, bool prefetch = false) {
    const uint32 signature = input.readUint32BE();
    const uint prefetchPasses = signature == MKTAG('N','P','R','2') ? 2 : signature == MKTAG('N','P','R','3') ? 3 : signature == MKTAG('N','P','R','E') ? 1 : 0;
    if ((prefetch ? !prefetchPasses : signature != (reenter ? MKTAG('N','R','E','N') : MKTAG('N','C','L','K'))) || input.readUint32LE() != 1 || input.readUint32LE() != 17928) return false;
    uint changed = 0,skipped = 0;
    for (uint i = 0; i < 17928; ++i) {
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
        const uint minutes = ((hour >> 4) * 10 + (hour & 15)) * 60 + (minute >> 4) * 10 + (minute & 15);
        const uint prior = (minutes + 1439) % 1440,priorHour = prior / 60,priorMinute = prior % 60;
        Native::NpcSchedulePolicy policy = {(policyByte & 1) != 0,(policyByte & 2) != 0};
        const bool skip = type < 4 || map == 69;
        if (Native::NpcScheduler::apply(world,byte(priorHour / 10 * 16 + priorHour % 10),byte(priorMinute / 10 * 16 + priorMinute % 10),policy,state) !=
            (skip ? Native::kNpcScheduleSkipped : Native::kNpcScheduleApplied)) return false;
        byte previous[1792]; memcpy(previous,world.currentTables()->data(),1792);
        const Native::NpcScheduleResult result = Native::NpcScheduler::activateAtClock(world,hour,minute);
        if (prefetch) {
            for (uint pass = 0; pass < prefetchPasses; ++pass) {
                const uint nextTime = (minutes + (pass == 2 ? 1 : 0)) % 1440,hh = nextTime / 60,mm = nextTime % 60;
                const byte clockHour = byte(hh / 10 * 16 + hh % 10),clockMinute = byte(mm / 10 * 16 + mm % 10);
                if (pass && Native::NpcScheduler::activateAtClock(world,clockHour,clockMinute) != (skip ? Native::kNpcScheduleSkipped : Native::kNpcScheduleApplied)) return false;
                if (Native::NpcScheduler::prefetchAtClock(world,clockHour,clockMinute,state) != (skip ? Native::kNpcScheduleSkipped : Native::kNpcScheduleApplied)) return false;
                Common::MemoryWriteStreamDynamic checkpoint(DisposeAfterUse::YES); if (!state.save(checkpoint)) return false;
                Native::NpcScheduleState restored; Common::MemoryReadStream saved(checkpoint.getData(),checkpoint.size());
                if (!restored.load(saved)) return false; state = restored;
            }
        }
        if (reenter && Native::NpcScheduler::apply(world,hour,minute,policy,state) != (skip ? Native::kNpcScheduleSkipped : Native::kNpcScheduleApplied)) return false;
        if (result != (skip ? Native::kNpcScheduleSkipped : Native::kNpcScheduleApplied) || memcmp(world.currentTables()->data(),tables,1792) ||
            !compareNativeNpcState(state,goals,requests,recent,counters,selected)) {
            error = Common::String::format("Native NPC clock activation differs case%u type%u map%u time%02X:%02X result%u",i,uint(type),uint(map),uint(hour),uint(minute),uint(result)); return false;
        }
        changed += memcmp(previous,tables,1792) != 0; skipped += skip;
        Common::MemoryWriteStreamDynamic checkpoint(DisposeAfterUse::YES); if (!world.save(checkpoint)) return false;
        Native::World restored(archive); Common::MemoryReadStream saved(checkpoint.getData(),checkpoint.size());
        if (!restored.load(saved) || memcmp(restored.currentTables()->data(),tables,1792)) return false;
    }
    if (input.err() || input.pos() != input.size()) return false;
    report += Common::String::format("%s\tlinked_entry_to_clock=17928\tcovered_maps=75\tchanged=%u\tsource_skipped=%u\tcastle15_padding_mutations=1\tresult_roundtrip=1\troute_prefetch_calls=%u\tpath_generation=0\tcomplete_AI_turn=0\treentry=%u\tcpu_execution=0\n",reenter ? "native-npc-reentry" : prefetch ? "native-npc-prefetch" : "native-npc-clock",changed,skipped,prefetch ? prefetchPasses : 0U,uint(reenter)); return true;
}
}
#endif
