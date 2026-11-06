/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_VENDOR_VALIDATION_H
#define NOX_NATIVE_VENDOR_VALIDATION_H
#include "nox/native_vendor_transactions.h"
#include "nox/native_merchant.h"
#include "nox/native_inventory_actions.h"
#include "nox/native_world_navigation.h"
#include "nox/native_world_projectile.h"
#include "nox/native_world_cannon.h"
#include "nox/native_battle_physical_validation.h"
namespace Nox {
inline bool validateNativeVendor(HdvArchive &archive,Common::SeekableReadStream &in,Common::String &report,Common::String &error) {
    if (in.readUint32BE()!=MKTAG('N','V','F','R') || in.readUint16LE()!=1) return false;
    const uint32 count=in.readUint32LE();if (!count || count>100000) return false;
    Native::VendorCatalog catalog;if (!catalog.load(archive)) return false;uint bought=0,poor=0,full=0,clipped=0;
    for (uint i=0;i<count;++i) {
        const byte map=in.readByte(),slot=in.readByte(),members=in.readByte(),requested=in.readByte();
        const uint16 food=in.readUint16LE(),gold=in.readUint16LE(),price=in.readUint16LE(),afterFood=in.readUint16LE(),afterGold=in.readUint16LE();
        const byte quantity=in.readByte(),wasClipped=in.readByte(),result=in.readByte();Native::FoodPurchase next;Native::VendorOffer offer;
        if (!catalog.select(1,map,slot,offer) || offer.price!=price || !Native::VendorTransactions::food(members,food,gold,offer.price,requested,next) ||
            next.food!=afterFood || next.gold!=afterGold || next.quantity!=quantity || next.clipped!=(wasClipped!=0) || next.result!=result) {
            error=Common::String::format("Complete food vendor transaction differs case%u",i);return false;
        }
        bought+=next.result==Native::kVendorBought;poor+=next.result==Native::kVendorPoor;full+=next.result==Native::kVendorFull;clipped+=next.clipped;
    }
    if (in.readUint32BE()!=MKTAG('N','V','C','A')) return false;const uint offers=in.readUint16LE();if (offers!=27) return false;
    for (uint i=0;i<offers;++i) {
        const byte role=in.readByte(),map=in.readByte(),slot=in.readByte();const uint16 price=in.readUint16LE(),offset=in.readUint16LE(),text=in.readUint16LE(),size=in.readUint16LE();
        Common::Array<byte> raw;raw.resize(size);Native::VendorOffer offer;
        if (size>255 || in.read(raw.data(),size)!=size || !catalog.select(role,map,slot,offer) || offer.price!=price || offer.offset!=offset || offer.textOffset!=text ||
            offer.raw.size()!=size || memcmp(offer.raw.data(),raw.data(),size)) {error=Common::String::format("Authored vendor offer differs case%u",i);return false;}
    }
    uint transportCases=0;
    if (in.pos()<in.size()) {
        if (in.readUint32BE()!=MKTAG('N','V','T','R') || in.readUint16LE()!=1) return false;
        transportCases=in.readUint32LE();if (!transportCases || transportCases>10000) return false;
        for (uint i=0;i<transportCases;++i) {
            const byte role=in.readByte(),map=in.readByte(),slot=in.readByte(),choice=in.readByte();byte ownership=in.readByte();
            uint16 gold=in.readUint16LE();Native::WorldTransportState state;state.allocator=in.readByte();byte objects[256],expectedObjects[256];
            if (in.read(objects,256)!=256) return false;
            const uint16 afterGold=in.readUint16LE();const byte afterOwnership=in.readByte(),allocator=in.readByte(),owned=in.readByte(),picture=in.readByte();
            if (in.read(expectedObjects,256)!=256) return false;
            Native::VendorOffer offer;Native::VendorResult result;
            if (!catalog.select(role,map,slot,offer)) return false;
            const bool ok=role==3?Native::VendorTransactions::boat(offer,choice,gold,ownership,result):
                role==6 && Native::VendorTransactions::mount(offer,choice!=0,gold,25,25,objects,256,state,result);
            if (!ok || gold!=afterGold || ownership!=afterOwnership || state.allocator!=allocator || state.object!=owned || state.appearance!=picture || memcmp(objects,expectedObjects,256)) {
                error=Common::String::format("Complete boat/mount vendor transaction differs case%u",i);return false;
            }
        }
    }
    uint carrierCases=0;
    if (in.pos()<in.size()) {
        if (in.readUint32BE()!=MKTAG('N','C','T','R') || in.readUint16LE()!=1) return false;
        carrierCases=in.readUint32LE();if (!carrierCases || carrierCases>10000) return false;
        for (uint i=0;i<carrierCases;++i) {
            const byte picture=in.readByte(),owned=in.readByte(),cursor=in.readByte();byte oldObjects[256],newObjects[256],afterOld[256],cargoBytes[4],afterNew[256];
            if (in.read(oldObjects,256)!=256 || in.read(newObjects,256)!=256 || in.read(afterOld,256)!=256 || in.read(cargoBytes,4)!=4 || in.read(afterNew,256)!=256) return false;
            const byte afterOwned=in.readByte(),afterCursor=in.readByte();Native::WorldTransportState state;state.object=owned;state.used=owned!=255;state.appearance=picture?picture:246;state.allocator=cursor;Native::TransportCargo cargo;
            if (!Native::WorldTransportControl::detachForTravel(oldObjects,256,state,cargo) || memcmp(oldObjects,afterOld,256) || cargo.active!=(owned!=255) || memcmp(cargo.record,cargoBytes,4) ||
                !Native::WorldTransportControl::restoreAfterTravel(10,19,newObjects,256,state,cargo) || memcmp(newObjects,afterNew,256) || state.object!=afterOwned || state.allocator!=afterCursor) {
                error=Common::String::format("Vehicle detach/map-load/restore differs case%u",i);return false;
            }
        }
    }
    uint merchantCases=0;
    if (in.pos()<in.size()) {
        if (in.readUint32BE()!=MKTAG('N','M','T','R') || in.readUint16LE()!=1) return false;
        merchantCases=in.readUint32LE();if (!merchantCases || merchantCases>20000) return false;
        Native::MerchantCatalog merchants;Native::ItemCatalog items;
        if (!merchants.load(archive) || !items.load(archive)) return false;
        for (uint i=0;i<merchantCases;++i) {
            const byte map=in.readByte(),actor=in.readByte(),kind=in.readByte(),id=in.readByte(),mode=in.readByte(),requested=in.readByte(),available=in.readByte();
            const bool book=in.readByte()!=0,ready=in.readByte()!=0;const byte choice=in.readByte();
            uint16 gold=in.readUint16LE();const uint16 afterGold=in.readUint16LE(),message=in.readUint16LE();
            byte before[1536],fields[256],after[1536],afterFields[256],actual[1536];
            if (in.read(before,1536)!=1536 || in.read(fields,256)!=256 || in.read(after,1536)!=1536 || in.read(afterFields,256)!=256) return false;
            Native::Inventory inventory;Native::MerchantOffer offer;Native::MerchantTransaction transaction;
            if (!inventory.importRecords(before,1536) || !merchants.select(map,actor,offer)) return false;
            const bool ok=mode?Native::MerchantTransactions::sell(merchants,items,offer,0,available,book,ready,choice,inventory,fields,256,gold,transaction):
                Native::MerchantTransactions::buy(merchants,items,offer,kind,id,requested,inventory,fields,256,gold,transaction);
            if (!ok || !inventory.exportRecords(actual,1536) || gold!=afterGold || memcmp(actual,after,1536) || memcmp(fields,afterFields,256)) {
                error=Common::String::format("Merchant economic transaction differs case%u mode%u kind%u id%u gold%u/%u message%04x result%u",i,mode,kind,id,gold,afterGold,message,transaction.result);return false;
            }
        }
    }
    uint dropCases=0,documentCases=0,unresolvedDrops=0;
    if (in.pos()<in.size()) {
        if (in.readUint32BE()!=MKTAG('N','I','D','R') || in.readUint16LE()!=1) return false;
        dropCases=in.readUint32LE();if (!dropCases || dropCases>20000) return false;
        Native::ItemCatalog items;Native::EquipmentOwnership ownership;
        if (!items.load(archive) || !ownership.load(archive)) return false;
        for (uint i=0;i<dropCases;++i) {
            const bool confirmed=in.readByte()!=0;byte before[1536],fields[256],bookBytes[20],after[1536],afterFields[256],actual[1536];
            if (in.read(before,1536)!=1536 || in.read(fields,256)!=256 || in.read(bookBytes,20)!=20 || in.read(after,1536)!=1536 || in.read(afterFields,256)!=256) return false;
            Native::Inventory inventory;Native::SpellBook book;
            if (!inventory.importRecords(before,1536) || !book.importState(bookBytes,20)) return false;
            byte priorFields[256];memcpy(priorFields,fields,256);
            const Native::InventoryDropResult result=Native::InventoryActions::dropRecords(items,ownership,book,inventory,fields,256,0,confirmed);
            if (!inventory.exportRecords(actual,1536)) return false;
            if (result==Native::kDropUnsupported) {
                ItemDefinition definition;
                if (!confirmed || !items.definition(before[0]&15,before[1],definition) || (before[0]&15)!=2 || !(definition.bytes[8]&32) ||
                    memcmp(actual,before,1536) || memcmp(fields,priorFields,256)) return false;
                ++unresolvedDrops;continue;
            }
            if (memcmp(actual,after,1536) || memcmp(fields,afterFields,256)) {
                error=Common::String::format("Complete source inventory Drop differs case%u",i);return false;
            }
        }
        if (in.readUint32BE()!=MKTAG('N','I','D','C')) return false;documentCases=in.readUint16LE();if (documentCases!=47) return false;
        for (uint i=0;i<documentCases;++i) {
            const byte id=in.readByte();const uint length=in.readUint16LE();if (!length || length>=768) return false;
            Common::String expected;for (uint j=0;j<length;++j) expected+=char(in.readByte());
            ItemDefinition definition;Common::String actual;
            if (!items.definition(2,id,definition) || !Native::InventoryActions::document(archive,definition,actual) || actual!=expected) {
                error=Common::String::format("Authored usable document differs case%u id%u",i,id);return false;
            }
        }
    }
    uint navigationCases=0;
    if (in.pos()<in.size()) {
        if (in.readUint32BE()!=MKTAG('N','N','V','R') || in.readUint16LE()!=1) return false;
        navigationCases=in.readUint32LE();if (!navigationCases || navigationCases>10000) return false;
        for (uint i=0;i<navigationCases;++i) {
            const byte operation=in.readByte();Native::WorldNavigation navigation;byte before[49],expected[49];
            if (operation<=1) {
                const byte type=in.readByte(),map=in.readByte(),x=in.readByte(),y=in.readByte(),argument=in.readByte();
                const byte result=operation?in.readByte():0;
                if (in.read(before,49)!=49 || in.read(expected,49)!=49) return false;
                Common::MemoryReadStream input(before,49);if (!navigation.load(input)) return false;
                if (operation) {
                    if (navigation.bind(argument,Native::NavigationPoint(type,map,x,y))!=result) return false;
                } else if (!navigation.enter(Native::NavigationPoint(type,map,x,y),argument)) return false;
                Common::MemoryWriteStreamDynamic actual(DisposeAfterUse::YES);
                if (!navigation.save(actual) || actual.size()!=49 || memcmp(actual.getData(),expected,49)) {
                    error=Common::String::format("Source navigation transaction differs case%u operation%u",i,operation);return false;
                }
                Native::WorldNavigation restored;Common::MemoryReadStream saved(actual.getData(),49);
                Common::MemoryWriteStreamDynamic roundtrip(DisposeAfterUse::YES);
                if (!restored.load(saved) || !restored.save(roundtrip) || roundtrip.size()!=49 || memcmp(actual.getData(),roundtrip.getData(),49)) return false;
            } else if (operation==2) {
                const byte orb=in.readByte(),result=in.readByte();byte point[4];
                if (in.read(point,4)!=4 || in.read(before,49)!=49) return false;
                Common::MemoryReadStream input(before,49);if (!navigation.load(input)) return false;
                Native::NavigationPoint actual;
                if (navigation.choose(orb,actual)!=result || (!result && (actual.type!=point[0] || actual.map!=point[1] || actual.x!=point[2] || actual.y!=point[3]))) {
                    error=Common::String::format("Source Sally Forth orb selection differs case%u",i);return false;
                }
            } else return false;
        }
    }
    uint projectileCases=0;
    if (in.pos()<in.size()) {
        if (in.readUint32BE()!=MKTAG('N','W','P','R') || in.readUint16LE()!=1) return false;
        projectileCases=in.readUint32LE();if (!projectileCases || projectileCases>10000) return false;
        for (uint i=0;i<projectileCases;++i) {
            const byte x=in.readByte(),y=in.readByte(),direction=in.readByte(),mode=in.readByte();
            const uint32 seed=in.readUint32LE();const byte tail=in.readByte(),last=in.readByte();const uint16 attempts=in.readUint16LE();
            const byte hit=in.readByte(),endpoint=in.readByte(),impact=in.readByte(),afterMode=in.readByte();
            const uint32 afterSeed=in.readUint32LE();const byte afterTail=in.readByte(),afterLast=in.readByte();const uint16 afterAttempts=in.readUint16LE();
            Native::GameplayRandom random;Native::WorldProjectileOutcome outcome;
            if (mode>1 || hit>1 || afterMode!=1 || !random.importState(seed,tail,last,attempts) ||
                !Native::WorldProjectile::hostile(x,y,direction,mode!=0,random,outcome) || outcome.hit!=(hit!=0) ||
                outcome.endpoint!=endpoint || outcome.impact!=impact || random.seed()!=afterSeed || random.tail()!=afterTail || random.last()!=afterLast || random.generationAttempts()!=afterAttempts) {
                error=Common::String::format("Complete source hostile world projectile differs case%u at%u,%u direction%u",i,x,y,direction);return false;
            }
        }
    }
    uint worldCannonCases=0;
    if (in.pos()<in.size()) {
        const uint32 tag=in.readUint32BE();in.seek(in.pos()-4);
        if (tag==MKTAG('N','W','C','F')) {
            in.readUint32BE();if (in.readUint16LE()!=1) return false;worldCannonCases=in.readUint32LE();if (!worldCannonCases || worldCannonCases>10000) return false;
            for (uint i=0;i<worldCannonCases;++i) {
                const byte appearance=in.readByte(),direction=in.readByte(),protection=in.readByte(),priorMode=in.readByte(),map=in.readByte();byte footer[72];
                if (in.read(footer,72)!=72) return false;
                const uint32 seed=in.readUint32LE();const byte tail=in.readByte(),last=in.readByte();const uint16 attempts=in.readUint16LE();
                byte before[886],expected[886];if (in.read(before,886)!=886) return false;
                const byte fired=in.readByte(),classification=in.readByte(),endpoint=in.readByte(),impact=in.readByte(),mode=in.readByte(),alarm=in.readByte(),message=in.readByte();
                const uint32 afterSeed=in.readUint32LE();const byte afterTail=in.readByte(),afterLast=in.readByte();const uint16 afterAttempts=in.readUint16LE();
                if (in.read(expected,886)!=886) return false;
                Native::GameplayRandom random;Native::WorldProjectileOutcome outcome;bool retained=priorMode!=0,alert=false;
                if (!random.importState(seed,tail,last,attempts)) return false;
                const Native::WorldCannonResult result=Native::WorldCannon::fire(appearance,direction,map,protection!=0,retained,alert,footer,before,before+256,before+512,before+699,random,outcome);
                if (result==Native::kWorldCannonUnsupported || (result==Native::kWorldCannonFired)!=(fired!=0) || retained!=(mode!=0) || alert!=(alarm!=0) ||
                    (result==Native::kWorldCannonRefused && message!=18) || (fired && (outcome.hit!=(classification==2) || outcome.endpoint!=endpoint || outcome.impact!=impact)) ||
                    memcmp(before,expected,886) || random.seed()!=afterSeed || random.tail()!=afterTail || random.last()!=afterLast || random.generationAttempts()!=afterAttempts) {
                    error=Common::String::format("Complete source world cannon transaction differs case%u ship%u direction%u result%u hit%u/%u endpoint%u/%u RNG%08x/%08x",i,appearance,direction,result,outcome.hit,classification==2,outcome.endpoint,endpoint,random.seed(),afterSeed);return false;
                }
            }
        }
    }
    if (in.pos()<in.size()) {
        Common::String inherited;
        if (!validateNativeBattlePhysical(archive,in,inherited,false,true)) {error=inherited;return false;}
        report+="native-inherited-projectile-mode\t"+inherited;
    }
    if (in.err() || in.pos()!=in.size()) return false;
    report+=Common::String::format("native-world-cannon\tcases=%u\tcomplete_F_records_RNG_removal_alarm=1\tordinary_UI=0\tcpu_execution=0\n",worldCannonCases);
    report+=Common::String::format("native-world-projectile\tcases=%u\tcomplete_hostile_ray_RNG=1\tparty_damage_integration=0\tcpu_execution=0\n",projectileCases);
    report+=Common::String::format("native-world-navigation\tcases=%u\tsource_origin_orb_selection=1\tcodec_roundtrip=1\tordinary_UI=0\tcpu_execution=0\n",navigationCases);
    report+=Common::String::format("native-inventory-drop\tcases=%u\tmatched=%u\tunresolved_counter=%u\tstatus=%s\tdocuments=%u\tordinary_UI=0\tcpu_execution=0\n",dropCases,dropCases-unresolvedDrops,unresolvedDrops,unresolvedDrops?"IN PROGRESS":"VERIFIED",documentCases);
    report+=Common::String::format("native-merchant\tcases=%u\tordinary_UI=0\tcpu_execution=0\n",merchantCases);
    report+=Common::String::format("native-transport-carrier\tcases=%u\tmap_admission=0\tcpu_execution=0\n",carrierCases);
    report+=Common::String::format("native-vendor-transport\tcases=%u\tordinary_UI=0\tcpu_execution=0\n",transportCases);
    report+=Common::String::format("native-vendor\tcases=%u\tbought=%u\tpoor=%u\tfull=%u\tclipped=%u\tauthored_offers=%u\tordinary_UI=0\tcpu_execution=0\n",count,bought,poor,full,clipped,offers);return true;
}
}
#endif
