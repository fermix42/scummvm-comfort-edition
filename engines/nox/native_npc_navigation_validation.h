/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_NAVIGATION_VALIDATION_H
#define NOX_NATIVE_NPC_NAVIGATION_VALIDATION_H
#include "nox/native_npc_navigation.h"
#include "nox/native_npc_route_follow.h"
namespace Nox {
inline bool validateNativeNpcNavigation(HdvArchive &archive, Common::SeekableReadStream &input, Common::SaveFileManager &files,
        bool cold, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','O','W','N') || input.readUint32LE() != 1 || input.readUint32LE() != 1495) return false;
    uint completed = 0,refused = 0,searchTicks = 0,moves = 0;
    const char *names[6] = {"nox-native-nav-world.bin","nox-native-nav-window.bin","nox-native-nav-schedule.bin","nox-native-nav-store.bin","nox-native-nav-search.bin","nox-native-nav-rng.bin"};
    for (uint group = 0; group < (cold ? 1U : 1495U); ++group) {
        const byte type = input.readByte(),map = input.readByte(),originX = input.readByte(),originY = input.readByte(),targetX = input.readByte(),targetY = input.readByte(),reachable = input.readByte();
        const uint minimum = input.readUint16LE(); const uint32 seed = input.readUint32LE(); const byte tail = input.readByte(),last = input.readByte(); const uint16 attempts = input.readUint16LE();
        Native::World world(archive); Native::WorldStory window; Native::NpcScheduleState schedule; Native::NpcRouteStore store; Native::NpcNavigation navigation; Native::GameplayRandom random;
        if (cold) {
            Common::InSaveFile *in[6] = {}; bool ok = true;
            for (uint i = 0; i < 6; ++i) { in[i] = files.openForLoading(names[i]); ok = ok && in[i] != nullptr; }
            ok = ok && world.load(*in[0]) && window.load(*in[1],world) && schedule.load(*in[2]) && store.load(*in[3]) && navigation.load(*in[4],archive,world,schedule) && random.load(*in[5]);
            for (uint i = 0; i < 6; ++i) delete in[i];
            if (!ok || !navigation.active() || world.mapType() != type || world.mapId() != map) return false;
        } else {
            byte fields[256] = {},spawns[256] = {};
            if (!world.initialize(type,map,24,24,fields,256) || !schedule.initialize(archive) || !random.importState(seed,tail,last,attempts)) return false;
            Common::MemoryWriteStreamDynamic w(DisposeAfterUse::YES),s(DisposeAfterUse::YES); if (!world.save(w) || !schedule.save(s)) return false;
            Common::Array<byte> data; data.resize(w.size()); memcpy(data.data(),w.getData(),w.size());
            const byte actor[8] = {originX,originY,137,0,77,126,1,0}; memcpy(data.data() + 271 + 1280,actor,8);
            Common::MemoryReadStream ws(data.data(),data.size()); if (!world.load(ws) || !window.initialize(world,spawns,256)) return false;
            byte npc[572]; memcpy(npc,s.getData(),572); npc[6] = originX; npc[166] = originY; memset(npc + 454,255,116);
            const byte pending[6] = {77,0,targetX,targetY,0,0}; memcpy(npc + 510,pending,6);
            Common::MemoryReadStream ss(npc,572); if (!schedule.load(ss)) return false;
        }
        Native::NpcNavigationResult result = Native::kNpcNavigationSearching; uint ticks = 0;
        while (result == Native::kNpcNavigationSearching) {
            if (++ticks > 1024) return false;
            result = navigation.advance(archive,world,window,schedule,store,random); ++searchTicks;
            if (schedule.pathBusy() != byte(result == Native::kNpcNavigationSearching)) return false;
            if (result != Native::kNpcNavigationSearching && result != Native::kNpcNavigationStored && result != Native::kNpcNavigationNoRoute) {
                error = Common::String::format("Native navigation failed case%u tick%u result%u",group,ticks,uint(result)); return false;
            }
            Common::MemoryWriteStreamDynamic s(DisposeAfterUse::YES),r(DisposeAfterUse::YES),n(DisposeAfterUse::YES),g(DisposeAfterUse::YES);
            if (!schedule.save(s) || !store.save(r) || !navigation.save(n) || !random.save(g)) return false;
            Common::MemoryReadStream ss(s.getData(),s.size()),rs(r.getData(),r.size()),ns(n.getData(),n.size()),gs(g.getData(),g.size());
            if (!schedule.load(ss) || !store.load(rs) || !navigation.load(ns,archive,world,schedule) || !random.load(gs)) return false;
            if (!cold && !group && ticks == 1) {
                if (result != Native::kNpcNavigationSearching) return false;
                for (uint i = 0; i < 6; ++i) {
                    Common::OutSaveFile *out = files.openForSaving(names[i],false); if (!out) return false;
                    bool ok = i == 0 ? world.save(*out) : i == 1 ? window.save(*out) : i == 2 ? schedule.save(*out) : i == 3 ? store.save(*out) : i == 4 ? navigation.save(*out) : random.save(*out);
                    out->finalize(); ok = ok && !out->err(); delete out; if (!ok) return false;
                }
            }
        }
        if (!reachable) {
            if (result != Native::kNpcNavigationNoRoute || world.currentTables()->data()[1280] != originX || world.currentTables()->data()[1281] != originY) return false;
            for (uint i = 0; i < 56; ++i) if (schedule.requests()[i] != 255) return false;
            ++refused; continue;
        }
        if (result != Native::kNpcNavigationStored || navigation.active() || schedule.recent()[5] != 255 || schedule.requests()[0] != 77 || schedule.requests()[1] != 0 || schedule.requests()[2] != 1) return false;
        Native::NpcRouteMotionState motion; uint movementCalls = 0,chainMoves = 0;
        Common::Array<byte> game,footer; Native::MapAssetRange range;
        if (!archive.read("NA/GAME",game) || !Native::resolveMapAssets(game,type,range) || !archive.readRange(range.resource,range.offset + range.size,72,footer)) return false;
        while (!(schedule.requests()[2] & 128)) {
            const byte *before = world.currentTables()->data() + 1280; const byte x = before[0],y = before[1]; byte direction = 4,blocked[4] = {};
            if (++movementCalls > 256 || Native::NpcRouteFollower::advance(world,schedule,store,0,blocked,motion,direction) != Native::kNpcRouteFollowAdvanced) return false;
            const byte *after = world.currentTables()->data() + 1280;
            if (direction < 4) {
                if (ABS(int(after[0]) - x) + ABS(int(after[1]) - y) != 1 || after[0] >= 48 || after[1] >= 48) return false;
                const byte tile = window.state()->terrain[after[1] * 48 + after[0]];
                if (!((tile >= footer[22] && tile < footer[23]) || (tile >= footer[24] && tile < footer[25]))) return false;
                ++chainMoves; ++moves;
            } else if (after[0] != x || after[1] != y) return false;
            Common::MemoryWriteStreamDynamic w(DisposeAfterUse::YES),s(DisposeAfterUse::YES),r(DisposeAfterUse::YES),m(DisposeAfterUse::YES);
            if (!world.save(w) || !schedule.save(s) || !store.save(r) || !motion.save(m)) return false;
            Common::MemoryReadStream ws(w.getData(),w.size()),ss(s.getData(),s.size()),rs(r.getData(),r.size()),ms(m.getData(),m.size());
            if (!world.load(ws) || !schedule.load(ss) || !store.load(rs) || !motion.load(ms)) return false;
        }
        const byte *actor = world.currentTables()->data() + 1280;
        if (actor[0] != targetX || actor[1] != targetY || actor[6] != 4 || chainMoves < minimum) return false;
        ++completed;
    }
    if (input.err() || (!cold && (input.pos() != input.size() || completed != 1493 || refused != 2))) return false;
    report += cold ? "native-npc-navigation\tphase=cold\tactual_result_restored=1\tcontinued_search_store_move_to_goal=1\tpartial_owner_only=1\tcpu_execution=0\n" :
        Common::String::format("native-npc-navigation\tphase=warm\tcontrolled_chains=1495\tcompleted_goals=1493\tunreachable_refused=2\tsearch_ticks=%u\tlegal_moves=%u\tretained_search_store_movement=1\troundtrip_between_operations=1\tclock_prefetch_full_turn=0\tcpu_execution=0\n",searchTicks,moves);
    return true;
}
}
#endif
