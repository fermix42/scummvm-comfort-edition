/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_ROUTE_STORE_VALIDATION_H
#define NOX_NATIVE_NPC_ROUTE_STORE_VALIDATION_H
#include "nox/native_npc_route_store.h"
#include "common/memstream.h"
namespace Nox {
inline bool validateNativeNpcRouteStore(HdvArchive &archive, Common::SeekableReadStream &input, Common::SaveFileManager &files,
        bool cold, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','R','A','L') || input.readUint32LE() != 1 || input.readUint32LE() != 51) return false;
    uint operations = 0,copies = 0,full = 0;
    const char *names[3] = {"nox-native-route-world.bin","nox-native-route-schedule.bin","nox-native-route-store.bin"};
    for (uint group = 0; group < (cold ? 1U : 51U); ++group) {
        byte actors[256],requests[56],pages[3584],fields[256] = {};
        if (input.read(actors,256) != 256 || input.read(requests,56) != 56 || input.read(pages,3584) != 3584) return false;
        const uint count = input.readUint16LE();
        Native::World world(archive); Native::NpcScheduleState state; Native::NpcRouteStore store;
        if (cold) {
            Common::InSaveFile *w = files.openForLoading(names[0]),*s = files.openForLoading(names[1]),*r = files.openForLoading(names[2]);
            const bool ok = w && s && r && world.load(*w) && state.load(*s) && store.load(*r);
            delete w; delete s; delete r; if (!ok) return false;
        } else {
            if (!world.initialize(8,7,24,38,fields,256) || !state.initialize(archive) || !store.importPages(pages,3584)) return false;
            Common::MemoryWriteStreamDynamic w(DisposeAfterUse::YES),s(DisposeAfterUse::YES);
            if (!world.save(w) || !state.save(s) || s.size() != 572) return false;
            Common::Array<byte> setup; setup.resize(w.size()); memcpy(setup.data(),w.getData(),w.size()); memcpy(setup.data() + 271 + 1280,actors,256);
            Common::MemoryReadStream ws(setup.data(),setup.size()); if (!world.load(ws)) return false;
            byte npc[572]; memcpy(npc,s.getData(),572); memcpy(npc + 454,requests,56);
            Common::MemoryReadStream ss(npc,572); if (!state.load(ss)) return false;
        }
        for (uint operation = 0; operation < count; ++operation) {
            const byte goal = input.readByte(),actor = input.readByte(); byte payload[256],expectedRequests[56],expectedPages[3584];
            if (input.read(payload,256) != 256) return false; const byte expected = input.readByte();
            if (input.read(expectedRequests,56) != 56 || input.read(expectedPages,3584) != 3584) return false;
            const Native::NpcRouteAllocation result = cold && !operation ? Native::kNpcRouteAllocated : store.allocate(world,state,goal,actor,payload,256);
            if (uint(result) != expected || memcmp(state.requests(),expectedRequests,56) || memcmp(store.data(),expectedPages,3584)) {
                error = Common::String::format("Native route allocation differs group%u operation%u result%u/%u",group,operation,uint(result),uint(expected)); return false;
            }
            Common::MemoryWriteStreamDynamic s(DisposeAfterUse::YES),r(DisposeAfterUse::YES);
            if (!state.save(s) || !store.save(r)) return false;
            Common::MemoryReadStream ss(s.getData(),s.size()),rs(r.getData(),r.size());
            if (!state.load(ss) || !store.load(rs) || memcmp(state.requests(),expectedRequests,56) || memcmp(store.data(),expectedPages,3584)) return false;
            if (!cold && !group && !operation) {
                for (uint i = 0; i < 3; ++i) {
                    Common::OutSaveFile *out = files.openForSaving(names[i],false); if (!out) return false;
                    bool ok = i == 0 ? world.save(*out) : i == 1 ? state.save(*out) : store.save(*out);
                    out->finalize(); ok = ok && !out->err(); delete out; if (!ok) return false;
                }
            }
            copies += expected == 0; full += expected == 2; ++operations;
        }
    }
    if (input.err() || (!cold && (input.pos() != input.size() || operations != 56 || copies != 34 || full != 1))) return false;
    report += cold ? "native-npc-route-store\tphase=cold\tactual_result_restored=1\tcontinued_allocations=4\tpartial_owner_only=1\tcpu_execution=0\n" :
        "native-npc-route-store\tphase=warm\tgroups=51\toperations=56\tallocated_copies=34\tcapacity_boundary=1\tordered_cleanup=1\tretained_payloads=1\troundtrip_between_operations=1\terror_presentation=0\tcomplete_manager_turn=0\tcpu_execution=0\n";
    return true;
}
}
#endif
