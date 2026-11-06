/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_PATH_VALIDATION_H
#define NOX_NATIVE_NPC_PATH_VALIDATION_H
#include "nox/native_npc_path.h"
#include "common/memstream.h"
#include "common/md5.h"
#include "common/savefile.h"
namespace Nox {
struct NativePathExpected {
    byte status,tail,last,frontier,current,x,y,hashes[64],route[256]; uint16 attempts,nodes; uint32 seed;
    bool read(Common::SeekableReadStream &input) {
        status = input.readByte(); seed = input.readUint32LE(); tail = input.readByte(); last = input.readByte(); attempts = input.readUint16LE();
        nodes = input.readUint16LE(); frontier = input.readByte(); current = input.readByte(); x = input.readByte(); y = input.readByte();
        return status <= 2 && nodes && nodes <= 256 && frontier <= 127 && input.read(hashes,64) == 64 &&
            (status != 1 || input.read(route,256) == 256) && !input.err();
    }
};
inline bool nativePathDigest(const byte *data, uint size, const byte *expected) {
    byte digest[16]; Common::MemoryReadStream stream(data,size); return Common::computeStreamMD5(stream,digest) && !memcmp(digest,expected,16);
}
inline bool compareNativePath(const Native::NpcPathSearch &path, const Native::GameplayRandom &random, const NativePathExpected &expected, uint &difference) {
    difference = 0;
    if (uint(path.status()) != expected.status || path.nodeCount() != expected.nodes || path.frontierCount() != expected.frontier ||
        path.currentNode() != expected.current || path.currentX() != expected.x || path.currentY() != expected.y) return false;
    difference = 1;
    if (random.seed() != expected.seed || random.tail() != expected.tail || random.last() != expected.last || random.generationAttempts() != expected.attempts) return false;
    byte nodes[2048],frontier[254];
    for (uint i = 0; i < path.nodeCount(); ++i) {
        const Native::NpcPathNode *n = path.node(i); byte *to = nodes + i * 8;
        to[0] = byte(i); to[1] = n->parent; to[2] = n->fromX; to[3] = n->fromY; to[4] = n->x; to[5] = n->y; to[6] = to[7] = 170;
    }
    for (uint i = 0; i < path.frontierCount(); ++i) { frontier[i * 2] = path.frontier(i)->distance; frontier[i * 2 + 1] = path.frontier(i)->node; }
    difference = 2; if (!nativePathDigest(path.visited(),1024,expected.hashes)) return false;
    difference = 3; if (!nativePathDigest(nodes,path.nodeCount() * 8,expected.hashes + 16)) return false;
    difference = 4; if (!nativePathDigest(frontier,path.frontierCount() * 2,expected.hashes + 32)) return false;
    difference = 5; if (!nativePathDigest(path.route(),256,expected.hashes + 48)) return false;
    difference = 6; if (expected.status == 1 && memcmp(path.route(),expected.route,256)) return false;
    return true;
}
inline bool saveNativePathCheckpoint(Common::SaveFileManager &files, const Native::World &world, const Native::WorldStory &window,
        const Native::NpcPathSearch &path, const Native::GameplayRandom &random) {
    const char *names[4] = {"nox-native-path-world.bin","nox-native-path-window.bin","nox-native-path-search.bin","nox-native-path-rng.bin"};
    for (uint i = 0; i < 4; ++i) {
        Common::OutSaveFile *output = files.openForSaving(names[i],false); if (!output) return false;
        bool ok = i == 0 ? world.save(*output) : i == 1 ? window.save(*output) : i == 2 ? path.save(*output) : random.save(*output);
        output->finalize(); ok = ok && !output->err(); delete output; if (!ok) return false;
    }
    return true;
}
inline bool validateNativePath(HdvArchive &archive, Common::SeekableReadStream &input, Common::SaveFileManager &files,
        bool cold, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','P','T','H') || input.readUint32LE() != 3 || input.readUint32LE() != 1495) return false;
    uint verified = 0,unverified = 0,totalCalls = 0;
    for (uint caseId = 0; caseId < (cold ? 1U : 1495U); ++caseId) {
        const uint32 seed = input.readUint32LE(); const byte tail = input.readByte(),last = input.readByte(); const uint16 attempts = input.readUint16LE();
        const byte type = input.readByte(),map = input.readByte(),originX = input.readByte(),originY = input.readByte(),targetX = input.readByte(),targetY = input.readByte();
        const byte visitedTail = input.readByte(),routeTail = input.readByte(),frontierTail = input.readByte();
        const uint calls = input.readUint16LE(); if (!calls || calls >= 512 || (!caseId && calls < 2)) return false;
        Native::World world(archive); Native::WorldStory window; Native::NpcPathSearch path; Native::GameplayRandom random;
        if (cold) {
            Common::InSaveFile *w = files.openForLoading("nox-native-path-world.bin"),*v = files.openForLoading("nox-native-path-window.bin"),
                *p = files.openForLoading("nox-native-path-search.bin"),*r = files.openForLoading("nox-native-path-rng.bin");
            const bool loaded = w && v && p && r && world.load(*w) && window.load(*v,world) && path.load(*p,archive) && random.load(*r);
            delete w; delete v; delete p; delete r; if (!loaded || world.mapType() != type || world.mapId() != map) return false;
        } else {
            byte fields[256] = {},spawns[256] = {};
            if (!world.initialize(type,map,24,24,fields,256) || !window.initialize(world,spawns,256) || !path.configure(archive,type) ||
                !path.seedWorkspaceTails(visitedTail,routeTail,frontierTail) || !path.start(originX,originY,targetX,targetY,true) || !random.importState(seed,tail,last,attempts)) return false;
        }
        bool blocked = false; byte finalStatus = 255;
        for (uint call = 0; call < calls; ++call) {
            NativePathExpected expected; if (!expected.read(input)) return false; finalStatus = expected.status;
            if (blocked) continue;
            const Native::NpcPathStatus result = cold && !call ? path.status() : path.advance(window,random);
            // Two recorded raw-map error contexts select an unallocated node,
            // then depend on ROM/error behavior. Keep them unverified; do not
            // synthesize a machine read or inject an expected RNG outcome.
            if (!cold && (caseId == 1010 || caseId == 1017) && result == Native::kNpcPathUnsupported && expected.current >= expected.nodes) {
                blocked = true; ++unverified; continue;
            }
            uint difference = 0;
            if (result == Native::kNpcPathUnsupported || !compareNativePath(path,random,expected,difference)) {
                error = Common::String::format("Native NPC path differs case%u call%u part%u status%u/%u nodes%u/%u frontier%u/%u current%u/%u point%u,%u/%u,%u RNG%08X/%08X",caseId,call,difference,uint(result),uint(expected.status),path.nodeCount(),uint(expected.nodes),uint(path.frontierCount()),uint(expected.frontier),uint(path.currentNode()),uint(expected.current),uint(path.currentX()),uint(path.currentY()),uint(expected.x),uint(expected.y),random.seed(),expected.seed); return false;
            }
            ++totalCalls;
            if (!cold) {
                Common::MemoryWriteStreamDynamic encoded(DisposeAfterUse::YES); if (!path.save(encoded)) return false;
                Native::NpcPathSearch restored; Common::MemoryReadStream saved(encoded.getData(),encoded.size());
                if (!restored.load(saved,archive) || !compareNativePath(restored,random,expected,difference)) return false;
                path = restored;
                if (!caseId && !call && !saveNativePathCheckpoint(files,world,window,path,random)) return false;
            }
        }
        if (blocked) { if (finalStatus != 2) return false; }
        else { if (finalStatus != 1) return false; ++verified; }
    }
    if (input.err() || (!cold && (verified != 1493 || unverified != 2 || input.pos() != input.size()))) return false;
    report += cold ? Common::String::format("native-npc-path\tphase=cold\tactual_result_restored=1\tcontinued_search=1\tcompared_calls=%u\tcomplete_route_bytes=256\tpartial_owner_only=1\tcpu_execution=0\n",totalCalls) :
        Common::String::format("native-npc-path\tphase=warm\tverified_paths=1493\tunverified_source_error_contexts=2\tcompared_calls=%u\tstate_hashes_RNG_every_yield=1\tactual_result_roundtrip_between_calls=1\tcontrolled_zero_goal_hash_profile=1\tnonzero_goal_outer_hash=0\tcomplete_manager_turn=0\tcpu_execution=0\n",totalCalls);
    return true;
}
}
#endif
