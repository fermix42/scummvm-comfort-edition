/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_OWNED_PATH_VALIDATION_H
#define NOX_NATIVE_OWNED_PATH_VALIDATION_H
#include "nox/native_npc_path_validation.h"
namespace Nox {
inline bool validateNativeOwnedPath(HdvArchive &archive, Common::SeekableReadStream &input, Common::SaveFileManager &files,
        bool cold, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','O','W','N') || input.readUint32LE() != 1 || input.readUint32LE() != 1495) return false;
    uint reached = 0,noRoute = 0,totalCalls = 0,maxNodes = 0;
    for (uint caseId = 0; caseId < (cold ? 1U : 1495U); ++caseId) {
        const byte type = input.readByte(),map = input.readByte(),originX = input.readByte(),originY = input.readByte(),targetX = input.readByte(),targetY = input.readByte(),reachable = input.readByte();
        const uint minimum = input.readUint16LE(); const uint32 seed = input.readUint32LE(); const byte tail = input.readByte(),last = input.readByte(); const uint16 attempts = input.readUint16LE();
        if (reachable > 1) return false;
        Native::World world(archive); Native::WorldStory window; Native::NpcPathSearch path; Native::GameplayRandom random;
        const char *names[4] = {"nox-native-owned-path-world.bin","nox-native-owned-path-window.bin","nox-native-owned-path-search.bin","nox-native-owned-path-rng.bin"};
        if (cold) {
            Common::InSaveFile *w = files.openForLoading(names[0]),*v = files.openForLoading(names[1]),*p = files.openForLoading(names[2]),*r = files.openForLoading(names[3]);
            const bool ok = w && v && p && r && world.load(*w) && window.load(*v,world) && path.load(*p,archive) && random.load(*r);
            delete w; delete v; delete p; delete r;
            if (!ok || world.mapType() != type || world.mapId() != map || path.status() != Native::kNpcPathSearching || path.referenceVisitProfile() || path.visitedSize() != 2304) return false;
        } else {
            byte fields[256] = {},spawns[256] = {};
            if (!world.initialize(type,map,24,24,fields,256) || !window.initialize(world,spawns,256) || !path.configure(archive,type) ||
                !path.seedWorkspaceTails(232,255,70) || !path.start(originX,originY,targetX,targetY) || !random.importState(seed,tail,last,attempts)) return false;
        }
        uint calls = 0;
        while (path.status() == Native::kNpcPathSearching) {
            if (++calls > 1024 || path.advance(window,random) == Native::kNpcPathUnsupported) {
                error = Common::String::format("Owned NPC path unsupported case%u call%u",caseId,calls); return false;
            }
            ++totalCalls; if (path.nodeCount() > maxNodes) maxNodes = path.nodeCount();
            if (!cold) {
                Common::MemoryWriteStreamDynamic encoded(DisposeAfterUse::YES),rng(DisposeAfterUse::YES);
                if (!path.save(encoded) || !random.save(rng)) return false;
                Native::NpcPathSearch restored; Native::GameplayRandom restoredRandom;
                Common::MemoryReadStream stateInput(encoded.getData(),encoded.size()),randomInput(rng.getData(),rng.size());
                if (!restored.load(stateInput,archive) || !restoredRandom.load(randomInput)) return false;
                Common::MemoryWriteStreamDynamic check(DisposeAfterUse::YES),checkRandom(DisposeAfterUse::YES);
                if (!restored.save(check) || !restoredRandom.save(checkRandom) || check.size() != encoded.size() || checkRandom.size() != rng.size() ||
                    memcmp(check.getData(),encoded.getData(),encoded.size()) || memcmp(checkRandom.getData(),rng.getData(),rng.size())) return false;
                path = restored; random = restoredRandom;
                if (!caseId && calls == 1) {
                    if (path.status() != Native::kNpcPathSearching) return false;
                    for (uint i = 0; i < 4; ++i) {
                        Common::OutSaveFile *out = files.openForSaving(names[i],false); if (!out) return false;
                        bool ok = i == 0 ? world.save(*out) : i == 1 ? window.save(*out) : i == 2 ? path.save(*out) : random.save(*out);
                        out->finalize(); ok = ok && !out->err(); delete out; if (!ok) return false;
                    }
                }
            }
        }
        if ((path.status() == Native::kNpcPathComplete) != (reachable != 0)) {
            error = Common::String::format("Owned NPC reachability differs from independent BFS case%u status%u expected%u nodes%u",caseId,uint(path.status()),uint(reachable),path.nodeCount()); return false;
        }
        if (!reachable) { if (path.status() != Native::kNpcPathError) return false; ++noRoute; continue; }
        Common::Array<Common::Point> points;
        if (!path.routePoints(points) || points.size() < minimum + 1 || points.front() != Common::Point(originX,originY) || points.back() != Common::Point(targetX,targetY)) return false;
        Common::Array<byte> game,footer; Native::MapAssetRange range;
        if (!archive.read("NA/GAME",game) || !Native::resolveMapAssets(game,type,range) || !archive.readRange(range.resource,range.offset + range.size,72,footer)) return false;
        for (uint i = 1; i < points.size(); ++i) {
            const Common::Point &a = points[i - 1],&b = points[i];
            if (b.x < 0 || b.x >= 48 || b.y < 0 || b.y >= 48 || ABS(a.x - b.x) + ABS(a.y - b.y) != 1) return false;
            const byte tile = window.state()->terrain[b.y * 48 + b.x];
            if (!((tile >= footer[22] && tile < footer[23]) || (tile >= footer[24] && tile < footer[25]))) return false;
        }
        ++reached;
    }
    if (input.err() || (!cold && (input.pos() != input.size() || reached != 1493 || noRoute != 2))) return false;
    report += cold ? Common::String::format("native-owned-path\tphase=cold\tactual_result_restored=1\tcontinued_search=1\tlegal_route_to_authored_goal=1\tremaining_calls=%u\tpartial_owner_only=1\tcpu_execution=0\n",totalCalls) :
        Common::String::format("native-owned-path\tphase=warm\tcontrolled_routes=1495\treachable=1493\tunreachable_refused=2\tindependent_BFS=1\tlegal_cardinal_paths=1\towned_visit_cells=2304\tstate_RNG_roundtrip_between_calls=1\tcooperative_calls=%u\tmax_nodes=%u\toriginal_outer_layout_parity=0\tcomplete_manager_turn=0\tcpu_execution=0\n",totalCalls,maxNodes);
    return true;
}
}
#endif
