/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_INVENTORY_ACTIONS_H
#define NOX_NATIVE_INVENTORY_ACTIONS_H
#include "nox/native_new_game.h"
namespace Nox { namespace Native {
enum InventoryDropResult {kDropApplied,kDropCancelled,kDropProtected,kDropReadied,kDropUnsupported};
class InventoryActions {
public:
    static InventoryDropResult drop(HdvArchive &archive,NewGame &game,uint slot,bool confirmed) {
        ItemCatalog catalog;EquipmentOwnership ownership;
        if (!catalog.load(archive) || !ownership.load(archive)) return kDropUnsupported;
        return dropRecords(catalog,ownership,game.book,game.inventory,game.world._fields,256,slot,confirmed);
    }
    static InventoryDropResult dropRecords(const ItemCatalog &catalog,const EquipmentOwnership &ownership,const SpellBook &book,
            Inventory &inventory,byte *fields,uint length,uint slot,bool confirmed) {
        const InventoryItem *item=inventory.item(slot);ItemDefinition definition;
        if (!fields || length!=256 || !item || item->empty() || !catalog.definition(item->kind(),item->id(),definition)) return kDropUnsupported;
        if (definition.bytes[8]&128) return kDropProtected;
        if (!confirmed) return kDropCancelled;
        byte copies;if (!ownership.readyCopies(*item,copies) || copies>item->quantity()) return kDropUnsupported;
        byte quantity=byte(item->quantity()-copies);
        if (item->kind()==2 && item->id()<64) {
            bool ready=false;for (uint i=0;i<10;++i) ready|=book.spell(i)==definition.bytes[6];
            if (ready) --quantity;
        }
        if (!quantity) return kDropReadied;
        // A24B's counter hook reads retained caller fields264/269, not this
        // removal quantity. Its ordinary Drop caller lifetime is unresolved.
        // Do not substitute a guessed counter update or commit a partial drop.
        if (item->kind()==2 && (definition.bytes[8]&32)) return kDropUnsupported;
        Inventory next=inventory;if (!next.removeQuantity(slot,quantity)) return kDropUnsupported;
        inventory=next;return kDropApplied;
    }
    // Item Use's read-only document family. The source loader reads a table
    // then a768-byte text buffer; the offsets are content references,not code.
    static bool document(HdvArchive &archive,const ItemDefinition &definition,Common::String &output) {
        if (!(definition.bytes[8]&64) || !(definition.bytes[11]&128) || definition.bytes[6]>=76) return false;
        Common::Array<byte> data;if (!archive.read("NA/SRTN.INVENTORY",data) || data.size()!=67130) return false;
        const uint at=READ_LE_UINT16(data.data()+0xd4b3+uint(definition.bytes[6])*2);
        if (at<0xd54b || at>=data.size()) return false;
        Common::String text;
        for (uint i=0;i<768 && at+i<data.size();++i) {
            const byte c=data[at+i]&127;
            if (!c) {output=text;return true;}
            if (c!=13 && (c<32 || c>=127)) return false;
            text+=char(c);
        }
        return false;
    }
};
} }
#endif
