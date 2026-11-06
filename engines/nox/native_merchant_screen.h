/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_MERCHANT_SCREEN_H
#define NOX_NATIVE_MERCHANT_SCREEN_H
#include "nox/native_merchant.h"
#include "nox/native_equipment_ownership.h"
#include "nox/native_new_game.h"
namespace Nox { namespace Native {
struct MerchantSelection {
    byte kind,id,available;uint slot;
    MerchantSelection(byte k,byte i,byte q,uint s) : kind(k),id(i),available(q),slot(s) {}
};
class MerchantScreen {
public:
    static bool selections(HdvArchive &archive,const NewGame &game,const MerchantOffer &offer,bool sale,Common::Array<MerchantSelection> &output) {
        Common::Array<MerchantSelection> next;
        if (sale) {
            EquipmentOwnership ownership;if (!ownership.load(archive)) return false;
            for (uint i=0;i<256;++i) {
                const InventoryItem &item=*game.inventory.item(i);if (item.empty()) continue;
                byte used;if (!ownership.readyCopies(item,used) || used>item.quantity()) return false;
                next.push_back(MerchantSelection(item.kind(),item.id(),byte(item.quantity()-used),i));
            }
        } else {
            for (uint i=0;i+1<offer.stock.size();i+=2) {
                if (offer.stock[i]&128) break;
                next.push_back(MerchantSelection(offer.stock[i]&191,offer.stock[i+1],0,256));
            }
        }
        output=next;return true;
    }
    static bool paint(HdvArchive &archive,const NewGame &game,const MerchantOffer &offer,bool sale,uint selected,
            byte phase,const Common::String &query,HgrFrame &frame) {
        ItemCatalog items;MerchantCatalog prices;GlyphFont font;Common::Array<MerchantSelection> list;
        if (!items.load(archive) || !prices.load(archive) || !font.load(archive,1) || !selections(archive,game,offer,sale,list)) return false;
        HgrFrame next;
        if (!line(font,next,2,1,sale?"Sell inventory":"Buy equipment") ||
            !line(font,next,2,3,Common::String::format("Gold: %u",game.gold))) return false;
        const uint page=selected/6*6;
        for (uint row=0;row<6 && page+row<list.size();++row) {
            const MerchantSelection &entry=list[page+row];ItemDefinition definition;uint16 base;
            if (!items.definition(entry.kind,entry.id,definition) || !prices.price(entry.kind,entry.id,base)) return false;
            Common::String name;
            for (uint j=13;j<32 && definition.bytes[j];++j) name+=char(definition.bytes[j]&127);
            const byte rate=sale?(offer.stocks(entry.kind,entry.id)?offer.stockPercent:offer.otherPercent):offer.buyPercent;
            const uint16 cost=sale?MAX<uint16>(1,MerchantTransactions::percent(base,rate)):MerchantTransactions::percent(base,rate);
            Common::String text=Common::String::format("%c %s",page+row==selected?'>':' ',name.c_str());
            if (!line(font,next,1,6+row*2,text) || !line(font,next,28,6+row*2,
                sale?Common::String::format("%u x%u",cost,entry.available):Common::String::format("%u",cost))) return false;
        }
        const Common::String prompt=phase==24?Common::String::format("How many? %s",query.c_str()):phase==25?"Sell: Y=one A=all Escape=cancel":"Arrows: select  B:buy S:sell";
        if (!line(font,next,1,20,prompt) || !line(font,next,1,22,"Return: select  Escape: leave")) return false;
        frame=next;return true;
    }
private:
    static bool line(const GlyphFont &font,HgrFrame &frame,uint x,uint y,const Common::String &text) {
        for (uint i=0;i<text.size() && x+i<40;++i)
            if (!frame.drawGlyph(font,byte(text[i])|128,x+i,y,0)) return false;
        return true;
    }
};
} }
#endif
