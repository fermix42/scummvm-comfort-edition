/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_PORTAL_H
#define NOX_NATIVE_WORLD_PORTAL_H
#include "nox/native_world_travel.h"
#include "nox/native_world_presentation.h"
namespace Nox { namespace Native {
enum WorldPortalResult {kWorldPortalNone,kWorldPortalLocked,kWorldPortalNeedsFoot,kWorldPortalSelected,kWorldPortalInvalid};
class WorldPortal {
public:
    static WorldPortalResult prepare(const World &world,const WorldScene &scene,WorldTravelPlan &out) {
        if (!world.ready()) return kWorldPortalInvalid;
        const byte slot=scene.objects.portal;if (slot==255) return kWorldPortalNone;
        if (slot>244 || (slot&3)) return kWorldPortalInvalid;
        const byte *object=world.currentTables()->data()+1024+slot;
        if (object[3]!=56 || object[0]!=world.x() || object[1]!=world.y() || !object[2] || object[2]==255) return kWorldPortalInvalid;
        PortalDestination destination={0,0,0,0};const GateResult result=portal(object,256-slot,QuestFields(world.questFields(),256),world.onFoot(),destination);
        if (result==kGateLocked) return kWorldPortalLocked;
        if (result==kGateNeedsFootTravel) return kWorldPortalNeedsFoot;
        if (result!=kGateAllowed) return kWorldPortalInvalid;
        WorldTravelPlan next;next.type=destination.mapType;next.map=destination.map;next.x=destination.x;next.y=destination.y;next.slot=slot;next.explicitArrival=true;out=next;return kWorldPortalSelected;
    }
    static bool text(HdvArchive &archive,bool selected,Common::String &out) {
        Common::Array<byte> source;return archive.read("NA/SRTN.COMBAT.S",source) && plainDialogueBody(source,0x86d1+(selected?0x9d20:0x9d10)-0x9c01,out);
    }
};
} }
#endif
