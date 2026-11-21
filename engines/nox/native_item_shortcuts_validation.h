/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ITEM_SHORTCUTS_VALIDATION_H
#define NOX_NATIVE_ITEM_SHORTCUTS_VALIDATION_H
#include "nox/native_inventory_window.h"
#include "nox/native_cast.h"
namespace Nox {
inline bool validateNativeItemShortcuts478(HdvArchive &archive,Common::String &report) {
    using namespace Native;
    ItemCatalog catalog;ItemDefinition definition;CastRules cast;
    if (!catalog.load(archive) || !catalog.definition(2,57,definition) || !cast.load(archive)) return false;
    for (uint slot=0;slot<10;++slot) {
        report=Common::String::format("book ready/cast shortcut%u",slot);
        NewGame game(archive);if (!game.create("BOOK",0,0)) return false;
        // All fixture mutations precede the tested InventoryWindow operation.
        byte empty[20];memset(empty,255,10);memset(empty+10,0,10);SpellBook book;if (!book.importState(empty,20)) return false;game.book=book;
        if (game.inventory.find(2,57)<0 && game.inventory.grantOne(catalog,2,57)!=kInventoryApplied) return false;
        byte members[768];
        for (uint i=0;i<6;++i) memcpy(members+i*128,game.party.member(i)->bytes,128);
        members[18]=definition.bytes[4];
        if (!game.party.applyBattleMembers(members,sizeof(members),game.party.memberCount())) return false;
        uint selected=0;bool found=false;
        for (uint i=0;i<256;++i) {
            const InventoryItem &item=*game.inventory.item(i);
            if (item.empty() || item.kind()!=2) continue;
            ItemDefinition candidate;
            if (!catalog.definition(2,item.id(),candidate)) return false;
            if ((candidate.bytes[12]&31)!=8) continue;
            if (item.id()==57) {found=true;break;}
            ++selected;
        }
        if (!found) return false;
        InventoryWindow input;HgrFrame frame;const TurnClock clock=game.clock;
        byte inventoryBefore[1536];Common::MemoryWriteStreamDynamic partyBefore(DisposeAfterUse::YES);if (!game.inventory.exportRecords(inventoryBefore,1536) || !game.party.save(partyBefore)) return false;
        if (!input.begin(archive,game,frame)) return false;
        auto send=[&](Common::KeyCode key,byte ascii=0) {Common::Event event;event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(key,ascii);return input.handle(event,archive,game,frame);};
        if (send(Common::KEYCODE_5,'5')!=kInventoryWindowHandled || input.tab()!=4) return false;
        for (uint i=0;i<selected;++i) if (send(Common::KEYCODE_DOWN)!=kInventoryWindowHandled) return false;
        if (send(Common::KEYCODE_r,'R')!=kInventoryWindowHandled || input.phase()!=2 || send(Common::KeyCode(Common::KEYCODE_0+slot),byte('0'+slot))!=kInventoryWindowHandled || input.phase()!=1 || send(Common::KEYCODE_ESCAPE)!=kInventoryWindowClosed) return false;
        for (uint i=0;i<10;++i) if (game.book.spell(i)!=(i==slot?57:255) || game.book.requirement(i)!=(i==slot?definition.bytes[4]:0)) return false;
        byte inventoryAfter[1536];Common::MemoryWriteStreamDynamic partyAfter(DisposeAfterUse::YES);if (!game.inventory.exportRecords(inventoryAfter,1536) || !game.party.save(partyAfter) || memcmp(inventoryBefore,inventoryAfter,1536) || partyBefore.size()!=partyAfter.size() || memcmp(partyBefore.getData(),partyAfter.getData(),partyBefore.size()) || memcmp(&clock,&game.clock,sizeof(TurnClock))) return false;
        Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!game.save(saved)) return false;
        NewGame restored(archive);Common::MemoryReadStream stream(saved.getData(),saved.size());if (!restored.load(stream)) return false;
        CharacterRecord caster=*restored.party.member(0);caster.bytes[4]=caster.bytes[18]=255;
        CastPreparation preparation;if (cast.prepare(restored.book,caster,byte(slot),false,preparation)!=kCastPrepared || preparation.spell!=57) return false;
    }
    report="items\tready_key_cast_shortcuts=10\tcore_roundtrips=10\topaque_party_inventory_preserved=1\tcampaign=0\taudio_suite=0\n";return true;
}
}
#endif
