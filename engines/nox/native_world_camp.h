/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_CAMP_H
#define NOX_NATIVE_WORLD_CAMP_H
#include "nox/native_new_game.h"
#include "nox/native_world_presentation.h"
namespace Nox { namespace Native {
enum WorldCampResult { kCampRested,kCampCastleRefused,kCampWaterRefused,kCampEnemiesRefused,kCampUnsupported };
// Original overlay5 (SRTN.INVENTORY97C0) as an owned transaction. Time
// advances eight hours directly; this is not960 artificial world turns.
class WorldCamp {
public:
    static WorldCampResult run(NewGame &game,const WorldScene &scene,Common::String &reply,HdvArchive &archive) {
        if (!game.ready()) return kCampUnsupported;
        const bool special=game.world.mapId()==50 || game.world.mapId()==52;
        const bool aboard=game.appearance>=119 && game.appearance<125;
        if (game.world.mapType()>=4 && !special) return kCampCastleRefused;
        const byte partyGround=scene.actors.terrain[93];
        if (!aboard && partyGround>=229 && partyGround<233) return kCampWaterRefused;
        for (uint cell=0;cell<187;++cell) {
            if (scene.actors.lookup[cell]==255 || scene.actors.kinds[cell]>=2) continue;
            const bool water=scene.actors.terrain[cell]>=229 && scene.actors.terrain[cell]<233;
            if (water==aboard) return kCampEnemiesRefused;
        }
        byte members[768];const uint count=game.party.memberCount();
        for (uint i=0;i<6;++i) {const CharacterRecord *member=game.party.member(i);if (!member) return kCampUnsupported;memcpy(members+i*128,member->bytes,128);}
        uint16 food=game.food;byte outcome=food?2:0;bool recovered=false;
        for (uint i=0;i<count && food;++i) {
            byte *member=members+i*128;if (member[0]==255) continue;
            const uint16 target=uint16(uint32(READ_LE_UINT16(member+72))*75/100),hp=READ_LE_UINT16(member+2);
            if (target>hp) {const uint16 gain=MIN<uint16>(food,target-hp);if (gain<target-hp) outcome=1;food-=gain;WRITE_LE_UINT16(member+2,hp+gain);recovered=true;}
            const uint mp=uint(member[18])*75/100;
            if (food && mp>member[4]) {const uint gain=MIN<uint>(food,mp-member[4]);if (gain<mp-member[4]) outcome=1;food-=gain;member[4]=byte(member[4]+gain);recovered=true;}
        }
        Common::Array<byte> resource;
        if (!archive.read("NA/SRTN.INVENTORY",resource) || resource.size()<0x97c0+1423) return kCampUnsupported;
        const byte *data=resource.data()+0x97c0;
        const uint address=READ_LE_UINT16(data+0x4f2+outcome*2);
        Common::String text;if (!readText(data,1423,address,text)) return kCampUnsupported;
        if (recovered) {Common::String extra;const uint more=READ_LE_UINT16(data+0x4f8+outcome*2);if (more && !readText(data,1423,more,extra)) return kCampUnsupported;text+=extra;}
        if (!game.party.applyBattleMembers(members,768,count)) return kCampUnsupported;
        game.food=food;const uint hour=((game.clock.hour>>4)*10+(game.clock.hour&15)+8)%24;
        game.clock.hour=byte((hour/10)*16+hour%10);game.clock.lightTicks=game.clock.lightActive=0;
        reply=text;return kCampRested;
    }
    static bool refusal(HdvArchive &archive,WorldCampResult result,Common::String &text) {
        Common::Array<byte> data;
        if (result==kCampWaterRefused) {
            if (!archive.read("NA/GAME",data)) return false;
            uint at=0x13c9;while (at<data.size() && data[at]) text+=char(data[at++]&127);return at<data.size();
        }
        if (result!=kCampEnemiesRefused || !archive.read("NA/SRTN.INVENTORY",data) || data.size()<0x97c0+1423) return false;
        return readText(data.data()+0x97c0,1423,0x99cf,text);
    }
private:
    static bool readText(const byte *data,uint size,uint address,Common::String &text) {
        if (address<0x9600 || address-0x9600>=size) return false;
        uint at=address-0x9600;for (;at<size && data[at];++at) {const byte c=data[at]&127;if (c!=13 && c<32) return false;text+=c==13?'\n':char(c);}
        return at<size;
    }
};
} }
#endif
