/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_LARGE_GEOMETRY_VALIDATION_H
#define NOX_NATIVE_WORLD_LARGE_GEOMETRY_VALIDATION_H
#include "nox/native_actor_spatial.h"
namespace Nox {
inline bool validateNativeWorldLargeGeometry(HdvArchive &archive,Common::SeekableReadStream &in,Common::String &report,Common::String &error) {
    if (in.readUint32BE()!=MKTAG('N','W','L','G') || in.readUint32LE()!=1 || in.readUint32LE()!=23936) return false;
    Native::ActorSpatialRules rules;byte type=255;uint guards=0;
    for (uint i=0;i<23936;++i) {
        byte header[7],visibility[187],prior[8],expected[4],objects[256]={};Native::WorldScene scene;
        if (in.read(header,7)!=7 || in.read(scene.actors.terrain,187)!=187 || in.read(scene.actors.lookup,187)!=187 ||
            in.read(scene.objects.lookup,187)!=187 || in.read(visibility,187)!=187 || in.read(prior,8)!=8 || in.read(expected,4)!=4) return false;
        if (header[0]!=type) { if (!rules.load(archive,header[0])) return false;type=header[0]; }
        Native::ActorSpatialContext context;context.column=byte(120+header[1]);context.row=byte(123+header[2]);context.tile=header[3];context.kind=header[4];
        context.capability=0;context.partyObject=255;context.previousTerrain=64;context.actorState=context.kind?16:8;
        context.actorFlags=0;context.actorSlot=16;context.large=true;context.restrictedTerrain=false;context.unrestricted=false;context.partyContact=false;context.visibility=visibility;
        Native::ActorSpatialState state;state.known=255;memcpy(state.cells,prior,8);
        Native::NpcContactState contact;contact.availability=0;Native::ActorSpatialResult result;
        if (!rules.probe(scene,objects,context,state,result,&contact)) { error=Common::String::format("Large world geometry rejected case%u",i);return false; }
        const byte guard=header[6];const uint completed=guard==255?4:guard;
        if (guard!=255 && guard>3) return false;
        for (uint direction=0;direction<completed;++direction) if (result.blocked[direction]!=expected[direction]) {
            error=Common::String::format("Large world geometry differs case%u direction%u native%u original%u",i,direction,result.blocked[direction],expected[direction]);return false;
        }
        if (guard!=255) {
            if (result.blocked[guard]!=2 || !result.boundedLookup) return false;++guards;
        } else if (result.boundedLookup) return false;
        Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES),again(DisposeAfterUse::YES);if (!state.save(saved)) return false;
        Native::ActorSpatialState cold;Common::MemoryReadStream bytes(saved.getData(),saved.size());
        if (!cold.load(bytes) || !cold.save(again) || saved.size()!=again.size() || memcmp(saved.getData(),again.getData(),saved.size())) return false;
    }
    if (guards!=830 || in.err() || in.pos()!=in.size()) return false;
    report+="native-world-large-geometry\tcases=23936\tcomplete_source_vectors=23106\tunsafe_source_prefix_guards=830\tcold_candidate_roundtrips=23936\tcpu_execution=0\n";return true;
}
}
#endif
