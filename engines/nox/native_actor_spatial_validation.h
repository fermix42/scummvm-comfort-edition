/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ACTOR_SPATIAL_VALIDATION_H
#define NOX_NATIVE_ACTOR_SPATIAL_VALIDATION_H
#include "nox/native_actor_spatial.h"
#include "common/savefile.h"
namespace Nox {
inline bool validateNativeActorSpatial(HdvArchive &archive, Common::SeekableReadStream &input, Common::SaveFileManager &files,
        bool cold, Common::String &report, Common::String &error) {
    if (input.readUint32BE()!=MKTAG('N','A','S','P') || input.readUint32LE()!=2 || input.readUint32LE()!=48896) return false;
    Native::ActorSpatialRules rules; Native::ActorSpatialState state; byte last=255; uint contacts=0;
    if (cold) { Common::InSaveFile *saved=files.openForLoading("nox-native-spatial-state.bin"); const bool ok=saved && state.load(*saved); delete saved; if (!ok) return false; }
    for (uint i=0; i<48896; ++i) {
        const byte type=input.readByte(); Native::ActorSpatialContext context;
        context.column=input.readByte(); context.row=input.readByte(); context.tile=input.readByte(); context.kind=input.readByte();
        context.capability=input.readByte(); context.partyObject=input.readByte(); context.previousTerrain=input.readByte(); const byte flags=input.readByte();
        context.large=(flags&1)!=0; context.restrictedTerrain=(flags&2)!=0; context.unrestricted=(flags&4)!=0; context.partyContact=(flags&8)!=0;
        Native::WorldScene scene; byte objects[256],before[8],after[8];
        if (input.read(before,8)!=8 || input.read(scene.actors.terrain,187)!=187 || input.read(scene.actors.lookup,187)!=187 || input.read(scene.objects.lookup,187)!=187 || input.read(objects,256)!=256) return false;
        const byte status=input.readByte(),direction=input.readByte(),cell=input.readByte(),terrain=input.readByte(); byte blocked[4]; input.read(blocked,4); input.read(after,8);
        const bool execute=!cold || i>=5248;
        if (type!=last) {
            if (!rules.load(archive,type)) return false; last=type;
            if (execute) { memcpy(state.cells,before,8); state.known=255; } // Explicit controlled initial cache for each style.
        }
        if (!execute) continue;
        if (memcmp(state.cells,before,8)) { error=Common::String::format("Native spatial retained input differs case%u",i); return false; }
        Native::ActorSpatialResult result;
        if (!rules.probe(scene,objects,context,state,result) || uint(result.status)!=status || result.contactDirection!=direction || result.contactCell!=cell ||
            result.effectiveTerrain!=terrain || memcmp(result.blocked,blocked,4) || memcmp(state.cells,after,8)) {
            error=Common::String::format("Native actor spatial differs case%u type%u large%u status%u/%u dir%u/%u terrain%u/%u blocked%u,%u,%u,%u/%u,%u,%u,%u",i,uint(type),uint(context.large),uint(result.status),uint(status),uint(result.contactDirection),uint(direction),uint(result.effectiveTerrain),uint(terrain),uint(result.blocked[0]),uint(result.blocked[1]),uint(result.blocked[2]),uint(result.blocked[3]),uint(blocked[0]),uint(blocked[1]),uint(blocked[2]),uint(blocked[3])); return false;
        }
        if (status) ++contacts;
        Common::MemoryWriteStreamDynamic encoded(DisposeAfterUse::YES); if (!state.save(encoded)) return false;
        Common::MemoryReadStream restored(encoded.getData(),encoded.size()); if (!state.load(restored)) return false;
        if (!cold && i==5247) {
            Common::OutSaveFile *saved=files.openForSaving("nox-native-spatial-state.bin",false); if (!saved) return false;
            bool ok=state.save(*saved); saved->finalize(); ok=ok&&!saved->err(); delete saved; if (!ok) return false;
        }
        Native::ActorSpatialContext clipped=context; clipped.large=true; clipped.column=137;
        Native::ActorSpatialResult untouched=result; Native::ActorSpatialState original=state;
        if (rules.probe(scene,objects,clipped,state,untouched) || untouched.status!=result.status || memcmp(untouched.blocked,result.blocked,4) ||
            untouched.effectiveTerrain!=result.effectiveTerrain || state.known!=original.known || memcmp(state.cells,original.cells,8)) return false;
        Native::ActorSpatialState unknown; clipped=context; clipped.large=true; clipped.column=120; clipped.row=124;
        if (!rules.probe(scene,objects,clipped,unknown,untouched) || !untouched.boundedLookup || !(unknown.known&64) || unknown.cells[6]!=255) return false;
    }
    if ((!cold && contacts!=556) || input.err() || input.pos()!=input.size()) return false;
    report+=cold ? "native-actor-spatial\tphase=cold\tactual_candidate_cache_restored=1\tcontinued_operations=43648\tcpu_execution=0\n" :
        "native-actor-spatial\tphase=warm\tcases=48896\tstyles=8\tcontact_handoffs=556\tretained_candidate_cache=1\tstamped_occupancy=1\tlarge_leading_edges=1\tclipped_large_supported=1\tunknown_candidate_bounded=1\tcontact_execution=0\tcomplete_actor_turn=0\tcpu_execution=0\n"; return true;
}
}
#endif
