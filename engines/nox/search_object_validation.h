/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_SEARCH_OBJECT_VALIDATION_H
#define NOX_SEARCH_OBJECT_VALIDATION_H
namespace Nox {
bool Runtime::campaignFixtureClock(byte hour,byte minute) {
    if(!atWorldInput()||!canSnapshot()||hour>0x23||minute>0x59||(hour&15)>9||(minute&15)>9)return false;
    mainram[0x3878]=hour;mainram[0x3879]=minute;return true;
}
bool Runtime::campaignFixtureWorldEntrance(byte entranceMap,byte entranceType,byte direction,byte entranceColumn,byte entranceRow) {
    if(atWorldInput()&&canSnapshot()&&campaignMapId()==104&&mainram[0x267d]==2){
        if(!_resourceArchive||entranceMap!=104||entranceType!=2||entranceColumn!=122||entranceRow!=48||direction!=11)return false;
        Common::MemoryReadStream stream(_resourceArchive->image().data(),_resourceArchive->image().size());HdvArchive image;Common::Array<byte> map;
        if(!image.open(stream)||!image.read("NA/DATA.MAP.UM",map))return false;uint following=0;
        for(uint at=1;at+23<map.size()&&map[at]!=255;){const uint length=map[at]?map[at]:256;if(length<23||at+length>map.size())return false;if(map[at+1]==104){following=map[at+7]|uint(map[at+8])<<8|uint(map[at+9])<<16;break;}at+=length;}
        if(!following||following+1792>map.size())return false;uint cursor=following+1536;bool owned=false,warp=false;const byte exit[]={122,48,104,2},destination[]={104,122,48,122,80};
        while(cursor+4<=following+1792&&!(map[cursor+2]&128)){if(!memcmp(map.data()+cursor,exit,4))owned=true;cursor+=4;}cursor+=4;
        while(cursor+5<=following+1792&&map[cursor]!=255){if(!memcmp(map.data()+cursor,destination,5))warp=true;cursor+=5;}
        if(!owned||!warp)return false;
        // CONTROLLED source-owned safe adjacency122,49 BEFORE real movement.
        // Never initialize exit contact, warp destination or quest outcome.
        mainram[0x6cec]=mainram[0x23f8]=122;mainram[0x6ced]=mainram[0x23f9]=49;
        push16(0x7127);push16(0x7127);push16(0xf64f);pc=0x8f1b;key_latch=0;keys.clear();worldWaiting=false;return true;
    }
    if(!atWorldInput()||!canSnapshot()||campaignMapId()!=0||mainram[0x267d]!=0||(direction!=8&&direction!=21&&direction!=11&&direction!=10))return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> source;
    if(!image.open(stream)||!image.read("NA/DATA.MAP.SURF",source)||source.size()<25||source[2]<23)return false;
    const uint following=source[9]|uint(source[10])<<8|uint(source[11])<<16;if(following+1792>source.size())return false;
    for(uint slot=0;slot<256;slot+=4){const byte *entry=mainram+0x6ab0+slot;if(entry[2]&128)break;if(entry[2]!=entranceMap||entry[3]!=entranceType||((entranceColumn||entranceRow)&&(entry[0]!=entranceColumn||entry[1]!=entranceRow)))continue;bool owned=false;
        for(uint at=following+1536;at+4<=following+1792;at+=4){if(source[at+2]&128)break;if(!memcmp(entry,source.data()+at,4))owned=true;}if(!owned)return false;
        const int px=int(entry[0])+(direction==8?1:direction==21?-1:0),py=int(entry[1])+(direction==11?1:direction==10?-1:0);if(px<1||px>254||py<1||py>254)return false;
        mainram[0x6cec]=mainram[0x23f8]=byte(px);mainram[0x6ced]=mainram[0x23f9]=byte(py);push16(0x7127);push16(0x7127);push16(0xf64f);pc=0x8f1b;key_latch=0;keys.clear();worldWaiting=false;return true;
    }return false;
}
bool Runtime::campaignPatchPrivateResource(const HdvArchive &archive,const Common::String &name,uint offset,const byte *data,uint count) {
    const HdvArchive::Entry *entry=archive.entry(name);if(!entry||entry->storage!=2||offset>entry->size||count>entry->size-offset)return false;
    Common::Array<byte> prepared=disk;
    for(uint i=0;i<count;++i){uint logical=offset+i,index=logical/512,table=uint(entry->key)*512;if(index>=256||table+512>prepared.size())return false;uint block=prepared[table+index]|uint(prepared[table+256+index])<<8;if(!block||uint64(block)*512+logical%512>=prepared.size())return false;prepared[block*512+logical%512]=data[i];}
    disk=prepared;return true;
}
bool Runtime::campaignFixtureRustyGuardDefeated() {
    if(!atWorldInput()||!canSnapshot()||campaignMapId()!=19||mainram[0x267d]!=3)return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> map;
    const byte guard[]={13,27,158,1};
    if(!image.open(stream)||!image.read("NA/DATA.MAP.RUIN",map)||map.size()<7499||memcmp(map.data()+7495,guard,4)||memcmp(mainram+0xbd04,guard,4))return false;
    // CONTROLLED prior fixed guard death BEFORE door operation. Original
    // actor-world D281 tests BD03,X/CMP255 for fixed and ambient actors.
    // This does not earn a guard fight, key, door, loot or quest result.
    const byte dead=255;if(!campaignPatchPrivateResource(image,"NA/DATA.MAP.RUIN",7498,&dead,1))return false;
    mainram[0xbd07]=255;return true;
}
bool Runtime::campaignFixtureCultSwitchAdjacent(byte column,byte row,byte direction) {
    if(!atWorldInput()||!canSnapshot()||campaignMapId()!=99||mainram[0x267d]!=2||!_resourceArchive||(direction!=10&&direction!=11))return false;
    const byte objects[][8]={{34,200,138,54,31,239,0,16},{57,216,138,54,31,223,0,32},{44,232,138,54,31,191,0,64},{120,213,138,54,31,127,0,128},{118,236,66,19,31,4,0,4}};
    const byte *object=nullptr;for(uint i=0;i<5;++i)if(objects[i][0]==column&&objects[i][1]==row)object=objects[i];if(!object)return false;
    Common::MemoryReadStream stream(_resourceArchive->image().data(),_resourceArchive->image().size());HdvArchive image;Common::Array<byte> map;
    if(!image.open(stream)||!image.read("NA/DATA.MAP.UM",map))return false;uint following=0;
    for(uint at=1;at+23<map.size()&&map[at]!=255;){uint length=map[at]?map[at]:256;if(length<23||at+length>map.size())return false;if(map[at+1]==99){following=map[at+7]|uint(map[at+8])<<8|uint(map[at+9])<<16;break;}at+=length;}
    if(!following||following+1280>map.size())return false;bool source=false;
    for(uint at=following+1024;at+8<=following+1280;++at)if(!memcmp(map.data()+at,object,8))source=true;if(!source)return false;
    const byte py=byte(row+(direction==11?1:-1));
    // CONTROLLED safe adjacency BEFORE a distinct real step/Open. Never place
    // the party on a plate, write its flag, open the door or inject passage.
    for(uint at=0;at<256;at+=8)if(mainram[0xbf02+at]&&mainram[0xbf02+at]!=255&&mainram[0xbf03+at]!=255&&mainram[0xbf00+at]==column&&mainram[0xbf01+at]==py)return false;
    mainram[0x6cec]=mainram[0x23f8]=column;mainram[0x6ced]=mainram[0x23f9]=py;
    push16(0x7127);push16(0x7127);push16(0xf64f);pc=0x8f1b;key_latch=0;keys.clear();worldWaiting=false;return true;
}
bool Runtime::campaignFixtureSearchObject(byte map,byte type,byte column,byte row,byte direction,byte item,uint16 field,byte mask) {
    if(!atWorldInput()||!canSnapshot()||campaignMapId()!=map||mainram[0x267d]!=type||type>8||field<0x400||field>=0x448||!mask||(mask&(mask-1)))return false;
    bool source=false;for(uint at=0;at+12<=256;++at){const byte *object=mainram+0xbe00+at;if(object[0]==column&&object[1]==row&&(object[3]&0xbf)==53&&object[4]==2&&object[5]==item&&object[8]==field-0x400&&object[11]==mask)source=true;}if(!source)return false;
    const int partyColumn=int(column)+(direction==8?1:direction==21?-1:0),partyRow=int(row)+(direction==11?1:direction==10?-1:0),limit=type<3?256:48;
    if((direction!=8&&direction!=21&&direction!=10&&direction!=11)||partyColumn<1||partyRow<1||partyColumn>=limit-1||partyRow>=limit-1)return false;
    for(uint at=0;at<256;at+=8)if(mainram[0xbf02+at]&&mainram[0xbf02+at]!=255&&mainram[0xbf03+at]!=255&&mainram[0xbf00+at]==partyColumn&&mainram[0xbf01+at]==partyRow)return false;
    // CONTROLLED adjacency before Search only. Original redraw loads retained
    // source terrain; caller must then check real floor/vehicle eligibility.
    mainram[0x6cec]=mainram[0x23f8]=byte(partyColumn);mainram[0x6ced]=mainram[0x23f9]=byte(partyRow);
    push16(0x7127);push16(0x7127);push16(0xf64f);pc=0x8f1b;key_latch=0;keys.clear();worldWaiting=false;return true;
}
uint Runtime::campaignKeyDoorState(byte column,byte row,uint16 field,byte mask,byte closedTile) const {
    if(field<0x400||field>=0x448||(closedTile!=18&&closedTile!=59&&closedTile!=62&&closedTile!=65))return 0xffffffffU;
    for(uint at=0;at+8<=256;++at){const byte *object=mainram+0xbe00+at;if(object[0]==column&&object[1]==row&&object[4]==field-0x400&&object[5]==mask&&(object[2]==closedTile||object[2]==closedTile-1)&&(object[3]==18||(object[3]>=33&&object[3]<=41)))return object[2]|uint(object[3])<<8;}return 0xffffffffU;
}
bool Runtime::campaignFixtureKeyDoor(byte map,byte type,byte column,byte row,byte direction,uint16 field,byte mask,byte closedTile) {
    const uint doorState=campaignKeyDoorState(column,row,field,mask,closedTile);
    if(!atWorldInput()||!canSnapshot()||campaignMapId()!=map||mainram[0x267d]!=type||type<2||type>8||!mask||(mask&(mask-1))||(doorState&255)!=closedTile||!((doorState>>8)==18||((doorState>>8)>=33&&(doorState>>8)<=41)))return false;
    const int partyColumn=int(column)+(direction==8?1:direction==21?-1:0),partyRow=int(row)+(direction==11?1:direction==10?-1:0);
    const int limit=type==2?256:48;
    if((direction!=8&&direction!=21&&direction!=10&&direction!=11)||partyColumn<1||partyColumn>=limit-1||partyRow<1||partyRow>=limit-1)return false;
    if(type==2){mainram[0x6cec]=mainram[0x23f8]=byte(partyColumn);mainram[0x6ced]=mainram[0x23f9]=byte(partyRow);push16(0x7127);push16(0x7127);push16(0xf64f);pc=0x8f1b;key_latch=0;keys.clear();worldWaiting=false;return true;}
    return campaignFixtureFloor(map,byte(partyColumn),byte(partyRow));
}
bool Runtime::campaignFixtureDratheAmbientCheckpoint(Common::String &evidence) {
    if(!atWorldInput()||!canSnapshot()||campaignMapId()!=32||mainram[0x267d]!=3||!_resourceArchive||campaignLogCount(154)||(mainram[0x40c]&8))return false;
    Common::MemoryReadStream originalStream(_resourceArchive->image().data(),_resourceArchive->image().size());HdvArchive original;Common::Array<byte> source;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;
    if(!original.open(originalStream)||!original.read("NA/DATA.MAP.RUIN",source)||!image.open(stream))return false;
    uint following=0;for(uint at=1;at+23<source.size()&&source[at]!=255;at+=source[at]){if(source[at]<23)return false;if(source[at+1]==32){following=source[at+7]|uint(source[at+8])<<8|uint(source[at+9])<<16;break;}}
    const byte roaming[]={19,17,153,1,16,12,152,1};const byte actor[]={15,20,165,26,0,126,0,0},quest[]={224,12,0,8,0,2,0,154,0,58,0,128};
    if(!following||following+1296>source.size()||memcmp(source.data()+following+788,roaming,8)||memcmp(source.data()+following+1288,actor,8)||memcmp(source.data()+following+776,quest,12)||memcmp(mainram+0xbf08,actor,8)||memcmp(mainram+0xbd08,quest,12))return false;
    byte fixed[256];memcpy(fixed,mainram+0xbf00,256);
    for(uint slot=20;slot<=24;slot+=4){byte *r=mainram+0xbd00+slot;const byte sprite=slot==20?153:152;if(r[2]!=sprite||(r[3]!=1&&r[3]!=255)||r[0]<1||r[0]>46||r[1]<1||r[1]>46)return false;byte dead[4];memcpy(dead,r,4);dead[3]=255;
        if(!campaignPatchPrivateResource(image,"NA/DATA.MAP.RUIN",following+768+slot,dead,4))return false;r[3]=255;
        evidence+=Common::String::format("CONTROLLED-before-access ordinary-roaming-slot%u sprite%u prior_dead_not_earned=1;",slot,sprite);
    }
    if(memcmp(fixed,mainram+0xbf00,256)||memcmp(mainram+0xbd08,quest,12))return false;
    evidence+=" fixed_Drathe_and_quest_definition_unchanged=1 no_flags_items_HP_MP_door_writes=1\n";
    mainram[0x23f8]=mainram[0x6cec];mainram[0x23f9]=mainram[0x6ced];push16(0x7127);push16(0x7127);push16(0xf64f);pc=0x8f1b;key_latch=0;keys.clear();worldWaiting=false;return true;
}
bool Runtime::campaignFixtureAmbientCheckpoint(Common::String &evidence) {
    if(!atWorldInput()||!canSnapshot()||mainram[0x267d]!=2||!mainram[0x3aff])return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> inventory;
    const uint base=0x1004b;const byte start[]={0xa2,100},dead[]={0xbd,3,0xbd,0xc9,255};
    if(!image.open(stream)||!image.read("NA/SRTN.INVENTORY",inventory)||inventory.size()<base+0x9894-0x9600+sizeof(dead)||memcmp(inventory.data()+base+0x987b-0x9600,start,2)||memcmp(inventory.data()+base+0x9894-0x9600,dead,5))return false;
    // Original procedural encounter slots start at100, stride4; FF is the
    // original reusable dead-slot state. Leave fixed BF quest actors, initial
    // definition region and spawn/action/cooldown fields unchanged. This is
    // CONTROLLED prior ambient kills, never earned combat or a quest result.
    Common::Array<byte> map;const Common::String resource=currentMapResource();if(!image.read(resource,map))return false;uint following=0;
    for(uint at=1;at+23<map.size()&&map[at]!=255;){const uint length=map[at]?map[at]:256;if(length<23||at+length>map.size())return false;if(map[at+1]==campaignMapId()){following=map[at+7]|uint(map[at+8])<<8|uint(map[at+9])<<16;break;}at+=length;}
    if(!following||following+1024>map.size())return false;
    byte fixed[256];memcpy(fixed,mainram+0xbf00,256);uint cleared=0;
    for(uint slot=100;slot<256;slot+=4){byte *actor=mainram+0xbd00+slot;if(!actor[2]||actor[2]==255||actor[3]==255)continue;if(ABS(int(actor[0])-int(positionX()))>32||ABS(int(actor[1])-int(positionY()))>32)continue;
        evidence+=Common::String::format("slot%u xy%u,%u sprite%u flag%u->255;",slot,actor[0],actor[1],actor[2],actor[3]);byte defeatedActor[4];memcpy(defeatedActor,actor,4);defeatedActor[3]=255;if(!campaignPatchPrivateResource(image,resource,following+768+slot,defeatedActor,4))return false;actor[3]=255;++cleared;}
    if(memcmp(fixed,mainram+0xbf00,256))return false;evidence=Common::String::format("CONTROLLED-before-Search-ambient-dead-slots=%u not_earned_kills fixed_quest_actors_unchanged=1 ",cleared)+evidence+"\n";
    if(cleared){mainram[0x23f8]=mainram[0x6cec];mainram[0x23f9]=mainram[0x6ced];push16(0x7127);push16(0x7127);push16(0xf64f);pc=0x8f1b;key_latch=0;keys.clear();worldWaiting=false;}return true;
}
}
#endif
