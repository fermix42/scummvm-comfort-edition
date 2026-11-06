/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ACTOR_CONTACT_VALIDATION_H
#define NOX_NATIVE_ACTOR_CONTACT_VALIDATION_H
#include "nox/native_actor_spatial.h"
namespace Nox {
inline bool validateNativeActorContact(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report,Common::String &error) {
    if (input.readUint32BE()!=MKTAG('N','A','C','P') || input.readUint32LE()!=1 || input.readUint32LE()!=48896) return false;
    Native::ActorSpatialRules rules; Native::ActorSpatialState state; byte last=255; uint battles=0;
    for (uint i=0;i<48896;++i) {
        const byte type=input.readByte(); Native::ActorSpatialContext context;
        context.column=input.readByte(); context.row=input.readByte(); context.tile=input.readByte(); context.kind=input.readByte(); context.capability=input.readByte();
        context.partyObject=input.readByte(); context.previousTerrain=input.readByte(); const byte flags=input.readByte();
        context.large=(flags&1)!=0; context.restrictedTerrain=(flags&2)!=0; context.unrestricted=(flags&4)!=0; context.partyContact=(flags&8)!=0;
        context.actorState=input.readByte(); context.actorFlags=input.readByte(); context.actorSlot=input.readByte();
        Native::NpcContactState contact; contact.availability=input.readByte(); contact.slot=input.readByte(); contact.cell=input.readByte();
        byte before[8],after[8],objects[256]; Native::WorldScene scene;
        input.read(before,8); input.read(scene.actors.terrain,187); input.read(scene.actors.lookup,187); input.read(scene.objects.lookup,187); input.read(objects,256);
        const byte status=input.readByte(),direction=input.readByte(),cell=input.readByte(),terrain=input.readByte(); byte blocked[4]; input.read(blocked,4); input.read(after,8);
        const byte availability=input.readByte(),slot=input.readByte(),requestCell=input.readByte();
        if (type!=last) { if (!rules.load(archive,type)) return false; last=type; memcpy(state.cells,before,8); state.known=255; }
        if (memcmp(state.cells,before,8)) return false;
        Native::ActorSpatialResult result;
        if (!rules.probe(scene,objects,context,state,result,&contact) || uint(result.status)!=status || result.contactDirection!=direction || result.contactCell!=cell || result.effectiveTerrain!=terrain ||
            memcmp(result.blocked,blocked,4) || memcmp(state.cells,after,8) || contact.availability!=availability || contact.slot!=slot || contact.cell!=requestCell) {
            error=Common::String::format("Native combined spatial/contact differs case%u type%u large%u status%u/%u request%u,%u,%u/%u,%u,%u",i,uint(type),uint(context.large),uint(result.status),uint(status),uint(contact.availability),uint(contact.slot),uint(contact.cell),uint(availability),uint(slot),uint(requestCell)); return false;
        }
        if (status==2) ++battles; else if (status) return false;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report+=Common::String::format("native-actor-contact\tcases=48896\tstyles=8\tactual_NPC_request_effects=1\tbattle_handoffs=%u\tbattle_execution=0\tcomplete_actor_turn=0\tcpu_execution=0\n",battles); return true;
}
}
#endif
