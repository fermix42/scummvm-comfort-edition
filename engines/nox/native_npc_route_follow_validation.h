/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_ROUTE_FOLLOW_VALIDATION_H
#define NOX_NATIVE_NPC_ROUTE_FOLLOW_VALIDATION_H
#include "nox/native_npc_route_follow.h"
#include "common/memstream.h"
namespace Nox {
inline bool validateNativeNpcRouteFollow(HdvArchive &archive, Common::SeekableReadStream &input, Common::SaveFileManager &files,
        bool cold, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','R','F','L') || input.readUint32LE() != 2 || input.readUint32LE() != 1493) return false;
    uint total = 0; const char *names[4] = {"nox-native-follow-world.bin","nox-native-follow-schedule.bin","nox-native-follow-store.bin","nox-native-follow-motion.bin"};
    for (uint group = 0; group < (cold ? 1U : 1493U); ++group) {
        byte context[6],actor[8],payload[256],fields[256] = {};
        if (input.read(context,6) != 6) return false;
        const byte actorSlot = input.readByte(),slot = input.readByte(),phase = input.readByte();
        if (actorSlot & 7 || slot >= 14 || phase > 1 || input.read(actor,8) != 8 || input.read(payload,256) != 256) return false;
        const uint count = input.readUint16LE(); Native::World world(archive); Native::NpcScheduleState state; Native::NpcRouteStore store; Native::NpcRouteMotionState motion; motion.movementPhase = phase != 0;
        if (cold) {
            Common::InSaveFile *w = files.openForLoading(names[0]),*s = files.openForLoading(names[1]),*r = files.openForLoading(names[2]),*m = files.openForLoading(names[3]);
            const bool ok = w && s && r && m && world.load(*w) && state.load(*s) && store.load(*r) && motion.load(*m); delete w; delete s; delete r; delete m; if (!ok) return false;
        } else {
            byte pages[3584] = {}; memcpy(pages + slot * 256,payload,256);
            if (!world.initialize(8,7,24,38,fields,256) || !state.initialize(archive) || !store.importPages(pages,3584)) return false;
            Common::MemoryWriteStreamDynamic w(DisposeAfterUse::YES),s(DisposeAfterUse::YES); if (!world.save(w) || !state.save(s)) return false;
            Common::Array<byte> setup; setup.resize(w.size()); memcpy(setup.data(),w.getData(),w.size()); memcpy(setup.data() + 271 + 1280 + actorSlot,actor,8);
            Common::MemoryReadStream ws(setup.data(),setup.size()); if (!world.load(ws)) return false;
            byte npc[572]; memcpy(npc,s.getData(),572); memset(npc + 454,255,56);
            byte *request = npc + 454 + slot * 4; request[0] = actor[4]; request[1] = actorSlot; request[2] = 1; request[3] = slot;
            Common::MemoryReadStream ss(npc,572); if (!state.load(ss)) return false;
        }
        for (uint operation = 0; operation < count; ++operation) {
            const byte expectedDirection = input.readByte(),expectedHidden = input.readByte(),changed = input.readByte(); byte expectedActor[8],expectedRequests[56],expectedPayload[256],blocked[4] = {};
            if (input.read(expectedActor,8) != 8 || input.read(expectedRequests,56) != 56 || input.read(expectedPayload,256) != 256 || changed) return false;
            const byte relativeX = input.readByte(),relativeY = input.readByte(),movementPhase = input.readByte();
            byte direction = 4;
            if (cold && !operation) direction = expectedDirection;
            else if (Native::NpcRouteFollower::advance(world,state,store,actorSlot,blocked,motion,direction) != Native::kNpcRouteFollowAdvanced) return false;
            if (direction != expectedDirection || byte(motion.hidden) != expectedHidden || motion.relativeX != relativeX || motion.relativeY != relativeY || byte(motion.movementPhase) != movementPhase || memcmp(world.currentTables()->data() + 1280 + actorSlot,expectedActor,8) ||
                memcmp(state.requests(),expectedRequests,56) || memcmp(store.data() + slot * 256,expectedPayload,256)) {
                error = Common::String::format("Native active route differs group%u operation%u direction%u/%u hidden%u/%u",group,operation,uint(direction),uint(expectedDirection),uint(motion.hidden),uint(expectedHidden)); return false;
            }
            Common::MemoryWriteStreamDynamic w(DisposeAfterUse::YES),s(DisposeAfterUse::YES),r(DisposeAfterUse::YES);
            if (!world.save(w) || !state.save(s) || !store.save(r)) return false;
            Common::MemoryReadStream ws(w.getData(),w.size()),ss(s.getData(),s.size()),rs(r.getData(),r.size());
            if (!world.load(ws) || !state.load(ss) || !store.load(rs)) return false;
            if (!cold && !group && !operation) {
                for (uint i = 0; i < 4; ++i) {
                    Common::OutSaveFile *out = files.openForSaving(names[i],false); if (!out) return false;
                    bool ok = i == 0 ? world.save(*out) : i == 1 ? state.save(*out) : i == 2 ? store.save(*out) : motion.save(*out);
                    out->finalize(); ok = ok && !out->err(); delete out; if (!ok) return false;
                }
            }
            ++total;
        }
        const byte *actual = world.currentTables()->data() + 1280 + actorSlot;
        if (actual[0] != context[4] || actual[1] != context[5]) return false;
    }
    if (input.err() || (!cold && (input.pos() != input.size() || total != 26943))) return false;
    report += cold ? "native-npc-route-follow\tphase=cold\tactual_result_restored=1\tcontinued_to_goal=1\tpartial_owner_only=1\tcpu_execution=0\n" :
        "native-npc-route-follow\tphase=warm\troutes=1493\toperations=26943\tactual_logical_movement=1\tretained_world_requests_cursor=1\troundtrip_between_operations=1\trecovery_mode=0\tcomplete_manager_turn=0\tcpu_execution=0\n";
    return true;
}
}
#endif
