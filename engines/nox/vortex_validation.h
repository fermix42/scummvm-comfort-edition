/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_VORTEX_VALIDATION_H
#define NOX_VORTEX_VALIDATION_H
namespace Nox {
bool Runtime::campaignFixtureRingDefenseInventory() {
    if(!atWorldInput()||!canSnapshot()||campaignMapId()!=74||mainram[0x267d]!=6||mainram[0x70a3]!=1)return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> inventory;
    if(!image.open(stream)||!image.read("NA/SRTN.INVENTORY",inventory)||inventory.size()<0x2ba0)return false;
    const byte *definition=inventory.data()+0x1cc0+118*32;
    if(definition[0]!=45||definition[2]!=0||definition[3]!=1||definition[4]!=1||definition[12]!=6)return false;
    uint slot=0;for(uint at=0xa00;at+6<=0x1000;at+=6){if((inventory[at]&15)==1&&inventory[at+1]==118)return false;if(!slot&&inventory[at]==254)slot=at;}
    if(!slot)return false;for(uint i=0;i<6;++i)if(inventory[slot+i]!=254)return false;
    const HdvArchive::Entry *entry=image.entry("NA/SRTN.INVENTORY");if(!entry||entry->storage!=2)return false;
    // Controlled owned UNREADIED armor118; actual Ready must set owner/defense.
    const byte record[]={0xa1,118,1,0,0,38};Common::Array<byte> prepared=disk;
    for(uint i=0;i<6;++i){uint logical=slot+i,index=logical/512,table=uint(entry->key)*512;if(index>=256||table+512>prepared.size())return false;uint block=prepared[table+index]|uint(prepared[table+256+index])<<8;if(!block||uint64(block)*512+logical%512>=prepared.size())return false;prepared[block*512+logical%512]=record[i];}
    disk=prepared;mainram[0x2bff]=1;return true;
}
bool Runtime::campaignRingDefenseValid() {
    if(!atWorldInput()||!canSnapshot()||mainram[0x70a3]!=1||campaignPartyHealth()!=125||campaignMana()!=110||auxram[0x160b]!=45||auxram[0x1610]!=8||auxram[0x1611]!=14||auxram[0x1612]!=110)return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> data;
    if(!image.open(stream)||!image.read("NA/SRTN.INVENTORY",data)||data.size()<0x2ba0)return false;
    const byte *definition=data.data()+0x1cc0+118*32;if(definition[0]!=45||definition[2]!=0||definition[3]!=1||definition[4]!=1||definition[12]!=6)return false;
    uint count=0;for(uint at=0xa00;at+6<=0x1000;at+=6)if((data[at]&15)==1&&data[at+1]==118){if(data[at+2]!=1||data[at+3]!=4||data[at+4]||data[at+5]!=38)return false;++count;}
    return count==1;
}
bool Runtime::campaignFixtureRingBattleApproach() {
    if(campaignMapId()!=32||mainram[0x267d]!=3||campaignMana()!=110||campaignLogCount(154))return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> data;
    const byte actor[]={15,20,165,26,0,126,0,0},definition[]={224,12,0,8,0,2,0,154,0,58,0,128};
    if(!image.open(stream)||!image.read("NA/DATA.MAP.RUIN",data)||data.size()<48488||memcmp(data.data()+48480,actor,8)||memcmp(data.data()+47968,definition,12)||memcmp(mainram+0xbf08,actor,8)||memcmp(mainram+0xbd08,definition,12))return false;
    return campaignFixtureFloor(32,15,21);
}
bool Runtime::campaignFixtureRingApproach() {
    if(campaignMapId()!=74||mainram[0x267d]!=6||campaignLogCount(155)||campaignRingFound())return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> data;
    const byte door[]={9,10,59,18,46,128,0,0},search[]={9,8,32,53,2,155,0,73,58,127,0,128};
    if(!image.open(stream)||!image.read("NA/DATA.MAP.TOWN",data)||data.size()<50719||memcmp(data.data()+50615,door,8)||memcmp(data.data()+50707,search,12))return false;
    return true; // Source validation only; actual walking preserves timed door.
}
bool Runtime::campaignFixtureFloor(byte map,byte column,byte row) {
    if(atWorldInput()&&canSnapshot()&&campaignMapId()==map&&mainram[0x267d]==2){
        if(!_resourceArchive||column<1||column>254||row<1||row>254)return false;
        Common::MemoryReadStream source(_resourceArchive->image().data(),_resourceArchive->image().size());HdvArchive original;Common::Array<byte> data;
        if(!original.open(source)||!original.read(currentMapResource(),data))return false;
        bool owned=false;for(uint at=1;at+23<data.size()&&data[at]!=255;){const uint n=data[at]?data[at]:256;if(n<23||at+n>data.size())return false;if(data[at+1]==map)owned=true;at+=n;}if(!owned)return false;
        for(uint at=0;at<256;at+=8)if(mainram[0xbf02+at]&&mainram[0xbf02+at]!=255&&mainram[0xbf03+at]!=255&&mainram[0xbf00+at]==column&&mainram[0xbf01+at]==row)return false;
        for(uint at=0;at<256;at+=4)if(mainram[0xbe02+at]&&mainram[0xbe02+at]!=255&&mainram[0xbe00+at]==column&&mainram[0xbe01+at]==row)return false;
        // CONTROLLED owned global-map floor BEFORE an operation; caller checks
        // actual original redraw/safe terrain. No actor, door or flag writes.
        mainram[0x6cec]=mainram[0x23f8]=column;mainram[0x6ced]=mainram[0x23f9]=row;
        push16(0x7127);push16(0x7127);push16(0xf64f);pc=0x8f1b;key_latch=0;keys.clear();worldWaiting=false;return true;
    }
    if(!atWorldInput()||!canSnapshot()||mainram[0x2af9]!=map||mainram[0x267d]<3||mainram[0x267d]>8||column<1||column>=47||row<1||row>=47)return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> data;
    if(!image.open(stream)||!image.read(currentMapResource(),data))return false;
    uint tileOffset=0;for(uint at=1;at+23<data.size()&&data[at]!=255;at+=data[at]){if(data[at]<23)return false;if(data[at+1]==map){tileOffset=data[at+2]|uint(data[at+3])<<8|uint(data[at+4])<<16;break;}}
    if(!tileOffset||tileOffset+2304>data.size())return false;
    const byte tile=data[tileOffset+((row/16)*3+column/16)*256+(row%16)*16+column%16];
    if(tile>=0xe5&&tile<=0xe8)return false;
    for(uint at=0;at<6;at+=2)if(tile>=mainram[0x781+at]&&tile<mainram[0x782+at])return false;
    for(uint at=0;at<256;at+=8)if(mainram[0xbf02+at]&&mainram[0xbf02+at]!=255&&mainram[0xbf03+at]!=255&&mainram[0xbf00+at]==column&&mainram[0xbf01+at]==row)return false;
    mainram[0x6cec]=mainram[0x23f8]=column;mainram[0x6ced]=mainram[0x23f9]=row;
    push16(0x7127);push16(0x7127);push16(0xf64f);pc=0x8f1b;key_latch=0;keys.clear();worldWaiting=false;return true;
}
bool Runtime::campaignFixtureShrineApproach() {
    if(!atWorldInput()||!canSnapshot()||mainram[0x267d]!=8||mainram[0x2af9]!=65||!(mainram[0x6ab4]&128)||(campaignTransportFlags()&0x38)||campaignLogCount(157))return false;
    // Before real Up into the authored activation rectangle21..27,17..23.
    return campaignFixtureFloor(65,24,24);
}
bool Runtime::campaignFixtureDevilsCave() {
    if(!atWorldInput()||!canSnapshot()||mainram[0x2af9]||(mainram[0x267d]&127))return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> surface,underworld;
    const byte entrance[]={69,80,97,2},door[]={69,90,65,18,15,16,0,0};
    if(!image.open(stream)||!image.read("NA/DATA.MAP.SURF",surface)||!image.read("NA/DATA.MAP.UM",underworld)||surface.size()<2208||underworld.size()<7077||memcmp(surface.data()+2204,entrance,4)||memcmp(underworld.data()+7069,door,8))return false;
    for(uint at=0;at<256;at+=4){const byte *exit=mainram+0x6ab0+at;if(exit[2]&128)break;if(memcmp(exit,entrance,4))continue;
        // Controlled source-authored cave entrance. Original F515 performs
        // actual map admission/arrival; key and door outcomes remain untouched.
        mainram[0x6cec]=mainram[0x23f8]=69;mainram[0x6ced]=mainram[0x23f9]=80;push16(0x7127);push16(0x7127);push16(0xf514);pc=0x8f1b;key_latch=0;keys.clear();worldWaiting=false;return true;}
    return false;
}
bool Runtime::campaignFixtureDevilsApproach() {
    if(!atWorldInput()||!canSnapshot()||mainram[0x2af9]!=97||mainram[0x267d]!=2||mainram[0x6cec]!=69||mainram[0x6ced]!=80||campaignDevilsDoor()!=0x1241)return false;
    // Original UM97 compressed sector84 independently decodes69,89 as floor76
    // hex. Door69,90 is over the same north/south corridor. No actor moved.
    for(uint i=0;i<6;i+=2)if(118>=mainram[0x781+i]&&118<mainram[0x782+i])return false;
    for(uint i=0;i<256;i+=8)if(mainram[0xbf00+i]==69&&mainram[0xbf01+i]==89&&mainram[0xbf02+i]&&mainram[0xbf03+i]!=255)return false;
    mainram[0x6cec]=mainram[0x23f8]=69;mainram[0x6ced]=mainram[0x23f9]=89;
    push16(0x7127);push16(0x7127);push16(0xf64f);push16(0x167b);pc=0x8f1b;key_latch=0;keys.clear();worldWaiting=false;return true;
}
bool Runtime::campaignFixtureEssenceApproach() {
    if(!atWorldInput()||!canSnapshot()||mainram[0x2af9]!=97||mainram[0x267d]!=2||mainram[0x6cec]!=69||mainram[0x6ced]!=91||campaignLogCount(149)!=2||campaignDevilsKeyFlags()!=3||campaignLogCount(112)||(campaignResearchFlags()&64))return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> data;
    const byte search[]={71,104,112,53,2,112,0,71,61,191,0,64};
    if(!image.open(stream)||!image.read("NA/DATA.MAP.UM",data)||data.size()<7069||memcmp(data.data()+7057,search,12))return false;
    // Controlled source-valid floor76 hex70,104, before actual Search Right.
    for(uint i=0;i<6;i+=2)if(118>=mainram[0x781+i]&&118<mainram[0x782+i])return false;
    for(uint i=0;i<256;i+=8)if(mainram[0xbf00+i]==70&&mainram[0xbf01+i]==104&&mainram[0xbf02+i]&&mainram[0xbf03+i]!=255)return false;
    mainram[0x6cec]=mainram[0x23f8]=70;mainram[0x6ced]=mainram[0x23f9]=104;
    push16(0x7127);push16(0x7127);push16(0xf64f);push16(0x167b);pc=0x8f1b;key_latch=0;keys.clear();worldWaiting=false;return true;
}
bool Runtime::campaignFixtureHairnetApproach() {
    if(!atWorldInput()||!canSnapshot()||mainram[0x2af9]!=70||mainram[0x267d]!=8||campaignLogCount(113)||(campaignHairnetFlags()&1)||campaignHairnetDoor()!=0x123b)return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> data;
    const byte door[]={12,37,59,18,26,32,0,0},search[]={9,37,154,53,2,113,0,71,63,254,0,1};
    if(!image.open(stream)||!image.read("NA/DATA.MAP.KEEP",data)||data.size()<31430||memcmp(data.data()+31410,door,8)||memcmp(data.data()+31418,search,12))return false;
    return campaignFixtureFloor(70,13,37);
}
bool Runtime::campaignFixtureGuildPrerequisites() {
    if(!atWorldInput()||!canSnapshot()||campaignMargraveKeyFlag()||campaignTombUnlocked()||campaignTombReconciled()||campaignLogCount(84)||campaignLogCount(85))return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> inventory,talk;
    if(!image.open(stream)||!image.read("NA/SRTN.INVENTORY",inventory)||!image.read("NA/DATA.TLK.L012",talk)||talk.empty()||inventory.size()<0x36a0)return false;
    const byte name[]={0xcd,0xe1,0xf2,0xe7,0xf2,0xe1,0xf6,0xe5,0xa0,0xd4,0xef,0xed,0xe2,0xa0,0xcb,0xe5,0xf9,0};
    if(memcmp(inventory.data()+0x2c00+84*32+13,name,sizeof(name)))return false;
    // CONTROLLED source-supported completed Guild/rescue prerequisites, BEFORE
    // actual Margrave conversation. Original L012 actor0 requires432/08 and
    // awards432/10+key84; L00F children32/40 set435/02 when actually rescued.
    // No key, reconciliation, door, Scroll or protection outcome initialized.
    mainram[0x432]|=8;mainram[0x435]|=2;return true;
}
bool Runtime::campaignFixtureTombTime(bool valid,bool removeRescue) {
    if(!atWorldInput()||!canSnapshot()||campaignTombUnlocked()||campaignTombReconciled()||campaignScrollFound())return false;
    // Source L00E actor0 H window03:27..04:00, clue03:30. Both times are valid
    // BCD.03:15 is the relevant early refusal, while both elders are scheduled.
    mainram[0x3878]=3;mainram[0x3879]=valid?0x30:0x15;
    if(removeRescue)mainram[0x435]&=byte(~2);
    return true;
}
bool Runtime::campaignFixtureTombApproach(bool unlocked) {
    if(campaignTombUnlocked()!=unlocked||campaignTombReconciled()!=unlocked||(unlocked&&campaignLogCount(84)!=1)||campaignLogCount(85)||campaignScrollFound()||campaignTombDoor()!=0x123b)return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> data;
    const byte door[]={10,10,59,18,42,1,0,0},search[]={10,8,32,53,2,85,0,70,28,254,0,1};
    if(!image.open(stream)||!image.read("NA/DATA.MAP.TOWN",data)||data.size()<42993||memcmp(data.data()+42889,door,8)||memcmp(data.data()+42981,search,12))return false;
    // Outside the actual locked door; no inside-tomb position injection.
    return campaignFixtureFloor(38,10,11);
}
bool Runtime::campaignFixtureWaterObjectApproach() {
    if(!atWorldInput()||!canSnapshot()||mainram[0x2af9]!=101||mainram[0x267d]!=2||campaignLogCount(85)!=1||!campaignScrollFound()||campaignWaterCondition()||campaignLogCount(153))return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> data;
    const byte record[]={117,145,186,54,12,251,0,4};
    if(!image.open(stream)||!image.read("NA/DATA.MAP.UM",data)||data.size()<49954||memcmp(data.data()+49946,record,8))return false;
    // Independently decoded south adjacency117,146 is original floor76 hex.
    for(uint i=0;i<6;i+=2)if(118>=mainram[0x781+i]&&118<mainram[0x782+i])return false;
    for(uint i=0;i<256;i+=8)if(mainram[0xbf00+i]==117&&mainram[0xbf01+i]==146&&mainram[0xbf02+i]&&mainram[0xbf03+i]!=255)return false;
    mainram[0x6cec]=mainram[0x23f8]=117;mainram[0x6ced]=mainram[0x23f9]=146;
    push16(0x7127);push16(0x7127);push16(0xf64f);push16(0x167b);pc=0x8f1b;key_latch=0;keys.clear();worldWaiting=false;return true;
}
bool Runtime::campaignFixtureOmitFireJewel() {
    if(!atWorldInput()||!canSnapshot()||campaignMapId()!=98||(mainram[0x439]&7)!=7||campaignLogCount(151)!=1||campaignPartyHealth()!=125)return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> inventory,underworld;
    const byte award[]={224,57,0,1,0,2,0,151,0,63,0,1};
    if(!image.open(stream)||!image.read("NA/SRTN.INVENTORY",inventory)||!image.read("NA/DATA.MAP.UM",underworld)||inventory.size()<0x1000||underworld.size()<49462||memcmp(underworld.data()+49450,award,12))return false;
    uint slot=0;for(uint at=0xa00;at+6<=0x1000;at+=6)if((inventory[at]&15)==2&&inventory[at+1]==151){if(slot||inventory[at+2]!=1||inventory[at+3])return false;slot=at;}
    const HdvArchive::Entry *entry=image.entry("NA/SRTN.INVENTORY");if(!slot||!entry||entry->storage!=2)return false;Common::Array<byte> prepared=disk;
    // CONTROLLED negative prerequisite, BEFORE movement: omit owned Fire
    // Jewel and its matching acquisition/power bit. No tested result seeded.
    for(uint i=0;i<6;++i){uint logical=slot+i,index=logical/512,table=uint(entry->key)*512;if(index>=256||table+512>prepared.size())return false;uint block=prepared[table+index]|uint(prepared[table+256+index])<<8;if(!block||uint64(block)*512+logical%512>=prepared.size())return false;prepared[block*512+logical%512]=254;}
    disk=prepared;mainram[0x439]&=254;return true;
}
bool Runtime::campaignFixtureTempleOutside() {
    if(!atWorldInput()||!canSnapshot()||campaignMapId()!=98||mainram[0x267d]!=2||campaignLogCount(154)!=1||!campaignDarkKeyFlag()||(mainram[0x438]&64)||campaignLogCount(156))return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> data;
    const byte actor[]={168,155,170,8,0,126,0,0},definition[]={192,56,0,64,0,2,0,156,53,50,176,1};
    if(!image.open(stream)||!image.read("NA/DATA.MAP.UM",data)||data.size()<16348||memcmp(data.data()+16340,actor,8)||memcmp(data.data()+15828,definition,12)||memcmp(mainram+0xbf18,actor,8)||memcmp(mainram+0xbd18,definition,12))return false;
    // CONTROLLED outside SOUTH floor168,160, independently decoded original
    // UM98 floor118. Never initialize inside the closed temple rectangle.
    for(uint i=0;i<6;i+=2)if(118>=mainram[0x781+i]&&118<mainram[0x782+i])return false;
    for(uint i=0;i<256;i+=8)if(mainram[0xbf00+i]==168&&mainram[0xbf01+i]==160&&mainram[0xbf02+i]&&mainram[0xbf03+i]!=255)return false;
    mainram[0x6cec]=mainram[0x23f8]=168;mainram[0x6ced]=mainram[0x23f9]=160;
    push16(0x7127);push16(0x7127);push16(0xf64f);push16(0x167b);pc=0x8f1b;key_latch=0;keys.clear();worldWaiting=false;return true;
}
bool Runtime::campaignFixtureLichLore(byte book) {
    const uint flag=book==70?0x40b:book==105?0x41d:0x419;const byte mask=book==70?8:book==105?16:64;
    if(!atWorldInput()||!canSnapshot()||(book!=70&&book!=105&&book!=111)||campaignLogCount(book)||(mainram[flag]&mask))return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> inventory,castle;
    const byte search70[]={18,23,120,53,2,70,0,65,11,247,0,8},search105[]={14,13,120,53,2,105,0,70,29,239,0,16},search111[]={47,12,120,53,2,111,0,70,25,143,0,64};
    const uint source=book==70?31427:book==105?70739:58972;const byte *search=book==70?search70:book==105?search105:search111;
    if(!image.open(stream)||!image.read("NA/SRTN.INVENTORY",inventory)||!image.read("NA/DATA.MAP.CSTL",castle)||castle.size()<source+12||memcmp(castle.data()+source,search,12)||inventory.size()<0x2c00+uint(book+1)*32)return false;
    const byte *definition=inventory.data()+0x2c00+book*32;const byte name[]={0xcc,0xe9,0xe3,0xe8,0xe5,0xf3};
    const byte jewelName[]={0xc1,0xe2,0xef,0xf5,0xf4,0xa0,0xca,0xe5,0xf7,0xe5,0xec,0xf3};
    if(definition[1]!=(book==70?9:5)||definition[6]!=(book==70?66:book==105?41:48)||definition[8]!=192||(book==111?memcmp(definition+13,jewelName,sizeof(jewelName)):memcmp(definition+13,name,sizeof(name))))return false;
    uint slot=0;for(uint at=0xa00;at+6<=0x1000;at+=6)if(inventory[at]==254){slot=at;break;}
    if(!slot)return false;for(uint i=0;i<6;++i)if(inventory[slot+i]!=254)return false;
    const HdvArchive::Entry *entry=image.entry("NA/SRTN.INVENTORY");if(!entry||entry->storage!=2)return false;
    // CONTROLLED source-supported owned selected lore book and matching acquisition flag,
    // BEFORE actual Use. Source Search packs definition byte1 in the upper nibble;
    // known, unreadied misc record follows the actual Scroll acquisition ABI.
    const byte record[]={byte((definition[1]<<4)|2),book,1,0,0,32};Common::Array<byte> prepared=disk;
    for(uint i=0;i<6;++i){uint logical=slot+i,index=logical/512,table=uint(entry->key)*512;if(index>=256||table+512>prepared.size())return false;uint block=prepared[table+index]|uint(prepared[table+256+index])<<8;if(!block||uint64(block)*512+logical%512>=prepared.size())return false;prepared[block*512+logical%512]=record[i];}
    disk=prepared;mainram[flag]|=mask;return true;
}
bool Runtime::campaignFixtureNoon() {
	if(!atWorldInput()||!canSnapshot())return false;
	// Controlled valid BCD clock, before any tested campaign operation. Map
	// loading subsequently runs the original NPC schedules. No actor is moved.
	mainram[0x3878]=0x12;mainram[0x3879]=0;return true;
}
bool Runtime::campaignFixtureSevenPages() {
	if(!atWorldInput()||!canSnapshot()||campaignPageFlags()||campaignLogCount(97)||campaignLogCount(12)||(campaignResearchFlags()&3)||auxram[0x1645])return false;
	Common::MemoryReadStream image(disk.data(),disk.size());HdvArchive archive;Common::Array<byte> inventory,castle;
	if(!archive.open(image)||!archive.read("NA/SRTN.INVENTORY",inventory)||!archive.read("NA/DATA.MAP.CSTL",castle))return false;
	const uint definition=0x2c00+97*32;
	if(inventory.size()<definition+32||inventory[definition+1]!=2||inventory[definition+8]!=128)return false;
	// Validate all seven authored sources supporting this controlled prerequisite.
	const uint offsets[]={42975,42987,42999,43011,58888,58900,58912};
	const byte masks[]={4,8,16,32,64,128,1};
	for(uint i=0;i<7;++i){const uint at=offsets[i];if(at+12>castle.size()||(castle[at+3]&0xbf)!=53||castle[at+4]!=2||castle[at+5]!=97||castle[at+8]!=62||castle[at+11]!=masks[i])return false;}
	uint slot=0;
	for(uint at=0xa00;at+6<=0x1000;at+=6)if(inventory[at]==254){slot=at;break;}
	if(!slot)return false;
	for(uint i=0;i<6;++i)if(inventory[slot+i]!=254)return false;
	const HdvArchive::Entry *entry=archive.entry("NA/SRTN.INVENTORY");
	if(!entry||entry->storage!=2||slot+6>entry->size)return false;
	Common::Array<byte> prepared=disk;
	// Original six-byte stacked quest-item shape: category2/kind2, ID97,
	// quantity7, no owner/equipment, quest-item flags20. Observed from the real
	// Search writer and checked against progressionHandoff's item record fields.
	const byte record[]={0x22,97,7,0,0,0x20};
	for(uint i=0;i<6;++i){const uint logical=slot+i,index=logical/512,table=uint(entry->key)*512;
		if(index>=256||table+512>prepared.size())return false;
		const uint block=prepared[table+index]|uint(prepared[table+256+index])<<8;
		if(!block||uint64(block)*512+logical%512>=prepared.size())return false;
		prepared[block*512+logical%512]=record[i];
	}
	// Atomic controlled initialization BEFORE Axea is addressed. No later award
	// or research/binding bit is seeded. Original party and original HDV retained.
	disk=prepared;mainram[0x43e]=253;
	return campaignLogCount(97)==7&&campaignPageFlags()==253;
}
bool Runtime::campaignFixtureActor(byte map,byte actor,byte &direction,byte preferredDirection) {
	if(!atWorldInput()||!canSnapshot()||mainram[0x2af9]!=map||(actor&7))return false;
	const byte column=mainram[0xbf00+actor],row=mainram[0xbf01+actor];
	if((!column&&!row)||!mainram[0xbf02+actor]||mainram[0xbf02+actor]==255)return false;
	const byte type=mainram[0x267d];
	byte partyColumn=column,partyRow=byte(row+1);direction=11;
	if(type>=3){
		if(column>=48||row>=47)return false;
		Common::MemoryReadStream image(disk.data(),disk.size());HdvArchive archive;Common::Array<byte> data;
		if(!archive.open(image)||!archive.read(currentMapResource(),data)||data.size()<24)return false;
		uint32 tileOffset=0;bool found=false;
		for(uint cursor=1;cursor+23<data.size();){
			const uint length=data[cursor]?data[cursor]:256;if(data[cursor]==255)break;
			if(length<23||cursor+length>data.size())return false;
			if(data[cursor+1]==map){tileOffset=data[cursor+2]|uint32(data[cursor+3])<<8|uint32(data[cursor+4])<<16;found=true;break;}cursor+=length;
		}
		if(!found||tileOffset+2304>data.size())return false;
		const int dx[]={0,1,-1,0},dy[]={1,0,0,-1};const byte directions[]={11,8,21,10};bool placed=false;
		for(uint candidate=0;candidate<4&&!placed;++candidate){
            if(preferredDirection&&directions[candidate]!=preferredDirection)continue;
			int px=column+dx[candidate],py=row+dy[candidate];if(px<1||px>=47||py<1||py>=47)continue;
			const byte tile=data[tileOffset+((py/16)*3+px/16)*256+(py%16)*16+px%16];
			bool denied=tile>=0xe5&&tile<=0xe8;
			for(uint i=0;i<6;i+=2)denied|=tile>=mainram[0x781+i]&&tile<mainram[0x782+i];
			denied|=type==7&&tile>=0x77&&tile<0x79;
			for(uint i=0;i<256;i+=8)if(mainram[0xbf00+i]==px&&mainram[0xbf01+i]==py&&mainram[0xbf02+i]&&mainram[0xbf02+i]!=255)denied=true;
			if(!denied){partyColumn=px;partyRow=py;direction=directions[candidate];placed=true;}
		}if(!placed)return false;
	}
	mainram[0x6cec]=mainram[0x23f8]=partyColumn;mainram[0x6ced]=mainram[0x23f9]=partyRow;
	push16(0x7127);push16(0x7127);push16(0xf64f);
	// LWR.MAIN.RTNS:167C is the original bank-safe location loader. After
	// GAME:8F1B derives the new camera/sector, reload actual map sectors for an
	// underworld fixture. This preserves all earned quest/inventory state.
	if(type==2)push16(0x167b);
	pc=0x8f1b;key_latch=0;keys.clear();worldWaiting=false;return true;
}
#if defined(_MSC_VER)
__declspec(noinline)
#endif
bool NoxEngine::validateVortexChain(bool cold,Common::String &error) {
	Common::String trace;
	auto report=[&](const Common::String &name,const Common::String &text){Common::OutSaveFile *out=getSaveFileManager()->openForSaving(name,false);if(!out)return false;out->write(text.c_str(),text.size());out->finalize();bool ok=!out->err();delete out;return ok;};
	auto frame=[&](const Common::String &name){_runtime->render(_surface,0);Common::OutSaveFile *out=getSaveFileManager()->openForSaving(name,false);if(!out)return false;bool ok=Image::writePNG(*out,_surface);out->finalize();ok&=!out->err();delete out;return ok;};
	auto fail=[&](const Common::String &why){error=why;Common::OutSaveFile *memory=getSaveFileManager()->openForSaving("nox-vortex-failure-memory.bin",false);if(memory){_runtime->campaignDiagnosticMemory(*memory);memory->finalize();delete memory;}report("nox-vortex-failure-site.tsv",Common::String::format("PC=%04X\n",_runtime->address()));if(canSaveGameStateCurrently())saveGameState(2,"Diagnostic failure, not acceptance");warning("Nox Vortex chain: %s",why.c_str());report("nox-vortex-failure.tsv",why+"\n"+_runtime->campaignConversationDiagnostic()+"\n"+_runtime->battleDiagnostic()+Common::String::format("\ntransport_flags=%02X ankh_quantity=%u portal_records=%u\n",_runtime->campaignTransportFlags(),_runtime->campaignLogCount(157),_runtime->campaignPortalCount())+_runtime->campaignMachineDiagnostic()+"\n"+_runtime->campaignActorDiagnostic(40)+"\n"+_runtime->campaignActorDiagnostic(88)+"\n"+_runtime->campaignActorDiagnostic(16)+"\n"+_runtime->campaignActorDiagnostic(56)+"\n");report("nox-vortex-trace.tsv",trace);Common::String replies;for(const auto &character:_runtime->enhancements().characters())for(const auto &reply:character.replies)replies+=Common::String::format("%s\t%u\t%llu\t%s\t%s\n",character.resource.c_str(),character.record,(unsigned long long)reply.sequence,reply.input.c_str(),reply.response.c_str());report("nox-vortex-replies.tsv",replies);frame("nox-vortex-failure.png");return false;};
	const int phase=ConfMan.getInt("nox_vortex_phase");
    const byte loreBook=ConfMan.hasKey("nox_lore_book")?ConfMan.getInt("nox_lore_book"):105;
    if(loreBook!=70&&loreBook!=105&&loreBook!=111)return fail("Invalid source lore book");
	const int scenario=ConfMan.hasKey("nox_vortex_scenario")?ConfMan.getInt("nox_vortex_scenario"):0;
	const bool checkOnly=ConfMan.hasKey("nox_vortex_check_only")&&ConfMan.getBool("nox_vortex_check_only");
    const bool earnedPages=ConfMan.hasKey("nox_vortex_earned_pages")&&ConfMan.getBool("nox_vortex_earned_pages");
    const bool prebinding=phase==38&&ConfMan.hasKey("nox_late_prebinding")&&ConfMan.getBool("nox_late_prebinding");
    const bool opening=phase==38&&ConfMan.hasKey("nox_late_opening")&&ConfMan.getBool("nox_late_opening");
	if(_targetName!="nox-gameplay-test"||!Common::CEAchievements().getLinkToken().empty()||phase<1||(phase>8&&phase!=11&&phase!=12&&phase!=13&&phase!=14&&phase!=15&&phase!=16&&phase!=17&&(phase<18||phase>38))||scenario<0||scenario>3)return fail("Invalid offline Vortex diagnostic");
    auto lateFields=[&](const char *stage){
        const Common::String prefix=Common::String("nox_late_")+stage;const int fields=ConfMan.getInt(prefix+"_field_count"),items=ConfMan.getInt(prefix+"_item_count");
        if(fields<1||fields>16||items<0||items>32)return false;
        for(int i=0;i<fields;++i){const Common::String key=prefix+Common::String::format("_field_%d_",i);const int field=ConfMan.getInt(key+"address"),mask=ConfMan.getInt(key+"mask"),value=ConfMan.getInt(key+"value");if(field<0x400||field>=0x448||mask<1||mask>255||value<0||value>255||(value&~mask)||(_runtime->campaignFlagByte(field)&mask)!=uint(value))return false;}
        for(int i=0;i<items;++i){const Common::String key=prefix+Common::String::format("_item_%d_",i);const int id=ConfMan.getInt(key+"id"),quantity=ConfMan.getInt(key+"quantity");if(id<0||id>255||quantity<0||quantity>255||_runtime->campaignLogCount(byte(id))!=uint(quantity))return false;}
        return true;
    };
	auto stateLine=[&](const char *label){trace+=Common::String::format("%s\t%04X\tmap=%u\txy=%u,%u\tpages=%u\thp=%u\tpageflags=%02X\tresearch=%02X\tbook=%u\t%s\n",label,_runtime->address(),_runtime->campaignMapId(),_runtime->positionX(),_runtime->positionY(),_runtime->campaignLogCount(97),_runtime->campaignPartyHealth(),_runtime->campaignPageFlags(),_runtime->campaignResearchFlags(),_runtime->campaignLogCount(12),_runtime->campaignMachineDiagnostic().c_str());};
	auto advance=[&](){bool ok=_runtime->advance(100000,true);_runtime->audio().clear();return ok;};
    uint ambientFlees=0;
    auto settle=[&](const char *label){
        bool fleeing=false;uint prompts=0;
        const bool allowFlee=phase==38&&ConfMan.hasKey("nox_world_flee_ambient")&&ConfMan.getBool("nox_world_flee_ambient");
        for(uint i=0;i<12000&&!shouldQuit();++i){
            if(fleeing&&!_runtime->inBattle())_runtime->enableBattleValidation(false);
            if(_runtime->atWorldInput()&&_runtime->canSnapshot()){
                if(fleeing){++ambientFlees;_runtime->enableBattleValidation(false);trace+="actual-ambient-Escape-world-return no_state_initialization=1\n";}
                stateLine(label);return true;
            }
            if(phase==38){
                if(_runtime->inBattle()){
                    if(!allowFlee||_runtime->campaignMapId()!=32)return false;
                    _runtime->enableBattleValidation();
                    if(_runtime->battleInputReady()){
                        const uint pc=_runtime->address();
                        if((pc==0xd53f||pc==0xfde9)&&_runtime->battlePlayerTurn()){
                            if(fleeing)_runtime->enableBattleValidation(false); // Original Escape redraw may retain battle type until world return.
                            else{
                                if(_runtime->campaignBattlePrimarySprite()==165)return false;
                                trace+="actual-ambient-before-Escape\t"+_runtime->battleDiagnostic()+"\n";
                                if(++prompts>16)return false;
                                _runtime->key(27);fleeing=true;
                            }
                        }else if(_runtime->campaignSceneInputReady()){if(++prompts>16)return false;_runtime->key(13);}
                        else if(pc==0x1324){if(++prompts>16)return false;_runtime->key(fleeing?'Y':'T');}
                        else _runtime->enableBattleValidation(false); // Let transient read/clear execute, without injecting a key.
                    }
                }else if(_runtime->campaignSceneInputReady()){
                    _runtime->key(13);trace+="actual-original-scene-acknowledgement\n";
                }
            }
            if(!(i%128))stateLine(label);
            if(!advance())return false;
        }
        return false;
    };
	auto setupExit=[&](byte id,byte type){stateLine("setup-exit-before");if(!_runtime->dialogueFixtureExit(id,type))return false;return settle("setup-exit-after");};
	auto replyContains=[&](const char *resource,const char *phrase,uint64 after){for(const auto &character:_runtime->enhancements().characters())if(character.resource==resource)for(const auto &reply:character.replies)if(reply.sequence>after){Common::String text=reply.response;text.toLowercase();Common::String needle=phrase;needle.toLowercase();if(text.contains(needle))return true;}return false;};
	auto save=[&](int slot,const Common::String &label){return saveGameState(slot,label).getCode()==Common::kNoError&&saveGameState(slot,label).getCode()==Common::kNoError;};
	auto load=[&](const Common::String &label){
		int handle=-1;for(const auto &entry:getMetaEngine()->listSaves(_targetName.c_str(),false))if(entry.getDescription()==label)handle=entry.getSaveSlot();if(handle<0)return false;
		const Common::String file=getSaveStateName(handle);Saves::Entry entry;Common::Array<byte> payload;
		if(!Saves::readGeneration(*getSaveFileManager(),file.substr(0,file.size()-3),file.lastChar()-'0',_targetName,entry,payload)||loadGameState(handle).getCode()!=Common::kNoError)return false;
		Common::MemoryWriteStreamDynamic bytes(DisposeAfterUse::YES);
		return _runtime->saveState(bytes)&&bytes.size()==uint(payload.size()-entry.offset)&&!memcmp(bytes.getData(),payload.data()+entry.offset,bytes.size())&&Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName)==entry.metadata.session;
	};
    if(ConfMan.hasKey("nox_transport_portal_cold")&&ConfMan.getBool("nox_transport_portal_cold")){
        if(phase!=5||!cold||!load("Max portal created")||_runtime->campaignMapId()!=68||_runtime->campaignPortalCount()!=1||_runtime->positionX()!=25||_runtime->positionY()!=19||(_runtime->campaignTransportFlags()&0x38)!=0x38)return fail("Cold actual created portal restoration failed");
        _runtime->enableDialogueValidationPolling();const byte walk[]={8,8,10};
        for(uint i=0;i<3;++i){_runtime->key(walk[i]);if(!settle("cold-created-portal-walk"))return fail("Cold portal walk failed");}
        if(_runtime->campaignMapId()!=8||_runtime->positionX()!=81||_runtime->positionY()!=168||_runtime->campaignLogCount(157)!=1)return fail("Cold actual created portal entry failed");
        return report("nox-vortex-portal-cold.tsv","PASS\truntime_bytes=identical\tactual_cold_portal_entry=1\n");
    }
	if(scenario){
		const Common::String label=checkOnly?Common::String::format("Vortex scenario %d",scenario):(scenario<3?Common::String("Vortex controlled seven pages"):Common::String("Vortex phase 1"));
		if(!cold||!load(label))return fail("Scenario exact predecessor restoration failed");
		const uint flags=scenario<3?253:255,quantity=scenario<3?7:8;
		const uint research=checkOnly&&scenario==3?2:0,book=checkOnly&&scenario==3?1:0;
		if(_runtime->campaignPageFlags()!=flags||_runtime->campaignLogCount(97)!=quantity||uint(_runtime->campaignResearchFlags()&3)!=research||_runtime->campaignLogCount(12)!=book)return fail("Scenario precondition/persistent fields differ");
		if(checkOnly)return report(Common::String::format("nox-vortex-scenario-cold-%d.tsv",scenario),"PASS\truntime_bytes=identical\tprovenance=retained-gameplay-with-controlled-map-setups\n");
	}else if(cold&&phase==1&&!checkOnly&&earnedPages){
        if(!load("Seven original pages")||_runtime->campaignPageFlags()!=253||_runtime->campaignLogCount(97)!=7||_runtime->campaignLogCount(12))return fail("Actual seven-Search predecessor differs");
	}else if(cold){
		const int from=checkOnly?phase:(phase==30||phase==32)?28:(phase==31||phase==33)?29:(phase==26||phase==27)?21:phase==7?5:phase==13?11:phase==17?14:(phase==18||phase==24||phase==25)?16:(phase==22||phase==23)?18:phase-1;const Common::String predecessor=ConfMan.hasKey("nox_ring_replay")&&ConfMan.getBool("nox_ring_replay")?Common::String::format("Ring effect approach %u",phase==30?1u:0u):ConfMan.hasKey("nox_ring_ready_defense")&&ConfMan.getBool("nox_ring_ready_defense")?Common::String::format("Vortex phase %u",phase==30?32u:33u):ConfMan.hasKey("nox_ring_defense_fixture")&&ConfMan.getBool("nox_ring_defense_fixture")?Common::String("Diagnostic failure, not acceptance"):!checkOnly&&(phase==4||phase==19)&&ConfMan.hasKey("nox_transport_late_predecessor")?ConfMan.get("nox_transport_late_predecessor"):!checkOnly&&phase==38?ConfMan.get("nox_late_predecessor"):!checkOnly&&(phase==36||phase==37)?Common::String("Vortex phase 34"):!checkOnly&&phase==34?Common::String("Dark Key gate result"):!checkOnly&&(phase==28||phase==29)?Common::String("Guardian 2 result"):!checkOnly&&(phase==4||phase==6||phase==11||phase==12)?Common::String("Vortex world cast"):Common::String::format("Vortex phase %d",from);if(from<1||!load(predecessor))return fail("Exact predecessor cold restoration failed");
		if(!opening&&(_runtime->campaignPageFlags()!=255||_runtime->campaignLogCount(97)!=8))return fail("Predecessor pages differ");
		if(!opening&&from>=2&&!(_runtime->campaignResearchFlags()&1))return fail("Research predecessor missing");
		if(!opening&&!prebinding&&from>=3&&(!(_runtime->campaignResearchFlags()&2)||_runtime->campaignLogCount(12)!=1))return fail("Binding predecessor missing");
		if(checkOnly&&phase>=4&&phase<=8){const int expected=phase==4?0x18:(phase==5||phase==7||phase==8)?0x38:0;if((_runtime->campaignTransportFlags()&0x38)!=expected||_runtime->campaignLogCount(157)!=(phase==6?0u:1u)||((phase==5||phase==8)&&(_runtime->campaignMapId()!=8||_runtime->positionX()!=81||_runtime->positionY()!=168))||(phase==7&&(_runtime->campaignMapId()!=68||_runtime->positionX()!=23||_runtime->positionY()!=20)))return fail("Cold transport fields differ");}
        if(checkOnly&&(phase==12||phase==13)){const bool success=phase==13;if(_runtime->campaignMapId()!=97||_runtime->positionX()!=69||_runtime->positionY()!=(success?91:89)||_runtime->campaignLogCount(149)!=(success?2u:0u)||_runtime->campaignDevilsKeyFlags()!=(success?3:0)||(success?!_runtime->campaignDevilsDoorOpen():_runtime->campaignDevilsDoor()!=0x1241u))return fail("Cold actual Devils Key gate fields differ");}
        if(checkOnly&&(phase==28||phase==29)){const bool success=phase==28;if(_runtime->campaignMapId()!=74||_runtime->positionX()!=9||_runtime->positionY()!=(success?9:11)||(!success&&_runtime->campaignRingUnlocked())||_runtime->campaignRingFound()!=success||_runtime->campaignLogCount(155)!=(success?1u:0u))return fail("Cold actual Ring gate fields differ");}
        if(checkOnly&&(phase==32||phase==33)&&(!_runtime->campaignRingDefenseValid()||_runtime->campaignRingFound()!=(phase==32)))return fail("Cold actual defensive Ready fields differ");
        if(checkOnly&&(phase==36||phase==37)&&(_runtime->campaignMapId()!=98||_runtime->positionX()!=168||_runtime->positionY()!=157||_runtime->campaignPartyHealth()!=(phase==37?90u:125u)||_runtime->campaignLogCount(151)!=(phase==37?0u:1u)||_runtime->campaignLogCount(152)!=1||_runtime->campaignLogCount(153)!=1||_runtime->campaignLogCount(156)))return fail("Cold actual Temple/Fire hazard result differs");
        if(opening&&!checkOnly&&(!((predecessor=="Controlled Captain log checkpoint"&&_runtime->campaignMapId()==7)||(predecessor=="Vortex phase 38"&&(_runtime->campaignMapId()==0||_runtime->campaignMapId()==7)))||_runtime->campaignLogCount(150)!=1||_runtime->campaignLogCount(97)||_runtime->campaignLogCount(12)||_runtime->campaignPartyHealth()!=50))return fail("Original opening predecessor differs");
        if(checkOnly&&phase==38&&(_runtime->campaignMapId()!=ConfMan.getInt("nox_late_map")||!lateFields("after")))return fail("Cold actual late NPC result differs");
        if(checkOnly&&phase==27&&(_runtime->campaignLogCount(loreBook)!=1||_runtime->campaignLogCount(85)!=1))return fail("Cold controlled Lich lore inventory differs");
        if(checkOnly&&phase==26&&(_runtime->campaignMapId()!=101||_runtime->positionX()!=117||_runtime->positionY()!=146||!_runtime->campaignWaterCondition()||_runtime->campaignLogCount(85)!=1||_runtime->campaignLogCount(153)))return fail("Cold original Water condition operation differs");
        if(checkOnly&&phase>=18&&phase<=25){
            const bool key=phase!=24&&phase!=25,unlocked=phase>=19&&phase<=21,scroll=phase==20||phase==21;
            if(_runtime->campaignLogCount(84)!=(key?1u:0u)||_runtime->campaignMargraveKeyFlag()!=key||_runtime->campaignTombUnlocked()!=unlocked||_runtime->campaignTombReconciled()!=unlocked||_runtime->campaignLogCount(85)!=(scroll?1u:0u)||_runtime->campaignScrollFound()!=scroll)return fail("Cold Margrave/Tomb/Scroll fields differ");
        }
        if(checkOnly&&phase>=15&&phase<=17){const bool key=phase!=17;if(_runtime->campaignLogCount(133)!=(key?2u:0u)||_runtime->campaignFooleryKeyFlag()!=key)return fail("Cold Foolery key differs");if(phase!=15&&(_runtime->campaignMapId()!=70||_runtime->positionX()!=(key?10:13)||_runtime->positionY()!=37||_runtime->campaignLogCount(113)!=(key?1u:0u)||bool(_runtime->campaignHairnetFlags()&1)!=key))return fail("Cold Hairnet outcome differs");}
        if(checkOnly&&phase==14&&(_runtime->campaignMapId()!=97||_runtime->campaignLogCount(112)!=1||(_runtime->campaignResearchFlags()&67)!=67||_runtime->campaignLogCount(149)!=2||_runtime->campaignDevilsKeyFlags()!=3))return fail("Cold Essence result differs");
        if(checkOnly&&phase==11&&(_runtime->campaignLogCount(149)!=2||_runtime->campaignDevilsKeyFlags()!=3))return fail("Cold actual Devils Key fields differ");
        if(checkOnly&&phase!=21&&phase!=27&&phase!=34)return report(Common::String::format("nox-vortex-cold-%d.tsv",phase),"PASS\truntime_bytes=identical\tprovenance=controlled-map-setups-linked-gameplay\n");
	}
	_runtime->enableDialogueValidationPolling();
    if(prebinding&&(_runtime->campaignPageFlags()!=255||_runtime->campaignLogCount(97)!=8||!(_runtime->campaignResearchFlags()&1)||_runtime->campaignLogCount(12)!=uint((_runtime->campaignResearchFlags()&2)?1:0)))return fail("Actual researched eight-page/binding consistency differs");
	if(phase==38)_runtime->enableCampaignSceneObservation();
	Common::CEAchievements().noteDisqualifyingAssistance("nox_controlled_campaign_checkpoint");
	auto converse=[&](byte map,byte actor,const Common::Array<Common::String> &topics){
		const bool strictWords=phase==38&&ConfMan.hasKey("nox_late_strict_words")&&ConfMan.getBool("nox_late_strict_words");
		if(strictWords)_runtime->enableStrictCampaignWords();
		byte direction=0;stateLine("setup-actor-before");trace+=_runtime->campaignActorDiagnostic(actor)+"\n";
		bool adjacent=false;
        if(phase==38&&ConfMan.hasKey("nox_late_current_npc")&&ConfMan.getBool("nox_late_current_npc")){if(_runtime->campaignMapId()!=map||!_runtime->atWorldInput()||!_runtime->canSnapshot()||!_runtime->campaignActorDirection(actor,direction))return false;adjacent=true;trace+="actual-retained-NPC-adjacency no_position_clock_NPC_control=1\n";}
		for(uint attempt=0;attempt<8&&!adjacent;++attempt){
			if(!_runtime->campaignFixtureActor(map,actor,direction,phase==38&&ConfMan.hasKey("nox_late_actor_direction")?byte(ConfMan.getInt("nox_late_actor_direction")):0)||!settle("setup-actor-after"))return false;
			trace+=_runtime->campaignActorDiagnostic(actor)+"\n";
			adjacent=_runtime->campaignActorDirection(actor,direction);
		}
		if(!adjacent)return false;
		uint topic=0;uint64 sentSequence=_runtime->enhancements().sequence(),lastSequence=sentSequence,lastPoll=_runtime->emptyKeyboardPolls();bool escaped=false,unobservedPrompt=false;
        if(phase==38&&ConfMan.hasKey("nox_late_bump_mouth")&&ConfMan.getBool("nox_late_bump_mouth")){
            if(map!=97||actor!=16||!ConfMan.hasKey("nox_late_current_npc")||!ConfMan.getBool("nox_late_current_npc")||_runtime->positionX()!=134||_runtime->positionY()!=72||_runtime->campaignKeyDoorState(133,72,0x42e,32,65)!=0x1241)return false;
            _runtime->key(8);trace+="actual-closed-door-Left-bump-auto-conversation no_position_NPC_control=1\n";
        }else{_runtime->key('T');_runtime->key(direction);}
		for(uint i=0;i<16000&&!shouldQuit();++i){
			if(ConfMan.hasKey("nox_late_battle")&&ConfMan.getBool("nox_late_battle")&&_runtime->inBattle()){stateLine("actual-NPC-battle-admission");return topic==topics.size();}
			if(_runtime->atWorldInput()&&_runtime->canSnapshot()){stateLine("conversation-completed");return topic==topics.size();}
			if(_runtime->campaignSceneInputReady()){_runtime->key(13);trace+="actual-NPC-scene-acknowledgement\n";continue;}
			const uint64 sequence=_runtime->enhancements().sequence();if(sequence!=lastSequence){lastSequence=sequence;lastPoll=_runtime->emptyKeyboardPolls();stateLine("resolved-reply");}
			// Strict word cases acknowledge only the qualified word/scene boundaries.
            // A fallback Return during animation can repeat completed dialogue.
			if(((map==74||strictWords)&&_runtime->campaignConversationInputReady())||(!strictWords&&_runtime->emptyKeyboardPolls()-lastPoll>=32)){
				if((map==74||strictWords)&&!_runtime->campaignConversationInputReady()){
					if(_runtime->atKeyboardPoll()){_runtime->key(13);lastPoll=_runtime->emptyKeyboardPolls();}
				}else
				if(sequence>sentSequence){
					if(topic<topics.size()){for(const char *key=topics[topic++].c_str();*key;++key)_runtime->key(*key);_runtime->key(13);sentSequence=sequence;lastPoll=_runtime->emptyKeyboardPolls();}
					else if(!escaped){_runtime->key(27);escaped=true;lastPoll=_runtime->emptyKeyboardPolls();}
					else if(_runtime->atKeyboardPoll()&&!_runtime->atWorldInput()){_runtime->key(13);lastPoll=_runtime->emptyKeyboardPolls();}
				}else if(_runtime->atKeyboardPoll()&&!_runtime->atWorldInput()){
					if(!unobservedPrompt){frame("nox-vortex-unobserved-prompt.png");stateLine("unobserved-input-prompt");unobservedPrompt=true;}
					_runtime->key(13);lastPoll=_runtime->emptyKeyboardPolls();}
			}
			if(!(i%128))stateLine("conversation-running");if(!advance())return false;
		}return false;
	};
	Common::Array<Common::String> topics;
	if(scenario){
		const uint64 before=_runtime->enhancements().sequence();
		if(scenario==1){
			if(!setupExit(0,0)||!setupExit(54,7)||!setupExit(78,7))return fail("Missing-page research map setup failed");
			topics.push_back("EXIST");topics.push_back("SPELL");
			if(!converse(78,88,topics)||!replyContains("NA/DATA.TLK.L015","don't yet have all 8 pages",before))return fail("Valess missing-page refusal failed");
		}else{
			if(scenario==3&&(!setupExit(0,128)||!setupExit(54,7)))return fail("Alternate binding surface return failed");
			if(scenario==2&&(!setupExit(0,0)||!setupExit(54,7)))return fail("Missing-page binding surface setup failed");
			if(!setupExit(78,7)||!setupExit(97,2)||!setupExit(104,2))return fail("Binding scenario map setup failed");
			topics.push_back("BIND");
			if(!converse(104,16,topics))return fail("Binding scenario conversation failed");
			if(scenario==2&&!replyContains("NA/DATA.TLK.L068","don't think any mages",before))return fail("Nystul missing-page refusal failed");
			if(scenario==3){
				if(!replyContains("NA/DATA.TLK.L068","bind together",before)||(_runtime->campaignResearchFlags()&3)!=2||_runtime->campaignLogCount(12)!=1)return fail("Explicit binding without research failed");
				const uint64 repeat=_runtime->enhancements().sequence();
				if(!converse(104,16,topics)||replyContains("NA/DATA.TLK.L068","bind together",repeat)||_runtime->campaignLogCount(12)!=1)return fail("Alternative binding duplicated award");
			}
		}
		if(scenario<3&&((_runtime->campaignResearchFlags()&3)||_runtime->campaignLogCount(12)||_runtime->campaignPageFlags()!=253||_runtime->campaignLogCount(97)!=7))return fail("Refusal changed page/research/binding outcome");
		stateLine("scenario-completed");
		if(!save(1,Common::String::format("Vortex scenario %d",scenario)))return fail("Scenario durable save failed");
		report("nox-vortex-trace.tsv",trace);
		return report(Common::String::format("nox-vortex-scenario-%d.tsv",scenario),Common::String::format("PASS\tscenario=%d\tpages=%u\tpageflags=%u\tresearch=%u\tbook=%u\n",scenario,_runtime->campaignLogCount(97),_runtime->campaignPageFlags(),_runtime->campaignResearchFlags(),_runtime->campaignLogCount(12)));
    }else if(phase==32||phase==33){
        if(_runtime->campaignRingFound()!=(phase==32)||!_runtime->campaignFixtureGuardianRecovery()||!_runtime->campaignFixtureRingDefenseInventory())return fail("Controlled source-valid unreadied defensive profile rejected");
        stateLine("controlled-owned-unreadied-armor118-before-actual-Ready");
        _runtime->enableSpellValidationObservation();_runtime->key(9);_runtime->key(9);uint stage=0,prompts=0;bool readied=false;
        for(uint i=0;i<12000&&!shouldQuit();++i){
            if(_runtime->atWorldInput()&&_runtime->canSnapshot()){readied=stage>=2&&_runtime->campaignRingDefenseValid();break;}
            if(_runtime->campaignTestInputReady()&&!_runtime->atWorldInput()){
                if(++prompts>64)return fail("Defensive Ready input bound exceeded");
                if(!stage){if(_runtime->address()!=0xa9a1||_runtime->campaignInventoryTab()!=2)_runtime->key(_runtime->address()==0x9966?9:'3');else{int row=_runtime->campaignRingDefenseInventoryRow();if(row<0)return fail("Controlled armor118 absent from original list");if(_runtime->campaignInventoryRow()!=row)_runtime->key(10);else{_runtime->key(13);stage=1;}}}
                else if(stage==1){if(_runtime->campaignRingDefenseInventoryCacheReady()){stage=2;_runtime->key(27);}else _runtime->key('1');}
                else _runtime->key(27);
            }
            if(!advance())return fail("Actual defensive Ready advance failed");
        }
        if(!readied||_runtime->campaignRingFound()!=(phase==32))return fail("Actual armor Ready ownership/derived defense not demonstrated");
        stateLine("actual-armor118-Ready-world-ownership-and-defense45");
    }else if(phase==30||phase==31){
        const bool protectedCase=phase==30,replay=ConfMan.hasKey("nox_ring_replay")&&ConfMan.getBool("nox_ring_replay");
        if(_runtime->campaignRingFound()!=protectedCase||_runtime->campaignLogCount(155)!=(protectedCase?1u:0u)||_runtime->campaignLogCount(154))return fail("Actual Ring/no-Ring predecessor differs");
        if(!replay){
            if((ConfMan.hasKey("nox_ring_defense_fixture")&&ConfMan.getBool("nox_ring_defense_fixture"))||(ConfMan.hasKey("nox_ring_ready_defense")&&ConfMan.getBool("nox_ring_ready_defense"))){if(!_runtime->campaignRingDefenseValid())return fail("Controlled defensive equipment checkpoint invalid");stateLine("controlled-equipment-checkpoint-exact-cold-restored-before-battle");}
            else{stateLine("before-controlled-original-max-recovery");if(!_runtime->campaignFixtureGuardianRecovery())return fail("Ring test source-valid recovery rejected");stateLine("after-controlled-original-max-recovery");}
            if(!setupExit(0,128)||!setupExit(31,3)||!setupExit(32,3)||!_runtime->campaignFixtureRingBattleApproach()||!settle("controlled-original-Lord-Drathe-south-approach"))return fail("Ring encounter authored route/adjacency failed");
            if(!save(2,Common::String::format("Ring effect approach %u",protectedCase?1u:0u)))return fail("Ring legal pre-encounter save failed");
        }
        if(_runtime->campaignMapId()!=32||_runtime->positionX()!=15||_runtime->positionY()!=21||_runtime->campaignMana()!=110||_runtime->campaignPartyHealth()!=125)return fail("Ring legal approach fields differ");
        _runtime->enableRingEffectObservation();_runtime->key('A');_runtime->key(11);
        bool tactical=false,admitted=false,completed=false;uint passes=0;
        for(uint i=0;i<24000&&!shouldQuit();++i){
            if(_runtime->campaignRingDeathSpells()&&!_runtime->campaignPartyHealth()){
                if(protectedCase&&!_runtime->campaignRingSurvivedSpellHP()){trace+=_runtime->campaignRingHealthTrace()+"\n";return fail("Ring case did not survive one completed original death spell");}
                completed=true;break;
            }
            if(_runtime->battleInputReady()){
                const uint pc=_runtime->address();
                if((pc==0xd53f||pc==0xfde9)&&_runtime->inBattle()&&_runtime->battlePlayerTurn()){
                    if(!admitted){if(_runtime->campaignFinalEnemyHealth()!=3210||_runtime->campaignInvulnerability()!=(protectedCase?0u:1u))return fail("Original Lord Drathe full-health/Ring vulnerability differs");admitted=true;}
                    if(_runtime->campaignRingDeathSpells()&&protectedCase&&_runtime->campaignRingProtectedSpells()&&_runtime->campaignPartyHealth()){completed=true;break;}
                    if(++passes>12)return fail("Original enemy did not demonstrate death spell within bounded real passes");_runtime->key(' ');
                }else if(pc==0x1324&&!tactical&&_runtime->battleAdmissionSeen()){_runtime->key('T');tactical=true;}
                else _runtime->key(13);
            }
            if(!(i%128))trace+=_runtime->battleDiagnostic()+"\n";
            if(!advance())return fail("Original Ring effect battle advance failed");
        }
        if(!completed||!admitted||!_runtime->campaignRingDeathSpells()||bool(_runtime->campaignRingProtectedSpells())!=protectedCase||_runtime->campaignLogCount(154)||_runtime->campaignMana()!=110)return fail("Actual Ring death-magic effect/refusal not demonstrated");
        Common::OutSaveFile *raw=getSaveFileManager()->openForSaving(replay?"nox-ring-effect-replay.bin":"nox-ring-effect.bin",false);if(!raw)return fail("Ring diagnostic open failed");bool captured=_runtime->campaignDiagnosticSnapshot(*raw);raw->finalize();captured&=!raw->err();delete raw;if(!captured)return fail("Ring raw diagnostic capture failed");
        frame("nox-ring-effect.png");trace+=_runtime->campaignRingHealthTrace()+"\n";trace+=Common::String::format("ring-effect-result\tprotected=%u original_death_spells=%u protected_spells=%u HP=%u passes=%u\n",protectedCase,_runtime->campaignRingDeathSpells(),_runtime->campaignRingProtectedSpells(),_runtime->campaignPartyHealth(),passes);report(replay?"nox-ring-effect-replay-trace.tsv":"nox-ring-effect-trace.tsv",trace);
        return report(replay?"nox-ring-effect-replay.tsv":"nox-ring-effect.tsv",Common::String::format("PASS\tRing=%u original_death_spells=%u protected_spells=%u HP=%u first_completed_spell_HP=%u no_Dark_Key_award=1 raw_diagnostic_not_player_save=1\n",protectedCase,_runtime->campaignRingDeathSpells(),_runtime->campaignRingProtectedSpells(),_runtime->campaignPartyHealth(),_runtime->campaignRingSurvivedSpellHP()));
    }else if(phase==28||phase==29){
        const bool success=phase==28;
        auto settleRing=[&](const char *label){uint64 polls=_runtime->emptyKeyboardPolls();uint inputs=0;for(uint i=0;i<12000&&!shouldQuit();++i){
            if(_runtime->atWorldInput()&&_runtime->canSnapshot()){stateLine(label);return true;}
            if(_runtime->campaignConversationInputReady()){
                if(++inputs>32)return false;const char *word=success?"KAYL":"WRONG";for(const char *key=word;*key;++key)_runtime->key(*key);_runtime->key(13);polls=_runtime->emptyKeyboardPolls();trace+=Common::String::format("actual-door-contact-password\t%s\n",word);
            }else if(_runtime->atKeyboardPoll()&&!_runtime->atWorldInput()&&_runtime->emptyKeyboardPolls()>polls+32){if(++inputs>32)return false;_runtime->key(13);polls=_runtime->emptyKeyboardPolls();}
            if(!advance())return false;
        }return false;};
        if(_runtime->campaignRingUnlocked()||_runtime->campaignRingFound()||_runtime->campaignLogCount(155))return fail("Ring gate predecessor already completed");
        if(!setupExit(100,2)||!setupExit(99,2)||!setupExit(98,2)||!setupExit(97,2)||!setupExit(0,0)||!setupExit(74,6))return fail("Ring authored route failed");
        topics.push_back(success?"KAYL":"WRONG");const uint64 before=_runtime->enhancements().sequence();
        if(!converse(74,0,topics)||_runtime->campaignRingUnlocked()!=success||!replyContains("NA/DATA.TLK.L01A",success?"Enter.":"word of sealing",before))return fail("Actual Ring password success/refusal differs");
        bool submitted=false;for(const auto &character:_runtime->enhancements().characters())if(character.resource=="NA/DATA.TLK.L01A")for(const auto &reply:character.replies)if(reply.sequence>before){Common::String input=reply.input;input.toUppercase();if(input==topics[0])submitted=true;}
        if(!submitted)return fail("Ring password was not observed as original parsed input");
        trace+=Common::String::format("actual-ring-password\tinput=%s unlock=%u\n",topics[0].c_str(),_runtime->campaignRingUnlocked());
        if(!_runtime->campaignFixtureRingApproach())return fail("Original Ring source validation rejected");
        _runtime->key(8);if(!settle("actual-Left-from-mouth-to-outside-Ring-door")||_runtime->positionX()!=9||_runtime->positionY()!=11)return fail("Actual Ring outside-door walk failed");
        if(!success){_runtime->key('O');_runtime->key(11);if(!settle("actual-refused-Ring-Open-Up"))return fail("Actual Ring door Open failed");}
        _runtime->key(11);if(!settleRing("actual-Ring-door-step-and-contact-dialogue"))return fail("Actual Ring door movement/contact dialogue failed");
        if(success){
            if(_runtime->positionX()!=9||_runtime->positionY()!=10)return fail("Unlocked Ring door did not admit passage");
            _runtime->key(11);if(!settleRing("actual-step-inside-Ring-tomb"))return fail("Actual Ring interior step failed");
            for(uint repeat=0;repeat<2;++repeat){_runtime->key('S');_runtime->key(11);if(!settleRing("actual-Ring-Search-Up-repeat"))return fail("Actual Ring Search failed");}
        }else if(_runtime->positionX()!=9||_runtime->positionY()!=11)return fail("Wrong-word Ring passage not blocked");
        if(_runtime->campaignLogCount(155)!=(success?1u:0u)||_runtime->campaignRingFound()!=success)return fail("Actual Ring reward/refusal differs");
    }else if(phase==26){
        if(_runtime->campaignMapId()!=38||_runtime->campaignLogCount(85)!=1||_runtime->campaignWaterCondition())return fail("Actual Scroll reading predecessor differs");
        if(!setupExit(0,128)||!setupExit(54,7)||!setupExit(78,7)||!setupExit(97,2)||!setupExit(98,2)||!setupExit(99,2)||!setupExit(100,2)||!setupExit(101,2))return fail("Water condition authored depths route failed");
        if(!_runtime->campaignFixtureWaterObjectApproach()||!settle("controlled-before-original-water-condition-object"))return fail("Original object source-valid adjacency rejected");
        _runtime->key('L');_runtime->key(11);if(!settle("actual-look-original-water-object"))return fail("Actual object Look failed");frame("nox-original-water-object-look.png");
        const int operation=ConfMan.hasKey("nox_water_operation")?ConfMan.getInt("nox_water_operation"):3;
        if(operation<0||operation>5)return fail("Invalid original object operation");
        _runtime->key('L');_runtime->key(10);if(!settle("actual-look-nearby-operable-object"))return fail("Nearby object Look failed");frame("nox-water-nearby-object-look.png");
        if(!operation){if(!save(2,"Water object investigation only"))return fail("Investigation checkpoint failed");report("nox-vortex-trace.tsv",trace);return report("nox-vortex-phase-26.tsv","PASS\tprobe=LOOK_ONLY scope=IN PROGRESS no_campaign_acceptance=1\n");}
        if(operation==5){
            // Original EA35/EA3D handles state54 objects under the current cell.
            // Initialization remains south of the object with the outcome clear.
            for(uint repeat=0;repeat<2;++repeat){
                _runtime->key(11);if(!settle("actual-step-onto-phylactery")||!_runtime->campaignWaterCondition())return fail("Actual phylactery contact did not set Water condition");
                _runtime->key(10);if(!settle("actual-return-south-of-phylactery"))return fail("Phylactery return step failed");
            }
        }else{
        _runtime->key(operation==2?'A':operation==4?'S':'O');_runtime->key(operation==3?10:11);uint64 polls=_runtime->emptyKeyboardPolls();bool done=false;
        for(uint i=0;i<12000&&!shouldQuit();++i){if(!advance())return fail("Actual Water object operation advance failed");if(_runtime->atWorldInput()&&_runtime->canSnapshot()){done=true;break;}if(_runtime->atKeyboardPoll()&&_runtime->emptyKeyboardPolls()>polls+16){_runtime->key(13);polls=_runtime->emptyKeyboardPolls();}}
        if(!done)return fail("Actual Water operation did not return to world input");
        }
        if(!_runtime->campaignWaterCondition()||_runtime->campaignLogCount(85)!=1||_runtime->campaignLogCount(153))return fail("Actual phylactery operation did not set authored Water condition");
        trace+="actual-water-object-result\t040C04=1 no_flag_initialization=1 no_Jewel_award=1\n";
    }else if(phase==18||phase==25){
        const bool success=phase==18;
        if(success&&!_runtime->campaignFixtureGuildPrerequisites())return fail("Valid controlled Guild/rescue prerequisites rejected");
        if(_runtime->campaignLogCount(84)||_runtime->campaignMargraveKeyFlag())return fail("Margrave key already present");
        if(!setupExit(71,8)||!setupExit(0,128)||!setupExit(49,5))return fail("Margrave authored route failed");
        const uint64 before=_runtime->enhancements().sequence();if(!converse(49,0,topics))return fail("Actual Margrave opening failed");
        if(_runtime->campaignLogCount(84)!=(success?1u:0u)||_runtime->campaignMargraveKeyFlag()!=success||replyContains("NA/DATA.TLK.L012","Here is my key",before)!=success)return fail("Actual Margrave award/refusal differs");
        if(success){const uint64 repeat=_runtime->enhancements().sequence();if(!converse(49,0,topics)||_runtime->campaignLogCount(84)!=1||replyContains("NA/DATA.TLK.L012","Here is my key",repeat))return fail("Margrave repeat duplicated key");}
    }else if(phase==19||phase==22||phase==23||phase==24){
        const bool success=phase==19;
        if(phase==24&&!_runtime->campaignFixtureGuildPrerequisites())return fail("Controlled rescued/no-key prerequisite rejected");
        if(!_runtime->campaignFixtureTombTime(phase!=22,phase==23))return fail("Controlled source-valid Tomb time/prerequisite rejected");
        if(phase==24&&!setupExit(71,8))return fail("Missing-key Foolery return failed");
        if(!setupExit(0,128)||!setupExit(38,6))return fail("Ghrodwir authored route failed");
        const uint64 before=_runtime->enhancements().sequence();topics.push_back("KEY");if(!converse(38,0,topics))return fail("Actual elder meeting failed");
        if(_runtime->campaignTombUnlocked()!=success||_runtime->campaignTombReconciled()!=success||replyContains("NA/DATA.TLK.L00E","The lock clicks",before)!=success||_runtime->campaignLogCount(85))return fail("Actual Tomb meeting success/refusal differs");
        if(success){const uint64 repeat=_runtime->enhancements().sequence();topics.clear();if(!converse(38,0,topics)||replyContains("NA/DATA.TLK.L00E","The lock clicks",repeat)||!_runtime->campaignTombUnlocked())return fail("Tomb repeat state differs");}
        else{
            if(!_runtime->campaignFixtureTombApproach(false)||!settle("controlled-outside-refused-tomb-door"))return fail("Source-valid refused Tomb approach rejected");
            _runtime->key('O');_runtime->key(11);if(!settle("actual-refused-tomb-open")||_runtime->campaignTombDoor()!=0x123b)return fail("Missing prerequisite unexpectedly opened Tomb");
            _runtime->key(11);if(!settle("actual-refused-tomb-passage")||_runtime->positionX()!=10||_runtime->positionY()!=11||_runtime->campaignTombUnlocked()||_runtime->campaignScrollFound()||_runtime->campaignLogCount(85))return fail("Refused Tomb allowed passage/Scroll outcome");
        }
    }else if(phase==20){
        if(!_runtime->campaignFixtureTombApproach(true)||!settle("controlled-outside-real-tomb-door"))return fail("Source-valid outside Tomb approach rejected");
        _runtime->key('O');_runtime->key(11);if(!settle("actual-tomb-open")||!_runtime->campaignTombDoorOpen())return fail("Actual elder-unlocked Tomb Open failed");
        for(uint i=0;i<2;++i){_runtime->key(11);if(!settle("actual-tomb-door-chamber-walk"))return fail("Actual Tomb passage failed");}
        if(_runtime->positionX()!=10||_runtime->positionY()!=9)return fail("Actual Scroll Search adjacency differs");
        for(uint attempt=0;attempt<2;++attempt){_runtime->key('S');_runtime->key(11);uint64 polls=_runtime->emptyKeyboardPolls();bool done=false;
            for(uint i=0;i<12000&&!shouldQuit();++i){if(!advance())return fail("Scroll Search advance failed");if(_runtime->atWorldInput()&&_runtime->canSnapshot()){done=true;break;}if(_runtime->atKeyboardPoll()&&_runtime->emptyKeyboardPolls()>polls+16){_runtime->key(13);polls=_runtime->emptyKeyboardPolls();}}
            if(!done||_runtime->campaignLogCount(85)!=1||!_runtime->campaignScrollFound())return fail("Actual Scroll Search/repeat differs");
        }
    }else if(phase==38){
        if(!lateFields("before"))return fail("Late NPC retained prerequisites differ");
        const int routes=ConfMan.getInt("nox_late_route_count"),count=ConfMan.getInt("nox_late_topic_count"),map=ConfMan.getInt("nox_late_map"),actor=ConfMan.getInt("nox_late_actor");
        if(routes<0||routes>16||count<0||count>8||map<0||map>255||actor<0||actor>248||(actor&7))return fail("Invalid bounded late NPC manifest");
        if(ConfMan.getBool("nox_late_noon")&&!_runtime->campaignFixtureNoon())return fail("Controlled valid late NPC noon rejected");
        if(ConfMan.hasKey("nox_late_clock_hour")&&!_runtime->campaignFixtureClock(byte(ConfMan.getInt("nox_late_clock_hour")),byte(ConfMan.getInt("nox_late_clock_minute"))))return fail("Controlled valid BCD clock rejected");
        for(int i=0;i<routes;++i){const Common::String key=Common::String::format("nox_late_route_%d_",i);const int id=ConfMan.getInt(key+"map"),type=ConfMan.getInt(key+"type");if(id<0||id>255||type<0||type>255||!setupExit(byte(id),byte(type)))return fail("Late NPC source-authored route rejected");}
        for(int i=0;i<count;++i){const Common::String text=ConfMan.get(Common::String::format("nox_late_topic_%d",i));if(text.empty()||text.size()>40)return fail("Invalid bounded late NPC topic");topics.push_back(text);}
        if(ConfMan.hasKey("nox_late_search")&&ConfMan.getBool("nox_late_search")){
            const int type=ConfMan.getInt("nox_search_type"),x=ConfMan.getInt("nox_search_x"),y=ConfMan.getInt("nox_search_y"),direction=ConfMan.getInt("nox_search_direction"),item=ConfMan.getInt("nox_search_item"),field=ConfMan.getInt("nox_search_field"),mask=ConfMan.getInt("nox_search_mask");
            if(count||type<0||type>8||x<0||x>255||y<0||y>255||item<0||item>255||field<0x400||field>=0x448||mask<1||mask>128||(_runtime->campaignFlagByte(uint16(field))&mask)||!_runtime->campaignFixtureSearchObject(byte(map),byte(type),byte(x),byte(y),byte(direction),byte(item),uint16(field),byte(mask))||!settle("controlled-before-source-Search"))return fail("Source Search object/adjacency precondition rejected");
            for(uint repeat=0;repeat<(ConfMan.getBool("nox_late_repeat")?2u:1u);++repeat){
                if(ConfMan.hasKey("nox_search_clear_ambient")&&ConfMan.getBool("nox_search_clear_ambient")){Common::String controlled;if(!_runtime->campaignFixtureAmbientCheckpoint(controlled)||!settle("controlled-ambient-checkpoint-before-next-Search"))return fail("Source-supported ambient checkpoint rejected");trace+=controlled;}
                const uint32 seed=ConfMan.hasKey("nox_search_seed")?uint32(ConfMan.getInt("nox_search_seed")):0x12345678;if(!_runtime->campaignFixturePageSearchSeed(seed))return fail("Original Search safe floor/PRNG prerequisite rejected");trace+=Common::String::format("CONTROLLED-before-Search-original-PRNG=%u repeat=%u; no item/flag/HP initialization\n",seed,repeat);
                _runtime->key('S');_runtime->key(byte(direction));uint64 polls=_runtime->emptyKeyboardPolls();bool done=false;
                for(uint i=0;i<12000&&!shouldQuit();++i){if(!advance())return fail(_runtime->error());if(_runtime->inBattle())return fail("Original ambient encounter interrupts Search case; no battle bypass or Search acceptance");if(_runtime->atWorldInput()&&_runtime->canSnapshot()){done=true;break;}if(_runtime->atKeyboardPoll()&&_runtime->emptyKeyboardPolls()>polls+16){_runtime->key(13);polls=_runtime->emptyKeyboardPolls();}}
                if(!done||!lateFields("after")||_runtime->campaignPartyHealth()!=uint(ConfMan.getInt("nox_search_health")))return fail("Actual source Search/repeat/result differs");stateLine(repeat?"actual-source-Search-repeat":"actual-source-Search-award");
            }
        }else if(ConfMan.hasKey("nox_late_gate")&&ConfMan.getBool("nox_late_gate")){
            const int type=ConfMan.getInt("nox_gate_type"),column=ConfMan.getInt("nox_gate_x"),row=ConfMan.getInt("nox_gate_y"),direction=ConfMan.getInt("nox_gate_direction"),field=ConfMan.getInt("nox_gate_field"),mask=ConfMan.getInt("nox_gate_mask");const bool success=ConfMan.getBool("nox_gate_open");const int closedTile=ConfMan.hasKey("nox_gate_closed_tile")?ConfMan.getInt("nox_gate_closed_tile"):59;
            if(ConfMan.hasKey("nox_gate_defeated_rusty_guard")&&ConfMan.getBool("nox_gate_defeated_rusty_guard")){if(!_runtime->campaignFixtureRustyGuardDefeated())return fail("Source-supported prior Rusty guard death checkpoint rejected");trace+="CONTROLLED-before-door-fixedBD4-guard-deadFF not_earned_combat no_key_quest_loot_door_outcome_initialized=1\n";}
            if(count||type<2||type>8||column<1||column>=(type==2?255:47)||row<1||row>=(type==2?255:47)||field<0x400||field>=0x448||mask<1||mask>128||bool(_runtime->campaignFlagByte(uint16(field))&mask)!=success||!_runtime->campaignFixtureKeyDoor(byte(map),byte(type),byte(column),byte(row),byte(direction),uint16(field),byte(mask),byte(closedTile))||!settle("controlled-before-original-key-door"))return fail("Original key-door/source-floor precondition rejected");
            if(type==2&&!_runtime->campaignSafeFloor())return fail("Original global key-door outside floor rejected");
            _runtime->key('O');_runtime->key(byte(direction));if(!settle("actual-key-door-Open"))return fail("Original key-door Open failed");const uint door=_runtime->campaignKeyDoorState(byte(column),byte(row),uint16(field),byte(mask),byte(closedTile));
            if(success?((door&255)!=uint(closedTile-1)||(door>>8)<33||(door>>8)>41):door!=(0x1200u|uint(closedTile)))return fail("Original key-door Open/refusal state differs");
            for(uint step=0;step<(success?2u:1u);++step){_runtime->key(byte(direction));if(!settle("actual-key-door-step"))return fail("Original key-door movement failed");}
            if(_runtime->campaignMapId()!=uint(map)||_runtime->positionX()!=ConfMan.getInt("nox_gate_expected_x")||_runtime->positionY()!=ConfMan.getInt("nox_gate_expected_y")||_runtime->campaignPartyHealth()!=uint(ConfMan.getInt("nox_gate_health"))||!lateFields("after"))return fail("Actual key-door passage/refusal/preserved state differs");
            trace+=Common::String::format("actual-key-door-result\topen=%u door=%04X xy=%u,%u\n",success,door,_runtime->positionX(),_runtime->positionY());
        }else if(ConfMan.hasKey("nox_late_world")&&ConfMan.getBool("nox_late_world")){
            if(ConfMan.hasKey("nox_world_cult_object_x")&&(!_runtime->campaignFixtureCultSwitchAdjacent(byte(ConfMan.getInt("nox_world_cult_object_x")),byte(ConfMan.getInt("nox_world_cult_object_y")),byte(ConfMan.getInt("nox_world_cult_direction")))||!settle("CONTROLLED-adjacency-before-actual-Cult-switch-or-door")||!_runtime->campaignSafeFloor()))return fail("Immutable Cult object adjacency/floor rejected");
            const int steps=ConfMan.getInt("nox_world_move_count"),absent=ConfMan.getInt("nox_world_absent_count");if(count||steps<1||steps>128||absent<0||absent>32)return fail("Invalid bounded world operation");
            if(ConfMan.hasKey("nox_world_entry_map")&&(!_runtime->campaignFixtureWorldEntrance(byte(ConfMan.getInt("nox_world_entry_map")),byte(ConfMan.getInt("nox_world_entry_type")),byte(ConfMan.getInt("nox_world_entry_direction")),ConfMan.hasKey("nox_world_entry_x")?byte(ConfMan.getInt("nox_world_entry_x")):0,ConfMan.hasKey("nox_world_entry_y")?byte(ConfMan.getInt("nox_world_entry_y")):0)||!settle("controlled-outside-owned-world-entrance")||!_runtime->campaignSafeFloor()))return fail("Original world entrance/source floor rejected");
            if(ConfMan.hasKey("nox_world_setup_x")&&(!_runtime->campaignFixtureFloor(byte(ConfMan.getInt("nox_world_setup_map")),byte(ConfMan.getInt("nox_world_setup_x")),byte(ConfMan.getInt("nox_world_setup_y")))||!settle("controlled-before-world-operation")||!_runtime->campaignSafeFloor()))return fail("World source-floor setup rejected");
            if(ConfMan.hasKey("nox_world_clear_drathe_ambient")&&ConfMan.getBool("nox_world_clear_drathe_ambient")){Common::String setup;if(!_runtime->campaignFixtureDratheAmbientCheckpoint(setup)||!settle("CONTROLLED-before-access-prior-roaming-kills"))return fail("Authored nonquest roaming checkpoint rejected");trace+=setup;}
            if(ConfMan.hasKey("nox_world_door_before")&&_runtime->campaignWorldObjectState(byte(ConfMan.getInt("nox_world_door_x")),byte(ConfMan.getInt("nox_world_door_y")))!=uint(ConfMan.getInt("nox_world_door_before")))return fail("Actual source door starting record differs");
            for(int step=0;step<steps;++step){
                const Common::String prefix=Common::String::format("nox_world_command_%d_",step);const int length=ConfMan.hasKey(prefix+"size")?ConfMan.getInt(prefix+"size"):1;if(length<1||length>8)return fail("Invalid bounded world command");
                for(uint retry=0;retry<4;++retry){
                    const uint beforeFlees=ambientFlees;
                    for(int index=0;index<length;++index){const int key=ConfMan.hasKey(prefix+"size")?ConfMan.getInt(prefix+Common::String::format("key_%d",index)):ConfMan.getInt(Common::String::format("nox_world_key_%d",step));if(key<1||key>127)return fail("Invalid world input");_runtime->key(byte(key));}
                    if(!settle("actual-world-input"))return fail("Original world operation failed");
                    if(ambientFlees==beforeFlees)break;
                    if(length!=1||retry==3)return fail("Ambient Escape retry bound exceeded");
                    trace+="actual-retry-movement-after-Escape no_position_initialization=1\n";
                }
            }
            if(ConfMan.hasKey("nox_world_door_after_picture")){const uint door=_runtime->campaignWorldObjectState(byte(ConfMan.getInt("nox_world_door_x")),byte(ConfMan.getInt("nox_world_door_y")));if((door&255)!=uint(ConfMan.getInt("nox_world_door_after_picture")))return fail("Actual door picture commit differs");trace+=Common::String::format("actual-world-door-record\t%04X\n",door);}
            for(int i=0;i<absent;++i){const int slot=ConfMan.getInt(Common::String::format("nox_world_absent_%d",i));if(slot<0||slot>248||!_runtime->campaignWorldActorAbsent(byte(slot)))return fail("Original actor retirement absent");}
            if(_runtime->campaignMapId()!=uint(map)||_runtime->positionX()!=ConfMan.getInt("nox_world_expected_x")||_runtime->positionY()!=ConfMan.getInt("nox_world_expected_y")||_runtime->campaignPartyHealth()!=uint(ConfMan.getInt("nox_world_health"))||!lateFields("after"))return fail("Actual world result differs");
            trace+="actual-world-operation-result explicit_manifest_absence_checks_only no_outcome_initialization=1\n";
        }else if(ConfMan.hasKey("nox_late_battle")&&ConfMan.getBool("nox_late_battle")){
            const bool cultists=map==99&&actor==0&&ConfMan.hasKey("nox_late_cultist_encounter")&&ConfMan.getBool("nox_late_cultist_encounter");
            const bool templeGuard=map==99&&actor==8&&ConfMan.hasKey("nox_late_guard_encounter")&&ConfMan.getBool("nox_late_guard_encounter");
            if(ConfMan.getBool("nox_late_repeat")||!((map==41&&actor==56)||(map==50&&actor==120)||templeGuard||cultists||(map==101&&actor==0&&ConfMan.hasKey("nox_late_full_guardian")&&ConfMan.getBool("nox_late_full_guardian"))))return fail("Unsupported bounded quest battle");
            if(ConfMan.hasKey("nox_late_recover")&&ConfMan.getBool("nox_late_recover")){if(!_runtime->campaignFixtureGuardianRecovery())return fail("Controlled between-battle source-valid recovery rejected");trace+="CONTROLLED-before-next-battle-recovery HP125 MP110 not_earned_healing\n";}
            if(map==101&&(!_runtime->campaignFixtureGuardianApproach(1)||!settle("source-guarded-Vazarath-approach")))return fail("Owned Vazarath source approach rejected");
            if(cultists){if(count||!_runtime->campaignCultistsApproach())return fail("Source-qualified actual Cultist approach differs");_runtime->enableBattleValidation();_runtime->key(11);trace+="actual-fixed-Cultist-world-trigger full_original_roster_no_enemy_control=1\n";}
            else if(templeGuard){
                if(count||!_runtime->campaignTempleMonksApproach())return fail("Source-qualified original Monk approach differs");
                _runtime->enableBattleValidation();_runtime->key(8);
                // The original notice precedes inBattle/roster setup. The battle
                // controller acknowledges it, then qualifies the complete source
                // roster at the first player turn before any combat fixture.
                trace+="actual-fixed-Monk-world-trigger full_admission_required_before_controlled_Cast no_world_death_initialization=1\n";
            }else{
                const Common::String resource=ConfMan.get("nox_late_reply_resource"),phrase=ConfMan.get("nox_late_reply_phrase");const uint64 before=_runtime->enhancements().sequence();
                if(!converse(byte(map),byte(actor),topics)||!replyContains(resource.c_str(),phrase.c_str(),before))return fail("Actual quest NPC encounter trigger absent");
            }
            _runtime->enableBattleValidation();_runtime->enableSpellValidationObservation();Common::String battleError;
            if(!finishQuestBattle(byte(map),byte(actor),trace,battleError))return fail(battleError);
            if(templeGuard&&(!_runtime->campaignTempleMonksDead()||_runtime->campaignWorldObjectState(215,100)!=0xfe4a))return fail("Actual Monk victory did not retire world actor/retain mouth wall removal");
            if(cultists&&(!(_runtime->campaignFlagByte(0x438)&2)||_runtime->campaignWorldObjectState(215,100)!=0xfe4a))return fail("Actual Cultist defeat predicate/wall state differs");
            if(!lateFields("after"))return fail("Actual quest battle reward/flags differ");
        }else{
        const Common::String resource=ConfMan.get("nox_late_reply_resource"),phrase=ConfMan.get("nox_late_reply_phrase");if(!resource.hasPrefix("NA/DATA.TLK.")||resource.size()>64||phrase.empty()||phrase.size()>256)return fail("Late NPC original reply requirement missing");
        const uint64 before=_runtime->enhancements().sequence();if(!converse(byte(map),byte(actor),topics)||!replyContains(resource.c_str(),phrase.c_str(),before)||!lateFields("after"))return fail("Actual late NPC reply/result differs");
        if(ConfMan.getBool("nox_late_repeat")){const uint64 repeated=_runtime->enhancements().sequence();if(!converse(byte(map),byte(actor),topics)||!replyContains(resource.c_str(),phrase.c_str(),repeated)||!lateFields("after"))return fail("Actual late NPC repeat state/reply differs");}
        }
        trace+="actual-manifest-result\tcontrols_explicit_in_input_identity no_outcome_initialization=1\n";
    }else if(phase==36||phase==37){
        const bool missingFire=phase==37;
        if(_runtime->campaignLogCount(151)!=1||_runtime->campaignLogCount(152)!=1||_runtime->campaignLogCount(153)!=1||_runtime->campaignPartyHealth()!=125)return fail("Actual Jewel/HP approach predecessor differs");
        if(!setupExit(98,2)||!_runtime->campaignFixtureTempleOutside()||!settle("controlled-outside-Temple168,160"))return fail("Original Temple outside approach rejected");
        if(missingFire&&!_runtime->campaignFixtureOmitFireJewel())return fail("Source-valid controlled omitted Fire prerequisite rejected");
        for(uint step=0;step<3;++step){_runtime->key(11);if(!settle("actual-Up-across-forcefield-floor-fire"))return fail("Actual Temple traversal failed");}
        if(_runtime->campaignMapId()!=98||_runtime->positionX()!=168||_runtime->positionY()!=157||_runtime->campaignPartyHealth()!=(missingFire?90u:125u)||_runtime->campaignLogCount(151)!=(missingFire?0u:1u)||_runtime->campaignLogCount(152)!=1||_runtime->campaignLogCount(153)!=1||_runtime->campaignLogCount(156))return fail("Actual Temple access/Fire resistance result differs");
        trace+=Common::String::format("actual-Temple-approach-completed\tmissingFire=%u HP=%u fire_damage=%u no_cultist_outcome=1\n",missingFire,_runtime->campaignPartyHealth(),125-_runtime->campaignPartyHealth());
    }else if(phase==35){
        if(!setupExit(98,2)||!_runtime->campaignFixtureTempleOutside()||!settle("controlled-outside-Temple168,160"))return fail("Source-valid outside Temple probe rejected");
        _runtime->key('L');_runtime->key(11);if(!settle("actual-Look-Up-Temple-wall"))return fail("Original Temple Look failed");frame("nox-temple-outside-look.png");
        _runtime->key('O');_runtime->key(11);if(!settle("actual-Open-Up-Temple-wall"))return fail("Original Temple Open failed");frame("nox-temple-outside-open.png");
        _runtime->key(11);if(!settle("actual-Up-Temple-wall"))return fail("Original Temple movement probe failed");frame("nox-temple-outside-move.png");
        if(!save(2,"Temple outside investigation only"))return fail("Temple probe checkpoint failed");report("nox-vortex-trace.tsv",trace);
        return report("nox-vortex-phase-35.tsv","PASS\tprobe=Temple-outside scope=IN PROGRESS no_campaign_acceptance=1\n");
    }else if(phase==21||phase==27||phase==34){
        if(phase==34&&(_runtime->campaignMapId()!=97||_runtime->positionX()!=150||_runtime->positionY()!=167||_runtime->campaignLogCount(154)!=1||!_runtime->campaignDarkKeyFlag()))return fail("Retained actual Dark Key gate result differs");
        const bool lore=phase==27||phase==34;const byte book=phase==34?111:lore?loreBook:85;
        if(lore&&!checkOnly&&!_runtime->campaignFixtureLichLore(book))return fail("Source-supported controlled Lich book rejected");
        if(_runtime->campaignLogCount(85)!=1||!_runtime->campaignScrollFound())return fail("Actual unread Scroll predecessor missing");
        _runtime->enableSpellValidationObservation();const bool waterBefore=_runtime->campaignWaterCondition();
        _runtime->key(9);_runtime->key(9);uint stage=0,navigation=0,exits=0;bool returned=false,bookComplete=false;
        for(uint i=0;i<12000&&!shouldQuit();++i){
            if(_runtime->atWorldInput()&&_runtime->canSnapshot()){returned=stage>=2&&bookComplete;break;}
            if((_runtime->campaignTestInputReady()||(stage>=1&&_runtime->campaignBookInputReady()))&&!_runtime->atWorldInput()){
                trace+="actual-scroll-use-input\t"+_runtime->campaignSpellDiagnostic()+Common::String::format(" PC=%04X protection=%u\n",_runtime->address(),_runtime->campaignWaterCondition());
                if(stage>=1&&_runtime->campaignBookInputReady()){
                    if(lore?(book==111?_runtime->campaignBookText().size()<32:!_runtime->campaignLichLorePresent()):!_runtime->campaignScrollCluePresent())return fail("Original selected book text differs");bookComplete=true;
                    if(lore&&!report(checkOnly?"nox-lich-lore-cold.txt":"nox-lich-lore.txt",_runtime->campaignBookText()))return fail("Local source lore evidence failed");
                    frame(lore?(checkOnly?"nox-lich-lore-cold-final.png":"nox-lich-lore-final.png"):checkOnly?"nox-scroll-cold-final-page.png":"nox-scroll-final-page.png");_runtime->key(13);stage=2;
                }
                else if(stage==0){if(++navigation>100)return fail("Scroll inventory navigation exhausted");if(_runtime->address()!=0xa9a1||_runtime->campaignInventoryTab()!=3)_runtime->key(_runtime->address()==0x9966?9:'4');else{const int row=_runtime->campaignOwnedMiscRow(book);if(row<0)_runtime->key(10);else if(_runtime->campaignInventoryRow()!=row)_runtime->key(_runtime->campaignInventoryRow()<row?10:11);else{_runtime->key('U');stage=1;}}}
                else if(stage==1){frame(lore?(checkOnly?"nox-lich-lore-cold-page1.png":"nox-lich-lore-page1.png"):checkOnly?"nox-scroll-cold-page1.png":"nox-original-scroll-use.png");_runtime->key(13);stage=2;}
                else{if(++exits>12)return fail("Selected book Use did not close");_runtime->key((_runtime->address()==0xa9a1||_runtime->address()==0x9966)?27:13);}
            }
            if(!advance())return fail("Actual Scroll Use advance failed");
        }
        if(!returned||!bookComplete||_runtime->campaignWaterCondition()!=waterBefore||_runtime->campaignLogCount(85)!=1||(lore&&_runtime->campaignLogCount(book)!=1))return fail("Actual selected book reading/preserved result differs");
        trace+=Common::String::format("actual-scroll-reading-completed\tcold_re_read=%u original_clue=1 water_condition_unchanged=1\n",checkOnly);
    }else if(phase==15){
        if(_runtime->campaignLogCount(133)||_runtime->campaignFooleryKeyFlag()||_runtime->campaignLogCount(113)||(_runtime->campaignHairnetFlags()&1))return fail("Foolery outcome already present");
        if(!setupExit(78,7)||!setupExit(54,7)||!setupExit(0,128)||!setupExit(71,8))return fail("Foolery authored route failed");
        topics.push_back("HOLY");topics.push_back("GOTH");topics.push_back("KEY");const uint64 before=_runtime->enhancements().sequence();
        if(!converse(71,144,topics)||!replyContains("NA/DATA.TLK.L019","This should work on any lock",before)||_runtime->campaignLogCount(133)!=1||!_runtime->campaignFooleryKeyFlag())return fail("Actual shrubber Foolery key award failed");
        topics.clear();topics.push_back("KEY");if(!converse(71,144,topics)||_runtime->campaignLogCount(133)!=2)return fail("Original Foolery repeat key stack differs");
    }else if(phase==16||phase==17){
        const bool success=phase==16;
        if(_runtime->campaignLogCount(133)!=(success?2u:0u)||_runtime->campaignFooleryKeyFlag()!=success)return fail("Foolery key gate prerequisite differs");
        if(!success&&(!setupExit(78,7)||!setupExit(54,7)||!setupExit(0,128)||!setupExit(71,8)))return fail("Foolery missing-key authored route failed");
        if(!setupExit(70,8)||!_runtime->campaignFixtureHairnetApproach()||!settle("controlled-before-hairnet-key-gate"))return fail("Source-valid Hairnet gate approach rejected");
        _runtime->key('O');_runtime->key(8);if(!settle("actual-hairnet-gate-open"))return fail("Hairnet gate Open failed");
        if(success?!_runtime->campaignHairnetDoorOpen():_runtime->campaignHairnetDoor()!=0x123bu)return fail("Actual Hairnet key gate result differs");
        _runtime->key(8);if(!settle("actual-hairnet-gate-cross")||_runtime->positionX()!=(success?12:13)||_runtime->positionY()!=37)return fail("Actual Hairnet gate crossing/refusal differs");
        if(success){
            for(uint i=0;i<2;++i){_runtime->key(8);if(!settle("actual-hairnet-chamber-walk"))return fail("Actual Hairnet chamber walk failed");}
            if(_runtime->positionX()!=10||_runtime->positionY()!=37)return fail("Actual Hairnet Search adjacency differs");
            for(uint attempt=0;attempt<2;++attempt){
                _runtime->key('S');_runtime->key(8);uint64 polls=_runtime->emptyKeyboardPolls();bool done=false;
                for(uint i=0;i<12000&&!shouldQuit();++i){if(!advance())return fail("Hairnet Search advance failed");if(_runtime->atWorldInput()&&_runtime->canSnapshot()){done=true;break;}if(_runtime->atKeyboardPoll()&&_runtime->emptyKeyboardPolls()>polls+16){_runtime->key(13);polls=_runtime->emptyKeyboardPolls();}}
                if(!done||_runtime->campaignLogCount(113)!=1||!(_runtime->campaignHairnetFlags()&1))return fail("Actual Hairnet Search/repeat failed");
            }
        }else if(_runtime->campaignLogCount(113)||(_runtime->campaignHairnetFlags()&1))return fail("Missing-key refusal awarded Hairnet");
        if(_runtime->campaignLogCount(112)!=1||(_runtime->campaignResearchFlags()&67)!=67||_runtime->campaignLogCount(133)!=(success?2u:0u))return fail("Hairnet operation lost actual predecessor items");
        trace+=Common::String::format("hairnet-result\tkey=%u hairnet=%u flags=%02X door=%04X\n",_runtime->campaignLogCount(133),_runtime->campaignLogCount(113),_runtime->campaignHairnetFlags(),_runtime->campaignHairnetDoor());
    }else if(phase==14){
        if(!_runtime->campaignFixtureEssenceApproach()||!settle("controlled-before-essence-search"))return fail("Source-valid Essence approach rejected");
        for(uint attempt=0;attempt<2;++attempt){
            _runtime->key('S');_runtime->key(21);uint64 polls=_runtime->emptyKeyboardPolls();bool done=false;
            for(uint i=0;i<12000&&!shouldQuit();++i){
                if(!advance())return fail("Essence Search advance failed");
                if(_runtime->atWorldInput()&&_runtime->canSnapshot()){done=true;break;}
                if(_runtime->atKeyboardPoll()&&_runtime->emptyKeyboardPolls()>polls+16){_runtime->key(13);polls=_runtime->emptyKeyboardPolls();}
            }
            if(!done||_runtime->campaignLogCount(112)!=1||(_runtime->campaignResearchFlags()&67)!=67||_runtime->campaignLogCount(149)!=2||_runtime->campaignDevilsKeyFlags()!=3)return fail("Actual Essence Search/repeat result differs");
            stateLine(attempt?"actual-essence-repeat-no-duplicate":"actual-essence-acquired");
        }
        trace+="essence-result\tquantity=1 flag043D40=1 retained_key=2 retained_keyflags=3\n";
    }else if(phase==12||phase==13){
        const bool success=phase==13;
        if(_runtime->campaignLogCount(149)!=(success?2u:0u)||_runtime->campaignDevilsKeyFlags()!=(success?3:0))return fail("Devils Key gate predecessor differs");
        if(!success&&(!setupExit(97,2)||!setupExit(78,7)||!setupExit(54,7)))return fail("Missing-key authored return route failed");
        if(!setupExit(0,128)||!_runtime->campaignFixtureDevilsCave()||!settle("controlled-authored-devils-cave-entry"))return fail("Authored Devils Cave admission failed");
        if(!_runtime->campaignFixtureDevilsApproach()||!settle("controlled-before-devils-gate"))return fail("Source-valid Devils gate approach rejected");
        trace+=Common::String::format("devils-gate-before\txy=%u,%u door=%04X key=%u flags=%u\n",_runtime->positionX(),_runtime->positionY(),_runtime->campaignDevilsDoor(),_runtime->campaignLogCount(149),_runtime->campaignDevilsKeyFlags());
        if(_runtime->campaignMapId()!=97||_runtime->positionX()!=69||_runtime->positionY()!=89||_runtime->campaignDevilsDoor()!=0x1241)return fail("Actual closed Devils gate arrival differs");
        _runtime->key('O');_runtime->key(10);if(!settle("actual-open-devils-gate"))return fail("Actual key gate Open failed to settle");
        if((success?!_runtime->campaignDevilsDoorOpen():_runtime->campaignDevilsDoor()!=0x1241u))return fail("Actual key gate Open result differs");
        _runtime->key(10);if(!settle("actual-step-devils-gate")||_runtime->positionX()!=69||_runtime->positionY()!=(success?90:89))return fail("Actual key gate passage/refusal differs");
        if(success){_runtime->key(10);if(!settle("actual-beyond-devils-gate")||_runtime->positionX()!=69||_runtime->positionY()!=91)return fail("Actual key gate subsequent passage failed");}
        if(_runtime->campaignLogCount(149)!=(success?2u:0u)||_runtime->campaignDevilsKeyFlags()!=(success?3:0))return fail("Key gate consumed or changed key prerequisite");
        trace+=Common::String::format("devils-gate-result\topen_and_cross=%u key=%u flags=%u door=%04X\n",success,_runtime->campaignLogCount(149),_runtime->campaignDevilsKeyFlags(),_runtime->campaignDevilsDoor());
    }else if(phase==11){
        if(_runtime->campaignLogCount(149)||_runtime->campaignDevilsKeyFlags())return fail("Devils Key outcome already initialized");
        if(!setupExit(97,2)||!setupExit(78,7)||!setupExit(54,7)||!setupExit(0,128)||!setupExit(24,8)||!setupExit(25,8)||!setupExit(26,8))return fail("Akora authored route failed");
        const uint64 before=_runtime->enhancements().sequence();topics.push_back("ESSE");topics.push_back("CAVE");topics.push_back("KEY");
        if(!converse(26,56,topics)||!replyContains("NA/DATA.TLK.L00A","here is the key",before)||_runtime->campaignLogCount(149)!=1||_runtime->campaignDevilsKeyFlags()!=3)return fail("Actual Akora Devils Key award failed");
        topics.clear();topics.push_back("KEY");if(!converse(26,56,topics)||_runtime->campaignLogCount(149)!=2||_runtime->campaignDevilsKeyFlags()!=3)return fail("Source repeat key stack semantics differ");
        trace+="devils-key-result\tquantity=2 flags=3 actual_award_and_repeat=1\n";
    }else if(phase==8){
        if(_runtime->campaignMapId()!=68||_runtime->positionX()!=23||_runtime->positionY()!=20||(_runtime->campaignTransportFlags()&0x38)!=0x38||_runtime->campaignLogCount(157)!=1)return fail("Real Praevoc return predecessor missing");
        // Map reload discards Max's transient object; retained Ankh and flags
        // must permit a fresh real NO transaction, without resetting outcomes.
        topics.push_back("NO");const uint64 before=_runtime->enhancements().sequence();
        if(!converse(68,0,topics)||!replyContains("NA/DATA.TLK.L018","sparkling blue portal",before)||_runtime->campaignPortalCount()!=1||_runtime->campaignLogCount(157)!=1)return fail("Actual Max portal recreation failed");
        const byte walk[]={8,8,10};for(uint i=0;i<3;++i){_runtime->key(walk[i]);if(!settle("actual-recreated-portal-walk"))return fail("Recreated portal walk failed");}
        if(_runtime->campaignMapId()!=8||_runtime->positionX()!=81||_runtime->positionY()!=168)return fail("Actual recreated Max portal entry failed");
    }else if(phase==7){
        if(_runtime->campaignMapId()!=8||_runtime->positionX()!=81||_runtime->positionY()!=168||(_runtime->campaignTransportFlags()&0x38)!=0x38||_runtime->campaignLogCount(157)!=1)return fail("Real plane arrival predecessor missing");
        _runtime->key(11);if(!settle("actual-leave-return-portal")||_runtime->campaignMapId()!=8||_runtime->positionX()!=81||_runtime->positionY()!=167)return fail("Actual return portal approach failed");
        _runtime->key(10);if(!settle("actual-return-to-praevoc")||_runtime->campaignMapId()!=68||_runtime->positionX()!=23||_runtime->positionY()!=20||_runtime->campaignLogCount(157)!=1)return fail("Actual plane return failed");
    }else if(phase==4){
        if((_runtime->campaignTransportFlags()&0x38)||_runtime->campaignLogCount(157))return fail("Ankh already initialized");
        const bool lateTransport=ConfMan.hasKey("nox_transport_late_predecessor");
        if(lateTransport){
            if((_runtime->campaignFlagByte(0x43b)&7)!=7||(_runtime->campaignTransportFlags()&0x87)!=0x87||_runtime->campaignLogCount(155)!=1||_runtime->campaignLogCount(156)!=1)return fail("Actual later guidance/Ring/journal predecessor missing");
            if(!setupExit(58,7)||!setupExit(54,7)||!setupExit(0,128)||!setupExit(65,8))return fail("Late Shrine authored route failed");
        }else if(!setupExit(97,2)||!setupExit(78,7)||!setupExit(54,7)||!setupExit(0,128)||!setupExit(65,8))return fail("Shrine authored route failed");
        if(!_runtime->campaignFixtureShrineApproach()||!settle("controlled-outside-shrine-circle"))return fail("Shrine approach rejected");
        _runtime->key(11);if(!settle("actual-shrine-circle-entry"))return fail("Real Shrine activation did not return");
        const uint64 before=_runtime->enhancements().sequence();topics.push_back("TRAVEL");topics.push_back("ANKH");
        if(!converse(65,0,topics)||!replyContains("NA/DATA.TLK.L01D","hands you an Ankh",before)||(_runtime->campaignTransportFlags()&0x18)!=0x18||_runtime->campaignLogCount(157)!=1)return fail("Actual Lord British Ankh award failed");
        topics.clear();topics.push_back("ANKH");const uint64 repeat=_runtime->enhancements().sequence();
        if(!converse(65,0,topics)||!replyContains("NA/DATA.TLK.L01D","already got one",repeat)||_runtime->campaignLogCount(157)!=1)return fail("Ankh repeat refusal failed");
    }else if(phase==5||phase==6){
        if(phase==5&&((_runtime->campaignTransportFlags()&0x18)!=0x18||_runtime->campaignLogCount(157)!=1))return fail("Actual Ankh predecessor missing");
        if(phase==6&&(_runtime->campaignTransportFlags()||_runtime->campaignLogCount(157)))return fail("Refusal Ankh precondition differs");
        if(phase==6&&(!setupExit(97,2)||!setupExit(78,7)||!setupExit(54,7)))return fail("Refusal authored maze return failed");
        if(!setupExit(0,128)||!setupExit(67,8)||!setupExit(68,8))return fail("Praevoc authored route failed");
        const uint64 before=_runtime->enhancements().sequence();topics.push_back("PORTAL");if(phase==5)topics.push_back("NO");
        if(!converse(68,0,topics))return fail("Max conversation failed");
        if(phase==6){if(!replyContains("NA/DATA.TLK.L018","we're closed",before)||_runtime->campaignTransportFlags()||_runtime->campaignLogCount(157)||_runtime->campaignPortalCount())return fail("Missing Ankh portal refusal failed");}
        else{
            if(!replyContains("NA/DATA.TLK.L018","sparkling blue portal",before)||(_runtime->campaignTransportFlags()&0x38)!=0x38||_runtime->campaignPortalCount()!=1||_runtime->campaignLogCount(157)!=1)return fail("Actual Max portal creation failed");
            topics.clear();topics.push_back("NO");if(!converse(68,0,topics)||_runtime->campaignPortalCount()!=1||_runtime->campaignLogCount(157)!=1)return fail("Portal repeat duplicated object or Ankh");
            if(!save(2,"Max portal created"))return fail("Actual created portal checkpoint save failed");
            // Retain Max's live portal/cache: walk from actual conversation
            // adjacency25,19 rather than redrawing a controlled approach.
            if(_runtime->positionX()!=25||_runtime->positionY()!=19)return fail("Unexpected Max conversation position");
            const byte portalWalk[]={8,8,10};
            for(uint i=0;i<3;++i){_runtime->key(portalWalk[i]);if(!settle("actual-portal-walk"))return fail("Portal walk did not settle");}
            if(_runtime->campaignMapId()!=8||_runtime->positionX()!=81||_runtime->positionY()!=168)return fail("Actual portal did not enter plane");
        }
	}else if(phase==1){
        if(earnedPages){
            if(!cold||_runtime->campaignPageFlags()!=253||_runtime->campaignLogCount(97)!=7||_runtime->campaignLogCount(12)||!_runtime->campaignFixtureNoon())return fail("Invalid actual seven-page prefix");
            stateLine("seven-actual-Search-awards-retained-no-page-initialization");
            if(!setupExit(58,7)||!setupExit(54,7)||!setupExit(0,128)||!setupExit(35,6))return fail("Retained pages Axea authored route failed");
        }else{
		    if(cold||_runtime->campaignPageFlags()||_runtime->campaignLogCount(97)||_runtime->campaignLogCount(12)||!_runtime->campaignFixtureNoon())return fail("Invalid empty campaign prefix");
		    stateLine("before-controlled-seven-pages");if(!_runtime->campaignFixtureSevenPages())return fail("Source-supported seven-page prerequisite rejected");stateLine("controlled-seven-pages-not-earned");
		    if(!save(2,"Vortex controlled seven pages"))return fail("Controlled seven-page checkpoint failed");
		    if(!setupExit(0,0)||!setupExit(35,6))return fail("Axea map setup failed");
        }
		const uint64 before=_runtime->enhancements().sequence();topics.push_back("VORTEX");
		if(!converse(35,40,topics)||!replyContains("NA/DATA.TLK.L00D","old parchment",before)||_runtime->campaignPageFlags()!=255||_runtime->campaignLogCount(97)!=8)return fail("Axea eighth-page transaction failed");
		const uint64 repeat=_runtime->enhancements().sequence();if(!converse(35,40,topics)||!replyContains("NA/DATA.TLK.L00D","only one",repeat)||_runtime->campaignLogCount(97)!=8)return fail("Axea repeat refusal failed");
	}else if(phase==2){
		if((_runtime->campaignResearchFlags()&3)||_runtime->campaignLogCount(12))return fail("Research outcome already present");
		if(!setupExit(0,128)||!setupExit(54,7))return fail("Valess map setup failed");
		byte direction=0; // Probe only: the actual setup is performed by converse.
		// Source map78 schedule puts Valess in the library at noon. Actor slots are local to each map.
		if(!setupExit(78,7))return fail("Valess library setup failed");
		(void)direction;const uint64 before=_runtime->enhancements().sequence();topics.push_back("CREATE");
		if(!converse(78,88,topics)||!replyContains("NA/DATA.TLK.L015","all the pages together",before)||!(_runtime->campaignResearchFlags()&1)||_runtime->campaignLogCount(97)!=8||_runtime->campaignLogCount(12))return fail("Valess actual research failed");
		const uint64 repeat=_runtime->enhancements().sequence();topics.clear();if(!converse(78,88,topics)||!replyContains("NA/DATA.TLK.L015","cannot help you further",repeat)||(_runtime->campaignResearchFlags()&3)!=1)return fail("Valess repeated research branch failed");
	}else{
		if(_runtime->campaignLogCount(12)||(_runtime->campaignResearchFlags()&3)!=1)return fail("Binding outcome already present or research missing");
		if(!setupExit(97,2)||!setupExit(104,2))return fail("Nystul authored maze setup failed");
		const uint64 before=_runtime->enhancements().sequence();
		if(!converse(104,16,topics)||!replyContains("NA/DATA.TLK.L068","bind together",before)||!(_runtime->campaignResearchFlags()&2)||_runtime->campaignLogCount(12)!=1)return fail("Nystul actual binding failed");
		const uint64 repeat=_runtime->enhancements().sequence();topics.push_back("BIND");if(!converse(104,16,topics)||_runtime->campaignLogCount(12)!=1||replyContains("NA/DATA.TLK.L068","bind together",repeat))return fail("Nystul repeated binding awarded twice");
	}
	trace+=Common::String::format("tomb-result\tkey=%u keyflag=%u rescued=%u unlocked=%u reconciled=%u scroll=%u found=%u protection=%u\n",_runtime->campaignLogCount(84),_runtime->campaignMargraveKeyFlag(),_runtime->campaignChildrenRescued(),_runtime->campaignTombUnlocked(),_runtime->campaignTombReconciled(),_runtime->campaignLogCount(85),_runtime->campaignScrollFound(),_runtime->campaignWaterCondition());
	trace+=Common::String::format("transport-result\tflags=%02X ankh=%u portals=%u\n",_runtime->campaignTransportFlags(),_runtime->campaignLogCount(157),_runtime->campaignPortalCount());
	stateLine("phase-completed");
	if(!save(1,Common::String::format("Vortex phase %d",phase))||!frame(Common::String::format("nox-vortex-phase-%d.png",phase)))return fail("Phase durable save/frame failed");
	report(checkOnly&&(phase==21||phase==27||phase==34)?"nox-scroll-cold-trace.tsv":"nox-vortex-trace.tsv",trace);
	return report(Common::String::format(checkOnly?"nox-vortex-cold-%d.tsv":"nox-vortex-phase-%d.tsv",phase),Common::String::format("PASS\tphase=%d\tpages=%u\tpageflags=%u\tresearch=%u\tbook=%u\tprovenance=controlled-map-setups-linked-gameplay\n",phase,_runtime->campaignLogCount(97),_runtime->campaignPageFlags(),_runtime->campaignResearchFlags(),_runtime->campaignLogCount(12)));
}
}
#endif
