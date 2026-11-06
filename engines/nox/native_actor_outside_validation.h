/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ACTOR_OUTSIDE_VALIDATION_H
#define NOX_NATIVE_ACTOR_OUTSIDE_VALIDATION_H
#include "nox/native_actor_outside.h"
#include "common/memstream.h"
namespace Nox {
inline bool validateNativeActorOutside(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report,Common::String &error) {
    if (input.readUint32BE()!=MKTAG('N','A','O','S') || input.readUint32LE()!=1 || input.readUint32LE()!=49152 || input.readUint32LE()!=72) return false;
    Native::ActorOutsideRules rules; byte last=255;
    for (uint i=0;i<49152;++i) {
        const byte type=input.readByte(); Native::ActorOutsideProbe probe; probe.actorTile=input.readByte(); const byte unrestricted=input.readByte(),x=input.readByte(),y=input.readByte();
        probe.unrestricted=unrestricted!=0; byte blocked[4]; input.read(probe.terrain,4); input.read(blocked,4);
        if (unrestricted>1 || x<1 || x>46 || y<1 || y>46) return false;
        if (last!=type) { if (!rules.load(archive,type)) return false; last=type; }
        Native::ActorOutsideDecision decision;
        if (!rules.probe(probe,decision) || memcmp(decision.terrain,probe.terrain,4) || memcmp(decision.blocked,blocked,4)) {
            error=Common::String::format("Native off-screen terrain decision differs case%u type%u",i,uint(type)); return false;
        }
    }
    for (uint i=0;i<72;++i) {
        const byte type=input.readByte(),map=input.readByte(),px=input.readByte(),py=input.readByte(),x=input.readByte(),y=input.readByte(),actor=input.readByte(),unrestricted=input.readByte();
        byte terrain[4],blocked[4],fields[256]={},spawns[256]={}; input.read(terrain,4); input.read(blocked,4);
        Native::World world(archive); Native::WorldStory window; Native::ActorOutsideDecision result;
        if (unrestricted>1 || !rules.load(archive,type) || !world.initialize(type,map,px,py,fields,256) || !window.initialize(world,spawns,256) ||
            !rules.fromWindow(world,window,x,y,actor,unrestricted!=0,result) || memcmp(result.terrain,terrain,4) || memcmp(result.blocked,blocked,4)) {
            error=Common::String::format("Native off-screen owned-window decision differs case%u type%u map%u",i,uint(type),uint(map)); return false;
        }
        Common::MemoryWriteStreamDynamic w(DisposeAfterUse::YES),s(DisposeAfterUse::YES); if (!world.save(w) || !window.save(s)) return false;
        Common::MemoryReadStream ws(w.getData(),w.size()),ss(s.getData(),s.size()); if (!world.load(ws) || !window.load(ss,world)) return false;
        if (!rules.fromWindow(world,window,x,y,actor,unrestricted!=0,result) || memcmp(result.terrain,terrain,4) || memcmp(result.blocked,blocked,4)) return false;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report+="native-actor-outside\trule_operations=49152\tmap_styles=8\towned_window_operations=72\tactual_source_directions=196896\tworld_window_roundtrip=1\tcache_edges=0\tfull_actor_turn=0\tcpu_execution=0\n"; return true;
}
}
#endif
