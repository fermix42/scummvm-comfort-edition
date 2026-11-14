/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ORB_NAVIGATION_VALIDATION_H
#define NOX_NATIVE_ORB_NAVIGATION_VALIDATION_H
#include "nox/native_world_cast.h"
#include "nox/native_inventory_window.h"
#include "common/ptr.h"
namespace Nox { namespace Native {
inline bool validateNativeOrb492(HdvArchive &archive,Common::String &report) {
    uint policies=0,codecs=0,workflows=0;
    for (uint orb=0;orb<9;++orb) {
        report=Common::String::format("orb%u aligned policies",orb);
        WorldNavigation navigation;if (!navigation.initialize(archive)) return false;
        NavigationPoint actual(8,99,88,77);
        if (navigation.choose(byte(orb),actual)!=kNavigationUnbound || actual.map!=99) return false;++policies;
        for (uint profile=0;profile<3;++profile) {
            const NavigationPoint expected(profile==2?2:0,profile==2?97:0,profile==1?42:0,byte(32+orb));
            byte before[36];memcpy(before,navigation.orbs(),36);
            if (navigation.bind(byte(orb),expected)!=kNavigationSelected || navigation.choose(byte(orb),actual)!=kNavigationSelected ||
                actual.type!=expected.type || actual.map!=expected.map || actual.x!=expected.x || actual.y!=expected.y) return false;
            for (uint at=0;at<36;++at) if (at/4!=orb && navigation.orbs()[at]!=before[at]) return false;
            Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!navigation.save(saved)) return false;
            WorldNavigation restored;Common::MemoryReadStream bytes(saved.getData(),saved.size());if (!restored.load(bytes) || restored.choose(byte(orb),actual)!=kNavigationSelected ||
                actual.type!=expected.type || actual.map!=expected.map || actual.x!=expected.x || actual.y!=expected.y) return false;
            memcpy(before,navigation.orbs(),36);
            if (navigation.bind(byte(orb),NavigationPoint(7,58,20,13))!=kNavigationRefused || memcmp(before,navigation.orbs(),36)) return false;
            ++codecs;policies+=2;
        }
        report=Common::String::format("orb%u actual Use/Cast setup",orb);
        Common::ScopedPtr<NewGame> owner(new NewGame(archive));NewGame &game=*owner;if (!game.create("ORB",0,0)) return false;
        World setup(archive);if (!setup.initialize(0,0,0,byte(32+orb),game.world.questFields(),256)) return false;
        Common::MemoryWriteStreamDynamic world(DisposeAfterUse::YES);if (!setup.save(world)) return false;Common::MemoryReadStream worldBytes(world.getData(),world.size());if (!game.world.load(worldBytes)) return false;
        // Declared checkpoint inventory: one actual authored orb135..143.
        // Spell63 in shortcut0; no party/HP/MP outcome mutation.
        Inventory inventory;ItemCatalog catalog;ItemDefinition definition;
        if (!catalog.load(archive) || !catalog.definition(2,byte(135+orb),definition) || definition.bytes[6]!=orb || definition.bytes[11]!=64 || inventory.grantOne(catalog,2,byte(135+orb))!=kInventoryApplied) return false;
        game.inventory=inventory;byte slots[20];memset(slots,255,10);memset(slots+10,0,10);slots[0]=63;SpellBook book;if (!book.importState(slots,20)) return false;game.book=book;
        Common::MemoryWriteStreamDynamic partyBefore(DisposeAfterUse::YES);if (!game.party.save(partyBefore)) return false;byte items[1536];if (!game.inventory.exportRecords(items,1536)) return false;const TurnClock clock=game.clock;
        HgrFrame frame;Common::ScopedPtr<InventoryWindow> ui(new InventoryWindow);if (!ui->begin(archive,game,frame)) return false;
        Common::Event event;event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(Common::KEYCODE_u,'u');
        report=Common::String::format("orb%u actual inventory Use",orb);
        if (ui->handle(event,archive,game,frame)!=kInventoryWindowHandled || game.navigation.choose(byte(orb),actual)!=kNavigationSelected || actual.type || actual.map || actual.x || actual.y!=32+orb) return false;
        event.kbd=Common::KeyState(Common::KEYCODE_ESCAPE,27);if (ui->handle(event,archive,game,frame)!=kInventoryWindowClosed) return false;
        Common::MemoryWriteStreamDynamic core(DisposeAfterUse::YES);if (!game.save(core)) return false;Common::ScopedPtr<NewGame> loaded(new NewGame(archive));Common::MemoryReadStream coreBytes(core.getData(),core.size());if (!loaded->load(coreBytes)) return false;
        Common::ScopedPtr<WorldCast> casting(new WorldCast);if (!casting->begin(archive,*loaded,frame)) return false;
        event.kbd=Common::KeyState(Common::KEYCODE_0,'0');if (casting->handle(event,archive,*loaded,false,frame)!=kWorldCastHandled || casting->phase()!=2) return false;
        event.kbd=Common::KeyState(Common::KEYCODE_RETURN,13);if (casting->handle(event,archive,*loaded,false,frame)!=kWorldCastHandled || casting->phase()!=4) return false;
        Common::MemoryWriteStreamDynamic modal(DisposeAfterUse::YES);if (!casting->save(modal)) return false;Common::ScopedPtr<WorldCast> restored(new WorldCast);Common::MemoryReadStream modalBytes(modal.getData(),modal.size());if (!restored->load(modalBytes)) return false;
        event.kbd=Common::KeyState(Common::KeyCode(Common::KEYCODE_1+orb),uint16('1'+orb));
        report=Common::String::format("orb%u actual Cast selection",orb);
        if (restored->handle(event,archive,*loaded,false,frame)!=kWorldCastTravel) return false;
        actual=restored->destination();if (actual.type || actual.map || actual.x || actual.y!=32+orb) return false;
        Common::MemoryWriteStreamDynamic partyAfter(DisposeAfterUse::YES);byte after[1536];
        if (!loaded->party.save(partyAfter) || partyBefore.size()!=partyAfter.size() || memcmp(partyBefore.getData(),partyAfter.getData(),partyBefore.size()) ||
            !loaded->inventory.exportRecords(after,1536) || memcmp(items,after,1536) || clock.phase!=loaded->clock.phase || clock.hour!=loaded->clock.hour || clock.minute!=loaded->clock.minute) return false;
        codecs+=2;++workflows;
    }
    report=Common::String::format("orb\taligned_policies=%u\tcodecs=%u\tactual_use_cast=%u\tfull_map_travel=0\tcampaign=0\taudio_suite=0\n",policies,codecs,workflows);return true;
}
} }
#endif
