/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_RECRUITMENT_H
#define NOX_NATIVE_RECRUITMENT_H
#include "nox/native_new_game.h"
#include "common/textconsole.h"
namespace Nox { namespace Native {
enum RecruitmentResult {kRecruitApplied,kRecruitFull,kRecruitRepeated,kRecruitInvalid};
class Recruitment {
public:
    static RecruitmentResult apply(HdvArchive &archive,NewGame &game,byte actorSlot,byte companion) {
        if (!game.ready() || actorSlot>248 || (actorSlot&7) || companion>=64) {warning("Native recruitment invalid line%d actor%u companion%u",__LINE__,uint(actorSlot),uint(companion));return kRecruitInvalid;}
        const uint count=game.party.memberCount();if(count>=6) return kRecruitFull;
        const byte *actor=game.world.currentTables()->data()+1280+actorSlot;const byte role=actor[5]&127;
        if(role<34 || role>=66) {warning("Native recruitment invalid line%d actor%u companion%u",__LINE__,uint(actorSlot),uint(companion));return kRecruitInvalid;}
        const uint flag=34+(role-34)/8;const byte bit=byte(1U<<((role-34)%8));
        if(game.world.field(flag)&bit) return kRecruitRepeated;
        Common::Array<byte> main,data;const byte type=game.world.mapType();
        if(type>8 || !archive.read("NA/GAME",main) || main.size()<0xc6e) {warning("Native recruitment invalid line%d actor%u companion%u",__LINE__,uint(actorSlot),uint(companion));return kRecruitInvalid;}
        const byte family=type?main[0xc65+type]:5;
        if(!archive.read(Common::String::format("NA/NPC.JOIN%u",uint(family)),data) || data.size()<3) {warning("Native recruitment invalid line%d actor%u companion%u",__LINE__,uint(actorSlot),uint(companion));return kRecruitInvalid;}
        const uint equipment=READ_LE_UINT16(data.data()+1);
        if(equipment<3 || (equipment-3)%128 || equipment>data.size()) {warning("Native recruitment invalid line%d actor%u companion%u",__LINE__,uint(actorSlot),uint(companion));return kRecruitInvalid;}
        const uint entries=(equipment-3)/128;
        if(companion>=entries || data.size()!=equipment+entries*16) {warning("Native recruitment invalid line%d actor%u companion%u",__LINE__,uint(actorSlot),uint(companion));return kRecruitInvalid;}
        NewGame next(game);Party &party=next.party;Inventory &inventory=next.inventory;
        if(party._members[count].bytes[0]!=170 && !party._members[count].bytes[69]) {warning("Native recruitment invalid line%d actor%u companion%u",__LINE__,uint(actorSlot),uint(companion));return kRecruitInvalid;}
        memcpy(party._members[count].bytes,data.data()+3+uint(companion)*128,128);
        for(uint i=0;i<16;i+=2) {party._equipment[count].bytes[i]=1;party._equipment[count].bytes[i+1]=0;}
        party._equipment[count].bytes[0]=0;party._equipment[count].bytes[2]=3;
        ItemCatalog catalog;EquipmentOwnership ownership;
        if(!catalog.load(archive) || !ownership.load(archive) || !EquipmentActions::recalculateAll(catalog,party)) {warning("Native recruitment invalid line%d actor%u companion%u",__LINE__,uint(actorSlot),uint(companion));return kRecruitInvalid;}
        const byte *gear=data.data()+equipment+uint(companion)*16;
        for(uint i=0;i<16;i+=2) {
            const byte kind=gear[i],id=gear[i+1];
            if(kind>=3) {party._equipment[count].bytes[i]=kind;continue;}
            if(!id) continue;
            if(kind>1) {warning("Native recruitment invalid line%d actor%u companion%u",__LINE__,uint(actorSlot),uint(companion));return kRecruitInvalid;}
            const InventoryResult granted=inventory.grantOne(catalog,kind,id);
            if(granted==kInventoryNoSpace) continue;
            if(granted!=kInventoryApplied) {warning("Native recruitment invalid line%d actor%u companion%u",__LINE__,uint(actorSlot),uint(companion));return kRecruitInvalid;}
            if(!kind) {if(EquipmentActions::selectWeapon(catalog,ownership,byte(count),id,false,party,inventory)==kWeaponUnsupported) {warning("Native recruitment invalid line%d actor%u companion%u",__LINE__,uint(actorSlot),uint(companion));return kRecruitInvalid;}}
            else if(EquipmentActions::selectArmor(catalog,ownership,byte(count),id,false,party,inventory)==kArmorUnsupported) {warning("Native recruitment invalid line%d actor%u companion%u",__LINE__,uint(actorSlot),uint(companion));return kRecruitInvalid;}
        }
        if(!EquipmentActions::recalculateAll(catalog,party)) {warning("Native recruitment invalid line%d actor%u companion%u",__LINE__,uint(actorSlot),uint(companion));return kRecruitInvalid;}
        byte *record=next.world._states[next.world._current].tables.data()+1280+actorSlot;
        record[3]=255;next.world._fields[flag]|=bit;
        game.adoptState(next);return kRecruitApplied;
    }
};
} }
#endif
