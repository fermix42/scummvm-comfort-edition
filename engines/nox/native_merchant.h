/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_MERCHANT_H
#define NOX_NATIVE_MERCHANT_H
#include "nox/native_inventory.h"
namespace Nox { namespace Native {
struct MerchantOffer {
    byte map, actor, buyPercent, stockPercent, otherPercent;
    Common::Array<byte> stock;
    Common::String welcome;
    MerchantOffer() : map(0),actor(0),buyPercent(0),stockPercent(0),otherPercent(0) {}
    bool stocks(byte kind,byte id) const {
        for (uint i=0;i+1<stock.size();i+=2) {
            if (stock[i]&128) break;
            if ((stock[i]&191)==kind && stock[i+1]==id) return true;
        }
        return false;
    }
};
// Immutable HDV records. Merchant stock is an authored offer list, not party
// inventory; purchases do not decrement it and sales do not append to it.
class MerchantCatalog {
public:
    bool load(HdvArchive &archive) {
        Common::Array<byte> data;
        if (!archive.read("NA/SRTN.INVENTORY",data) || data.size()!=67130) return false;
        uint at=0x4100,count=0;
        while (at<0x48c9) {
            const uint size=data[at],text=data[at+3];
            if (size<9 || at+size>0x48c9 || text<8 || text>=size || (text&1)) return false;
            if (data[at+size-1]!=0) return false;
            at+=size;++count;
        }
        if (at!=0x48c9 || count!=22) return false;
        _data=data;return true;
    }
    bool select(byte map,byte actor,MerchantOffer &output) const {
        for (uint at=0x4100;at<0x48c9 && at<_data.size();at+=_data[at]) {
            if (_data[at+1]!=map || _data[at+2]!=actor) continue;
            MerchantOffer next;next.map=map;next.actor=actor;
            next.buyPercent=_data[at+5];next.stockPercent=_data[at+6];next.otherPercent=_data[at+7];
            const uint text=_data[at+3],end=at+_data[at];
            for (uint p=at+8;p<at+text;++p) next.stock.push_back(_data[p]);
            for (uint p=at+text;p<end && _data[p];++p) next.welcome+=char(_data[p]&127);
            output=next;return true;
        }
        return false;
    }
    bool price(byte kind,byte id,uint16 &output) const {
        // B0D8's three source pointers, mapped to file7650's loaded price data.
        static const uint starts[]={0,0xcc,0x1f6},counts[]={102,149,168};
        if (kind>2 || id>=counts[kind] || _data.size()!=67130) return false;
        output=READ_LE_UINT16(_data.data()+0x7650+starts[kind]+uint(id)*2);return true;
    }
private:
    Common::Array<byte> _data;
};
enum MerchantResult { kMerchantApplied,kMerchantPoor,kMerchantFull,kMerchantOverflow,
    kMerchantCancelled,kMerchantNoValue,kMerchantGoldFull,kMerchantUnavailable,kMerchantReadyBook };
struct MerchantTransaction {
    MerchantResult result;byte quantity;uint16 unitPrice,total;
    MerchantTransaction() : result(kMerchantCancelled),quantity(0),unitPrice(0),total(0) {}
};
class MerchantTransactions {
public:
    static uint16 percent(uint16 value,byte rate) {return uint16(uint32(value)*rate/100);}
    static bool buy(const MerchantCatalog &prices,const ItemCatalog &items,const MerchantOffer &offer,
            byte kind,byte id,byte requested,Inventory &inventory,byte *fields,uint length,
            uint16 &gold,MerchantTransaction &output) {
        if (!fields || length!=256 || !offer.stocks(kind,id)) return false;
        ItemDefinition definition;uint16 base;
        if (!items.definition(kind,id,definition) || !prices.price(kind,id,base)) return false;
        MerchantTransaction next;
        // B151 bypasses numeric input for the non-ammunition weapon range.
        const byte quantity=kind==0 && id<64?1:requested;
        if (!quantity) {output=next;return true;}
        next.unitPrice=percent(base,offer.buyPercent);
        const uint32 total=uint32(next.unitPrice)*quantity;
        if (total>65535 || gold<total) {next.result=kMerchantPoor;output=next;return true;}
        Inventory candidate=inventory;
        const InventoryResult result=candidate.buyQuantity(definition,kind,id,quantity);
        if (result!=kInventoryApplied) {
            if (result==kInventoryInvalid) return false;
            next.result=result==kInventoryQuantityOverflow?kMerchantOverflow:kMerchantFull;
            output=next;return true;
        }
        inventory=candidate;gold=uint16(gold-total);counter(definition,kind,quantity,false,fields);
        next.result=kMerchantApplied;next.quantity=quantity;next.total=uint16(total);output=next;return true;
    }
    // available excludes readied equipment, as the original inventory list does.
    // Book/category identity and readied IDs come from their existing owners.
    static bool sell(const MerchantCatalog &prices,const ItemCatalog &items,const MerchantOffer &offer,
            uint slot,byte available,bool book,bool readyBook,byte choice,Inventory &inventory,
            byte *fields,uint length,uint16 &gold,MerchantTransaction &output) {
        const InventoryItem *item=inventory.item(slot);
        if (!fields || length!=256 || !item || item->empty() || available>item->quantity()) return false;
        const byte kind=item->kind(),id=item->id();ItemDefinition definition;uint16 base;
        if (!items.definition(kind,id,definition) || !prices.price(kind,id,base)) return false;
        MerchantTransaction next;next.unitPrice=MAX<uint16>(1,percent(base,offer.stocks(kind,id)?offer.stockPercent:offer.otherPercent));
        if (!base) {next.result=kMerchantNoValue;output=next;return true;}
        if (choice!=1 && choice!=2) {output=next;return true;}
        const uint16 room=uint16(9999-gold),fit=room/next.unitPrice;
        if (!fit) {next.result=kMerchantGoldFull;output=next;return true;}
        byte quantity=choice==1?1:byte(MIN<uint16>(fit,available));
        if (!available) {next.result=kMerchantUnavailable;output=next;return true;}
        if (book && readyBook) {
            if (available==1) {next.result=kMerchantReadyBook;output=next;return true;}
            // B32E decrements an already gold-limited all quantity as well.
            if (quantity!=1) --quantity;
        }
        Inventory candidate=inventory;if (!candidate.removeQuantity(slot,quantity)) return false;
        const uint16 total=uint16(uint32(quantity)*next.unitPrice);
        inventory=candidate;gold=uint16(gold+total);counter(definition,kind,quantity,true,fields);
        next.result=kMerchantApplied;next.quantity=quantity;next.total=total;output=next;return true;
    }
private:
    static void counter(const ItemDefinition &definition,byte kind,byte quantity,bool remove,byte *fields) {
        // A24B tracks selected miscellaneous items by their authored quest field.
        if (kind==2 && (definition.bytes[8]&32)) {
            byte &value=fields[definition.bytes[7]];value=byte(remove?value-quantity:value+quantity);
        }
    }
};
} }
#endif
