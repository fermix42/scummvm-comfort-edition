/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_INN_MEMBERSHIP_H
#define NOX_NATIVE_INN_MEMBERSHIP_H
#include "nox/native_new_game.h"
namespace Nox { namespace Native {
enum InnMembershipResult { kInnMemberApplied,kInnMemberLast,kInnMemberFullStorage,kInnMemberNoWaiting,kInnMemberFullParty,kInnMemberPoor,kInnMemberInvalid };
class InnMembership {
public:
    static InnMembershipResult leave(HdvArchive &archive,NewGame &game,byte member) {
        if (!game.ready() || !game.inn.ready() || member>=game.party.memberCount() || game.party.member(member)->bytes[0]==255) return kInnMemberInvalid;
        const uint count=game.party.memberCount();if (count<2) return kInnMemberLast;
        const int slot=game.inn.firstFree();if (slot<0) return kInnMemberFullStorage;
        Common::Array<byte> source;if (!archive.read("NA/SRTN.NPC.TALK",source) || source.size()!=32831) return kInnMemberInvalid;
        const byte *fists=source.data()+0x2b44+0x672;uint length=0;while (length<19 && fists[length]) ++length;
        if (length==19) return kInnMemberInvalid;++length;
        NewGame next(game);byte *record=next.party._members[member].bytes;
        memcpy(record+90,fists,length);memcpy(record+109,fists,length);
        const byte cleared[]={7,8,9,10,11,21,22,23,64,65};for (uint i=0;i<ARRAYSIZE(cleared);++i) record[cleared[i]]=0;
        next.inn._bytes[uint(slot)*8]=game.world.mapId();memcpy(next.inn._bytes+512+uint(slot)*128,record,128);record[69]=1;
        byte items[1536];if (!next.inventory.exportRecords(items,1536)) return kInnMemberInvalid;
        const byte remove=byte(~(1U<<(member+2)));
        for (uint i=0;i<256;++i) {items[i*6+3]&=remove;items[i*6+4]&=remove;}
        byte *equipment=next.party._equipment[member].bytes;
        for (uint i=0;i<16;i+=2) {equipment[i]=1;equipment[i+1]=0;}equipment[0]=0;equipment[2]=3;
        for (uint at=member;at+1<count;++at) {
            CharacterRecord saved=next.party._members[at];next.party._members[at]=next.party._members[at+1];next.party._members[at+1]=saved;
            EquipmentRecord savedEquipment=next.party._equipment[at];next.party._equipment[at]=next.party._equipment[at+1];next.party._equipment[at+1]=savedEquipment;
            const byte first=byte(1U<<(at+2)),second=byte(first<<1);
            for (uint i=0;i<256;++i) for (uint field=3;field<=4;++field) {
                byte &flags=items[i*6+field];const byte old=flags;flags=byte(old&~(first|second));
                if (old&first) flags|=second;if (old&second) flags|=first;
            }
        }
        if (!next.inventory.importRecords(items,1536)) return kInnMemberInvalid;
        next.food=MIN<uint16>(next.food,uint16((count-1)*150));game.adoptState(next);return kInnMemberApplied;
    }
    static InnMembershipResult pickup(NewGame &game,uint16 fee,byte chosen=255) {
        if (!game.ready() || !game.inn.ready()) return kInnMemberInvalid;
        const int slot=chosen==255?game.inn.firstAt(game.world.mapId()):int(chosen);
        if (slot<0) return kInnMemberNoWaiting;
        if (slot>=12 || game.inn.data()[uint(slot)*8]!=game.world.mapId()) return kInnMemberInvalid;
        const uint count=game.party.memberCount();if (count>=6) return kInnMemberFullParty;
        uint target=0;while (target<6 && !game.party.member(target)->bytes[69]) ++target;
        if (target!=count) return kInnMemberInvalid;
        if (game.gold<fee) return kInnMemberPoor;
        NewGame next(game);next.gold-=fee;byte *record=next.party._members[target].bytes;
        memcpy(record,next.inn._bytes+512+uint(slot)*128,128);record[70]=0;
        // Pickup calls AA70 unconditionally, including a retained dead record;
        // it does not perform Resurrection or normalize the status byte.
        memcpy(record+2,record+72,2);record[4]=record[18];next.inn._bytes[uint(slot)*8]=255;
        game.adoptState(next);return kInnMemberApplied;
    }
};
} }
#endif
