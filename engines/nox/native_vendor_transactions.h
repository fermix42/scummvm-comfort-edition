/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_VENDOR_TRANSACTIONS_H
#define NOX_NATIVE_VENDOR_TRANSACTIONS_H
#include "nox/resources.h"
#include "nox/native_world_transport.h"
namespace Nox { namespace Native {
enum VendorResult { kVendorBought,kVendorPoor,kVendorFull,kVendorCancelled,kVendorAlreadyOwned,kVendorInvalid };
struct FoodPurchase {
    uint16 food,gold;byte quantity;bool clipped;VendorResult result;
    FoodPurchase() : food(0),gold(0),quantity(0),clipped(false),result(kVendorInvalid) {}
};
struct VendorOffer {
    byte role,map,slot;uint16 offset,textOffset,price;Common::Array<byte> raw;
    VendorOffer() : role(255),map(0),slot(0),offset(0),textOffset(0),price(0) {}
};
// Complete source food economic transaction. The service continuation owns
// numeric input, authored text and return; this owner commits no UI/clock/RNG.
class VendorTransactions {
public:
    static bool boat(const VendorOffer &offer,byte choice,uint16 &gold,byte &ownership,VendorResult &result) {
        if (offer.role!=3 || offer.raw.size()<14) return false;
        if (ownership) {result=kVendorAlreadyOwned;return true;}
        if (!choice) {result=kVendorCancelled;return true;}
        if (choice>3 || uint(0xb0+choice)>=offer.raw[3]) return false;
        const uint price=READ_LE_UINT16(offer.raw.data()+8+(choice-1)*2);
        if (gold<price) {ownership=0;result=kVendorPoor;return true;}
        // TALK boat98EF's authored masks: frigate purchase grants255,not128.
        const byte masks[]={1,64,255};ownership=masks[choice-1];gold=uint16(gold-price);result=kVendorBought;return true;
    }
    static bool mount(const VendorOffer &offer,bool accepted,uint16 &gold,byte x,byte y,byte *objects,uint count,WorldTransportState &state,VendorResult &result) {
        if (offer.role!=6) return false;
        if (!accepted) {result=kVendorCancelled;return true;}
        if (gold<offer.price) {result=kVendorPoor;return true;}
        if (!WorldTransportControl::createMount(x,y,offer.map==71?238:127,objects,count,state)) return false;
        gold=uint16(gold-offer.price);result=kVendorBought;return true;
    }
    static bool food(byte partyCount,uint16 initialFood,uint16 initialGold,uint16 unitPrice,byte requested,FoodPurchase &output) {
        if (!partyCount || partyCount>6) return false;
        FoodPurchase next;next.food=initialFood;next.gold=initialGold;
        if (!requested) {next.result=kVendorCancelled;output=next;return true;}
        const uint16 capacity=uint16(partyCount*150),remaining=uint16(capacity-initialFood);
        // TALK food976B compares the low quotient BEFORE rounding. Even an
        // exact request for the rounded-up maximum takes the clipped branch.
        const byte quotient=byte(remaining/50);next.clipped=quotient<requested;
        next.quantity=next.clipped?byte(quotient+(remaining%50?1:0)):requested;
        if (!next.quantity) {next.result=kVendorFull;output=next;return true;}
        const uint32 price=uint32(next.quantity)*unitPrice;
        if (price>65535 || initialGold<price) {next.result=kVendorPoor;output=next;return true;}
        next.gold=uint16(initialGold-price);
        next.food=next.clipped?capacity:uint16(initialFood+uint(next.quantity)*50);
        next.result=kVendorBought;output=next;return true;
    }
};
// All authored offers are indexed by semantic role/map/NPC identity. Original
// offsets select immutable data; no service code, PC or bank ABI is executed.
class VendorCatalog {
public:
    bool load(HdvArchive &archive) {return archive.read("NA/SRTN.NPC.TALK",_data) && _data.size()==32831;}
    bool select(byte role,byte map,byte slot,VendorOffer &output) const {
        uint base,size,first,text;
        switch (role) {
        case 1:base=0x186f;size=0x1280;first=0x42f;text=5;break;
        case 3:base=0x4a98;size=0x376;first=0x2f4;text=14;break;
        case 4:base=0x57e7;size=0x4b7;first=0x1a2;text=3;break;
        case 6:base=0x656f;size=0x392;first=0x28a;text=5;break;
        default:return false;
        }
        if (_data.size()!=32831 || slot&7 || base+size>_data.size()) return false;
        for (uint at=first;at<size && _data[base+at]!=255;) {
            const uint length=_data[base+at];if (length<=text || length>size-at) return false;
            const byte *row=_data.data()+base+at;
            if (row[1]==map && row[2]==slot) {
                VendorOffer next;next.role=role;next.map=map;next.slot=slot;next.offset=uint16(base+at);next.textOffset=uint16(base+at+text);
                if (role==1 || role==6) next.price=READ_LE_UINT16(row+3);
                next.raw.resize(length);memcpy(next.raw.data(),row,length);output=next;return true;
            }
            at+=length;
        }
        return false;
    }
private:
    Common::Array<byte> _data;
};
} }
#endif
