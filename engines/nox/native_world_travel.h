/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_TRAVEL_H
#define NOX_NATIVE_WORLD_TRAVEL_H
#include "nox/native_world_story.h"
#include "nox/native_world_transport.h"
namespace Nox { namespace Native {
enum WorldTravelResult { kTravelNone,kTravelSelected,kTravelSpecialBattle,kTravelInvalid,kTravelParentFallback };
struct WorldTravelPlan {
    byte type,map,x,y,slot;bool explicitArrival;
    WorldTravelPlan() : type(0),map(0),x(0),y(0),slot(255),explicitArrival(false) {}
};
// Typed authored four-byte exit and five-byte arrival metadata. Original code
// coordinates identify the analysis contract only; no instruction dispatch.
class WorldTravelRules {
public:
    static WorldTravelResult enter(byte type,byte map,byte x,byte y,byte field59,const byte *spawns,uint count,WorldTravelPlan &out,bool afterBattle=false) {
        if (!spawns || count!=256) return kTravelInvalid;
        WorldTravelPlan next;next.type=type;next.map=map;next.x=x;next.y=y;
        uint selected=256,end=256;
        for (uint at=0;at<256;at+=4) {
            // Original compares coordinates before testing the terminator.
            if (spawns[at]==x && spawns[at+1]==y) {selected=at;break;}
            if (spawns[at+2]&128) break;
        }
        if (selected==256) {out=next;return kTravelNone;}
        const bool special=map==8 || (!map && x==150 && y==168 && (field59&1));
        if (afterBattle && (!special || map==8)) {out=next;return kTravelInvalid;}
        if (special && !afterBattle) {out=next;return kTravelSpecialBattle;}
        if (spawns[selected+3]&128) {out=next;return kTravelNone;}
        next.slot=byte(selected);next.map=spawns[selected+2];next.type=spawns[selected+3];
        for (uint at=0;at<256;at+=4) if (spawns[at+2]&128) {end=at+4;break;}
        if (end==256) {out=next;return kTravelInvalid;}
        for (uint at=end;at<256;at+=5) {
            if (at+4<256 && spawns[at]==next.map && spawns[at+1]==x && spawns[at+2]==y) {
                next.x=spawns[at+3];next.y=spawns[at+4];next.explicitArrival=true;out=next;return kTravelSelected;
            }
            if (spawns[at]&128) {
                if (!next.type) {out=next;return kTravelInvalid;}
                if (next.type!=2 && type<3) {next.x=24;next.y=38;}
                out=next;return kTravelSelected;
            }
        }
        out=next;return kTravelInvalid;
    }
    static bool lightMode(byte type,byte map,byte hour,byte minute,const byte *darkMaps,uint count,byte &mode) {
        if (!darkMaps || !count || hour>=0x24 || (hour&15)>9 || minute>=0x60 || (minute&15)>9) return false;
        bool dark=type==2,terminated=false;
        for (uint i=0;i<count;++i) { if (darkMaps[i]&128) {terminated=true;break;}if (darkMaps[i]==map) dark=true; }
        if (!terminated) return false;
        const bool night=hour<5 || hour>0x20 || (hour==5 && minute<0x30) || (hour==0x20 && minute>=0x30);
        mode=dark || night?3:1;return true;
    }
    static WorldTravelResult leave(byte type,byte map,byte x,byte y,const byte *spawns,uint count,WorldTravelPlan &out) {
        if (!spawns || count!=256 || type<3) return kTravelInvalid;
        WorldTravelPlan next;next.type=type;next.map=map;next.x=x;next.y=y;
        for (uint at=0;at<256;at+=4) {
            if (spawns[at+3]&128) {
                next.type=spawns[at+3]&127;next.map=spawns[at+2];next.x=spawns[at];next.y=spawns[at+1];next.slot=byte(at);
                out=next;return kTravelSelected;
            }
            if (spawns[at+2]&128) {out=next;return kTravelParentFallback;}
        }
        out=next;return kTravelInvalid;
    }
};
// Resource/changed-map ownership transaction; command and presentation owners
// retain their own continuations and do not expose the private fixture relocator.
class WorldTravel {
public:
    static bool commit(World &world,const WorldTravelPlan &plan,WorldTransportState *transport=nullptr) {
        if (!world.ready()) return false;
        const bool riding=!world.onFoot();World next(world);WorldTransportState controls;TransportCargo cargo;
        if (riding) {
            if (!transport || transport->object==255) return false;controls=*transport;
            // Main bank1 DA96 clears the old map record before storage. All
            // mutations stay in this candidate until loading/presentation wins.
            if (!WorldTransportControl::detachForTravel(next._states[next._current].tables.data()+1024,256,controls,cargo)) return false;
        } else if (transport && transport->object!=255) return false;
        if (!next.travel(plan.type,plan.map,plan.x,plan.y)) return false;
        if (riding && !WorldTransportControl::restoreAfterTravel(next.x(),next.y(),next._states[next._current].tables.data()+1024,256,controls,cargo)) return false;
        world.adoptState(next);if (riding) *transport=controls;return true;
    }
};
} }
#endif
