/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_PARTY_ORDER_VALIDATION_H
#define NOX_PARTY_ORDER_VALIDATION_H
#include "nox/native_world_input.h"
namespace Nox {
inline bool validatePartyOrder474(HdvArchive &archive,Common::SeekableReadStream &in,Common::String &report) {
    using namespace Native;
    if (in.readUint32BE()!=MKTAG('N','P','O','R') || in.readUint32LE()!=1 || in.readUint32LE()!=91) return false;
    EquipmentOwnership ownership;if (!ownership.load(archive)) return false;
    auto loadParty=[](const byte *bytes,Party &party) {
        Common::MemoryWriteStreamDynamic out(DisposeAfterUse::YES);out.writeUint32BE(MKTAG('N','P','A','R'));out.writeUint16LE(1);out.write(bytes,864);
        Common::MemoryReadStream stream(out.getData(),out.size());return party.load(stream);
    };
    auto records=[](const Party &party,const Inventory &items,byte *bytes) {
        for (uint i=0;i<6;++i) {memcpy(bytes+i*128,party.member(i)->bytes,128);memcpy(bytes+768+i*16,party.equipment(i)->bytes,16);}
        return items.exportRecords(bytes+864,1536);
    };
    byte uiRecords[2400]={};
    for (uint c=0;c<91;++c) {
        const byte count=in.readByte(),first=in.readByte(),second=in.readByte();byte before[2400],expected[2400],actual[2400];
        if (in.read(before,2400)!=2400 || in.read(expected,2400)!=2400) return false;
        Party party;Inventory items;
        if (!loadParty(before,party) || party.memberCount()!=count || !items.importRecords(before+864,1536) ||
                !PartyOrder::exchange(ownership,first,second,party,items) || !records(party,items,actual) || memcmp(actual,expected,2400)) {report=Common::String::format("Source record mismatch case %u",c);return false;}
        Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!party.save(saved)) return false;
        Party restored;Common::MemoryReadStream state(saved.getData(),saved.size());
        Inventory restoredItems;if (!restored.load(state) || !restoredItems.importRecords(actual+864,1536) || !records(restored,restoredItems,actual) || memcmp(actual,expected,2400)) return false;
        if (PartyOrder::exchange(ownership,count,0,party,items) || !records(party,items,actual) || memcmp(actual,expected,2400)) return false;
        if (count==3 && !first && !second) memcpy(uiRecords,before,2400);
    }
    if (in.err() || in.pos()!=in.size()) return false;
    Party unready;Inventory empty;if (PartyOrder::exchange(ownership,0,0,unready,empty)) return false;
    NewGame game(archive);if (!game.create("ORDER",0,0) || !loadParty(uiRecords,game.party) || !game.inventory.importRecords(uiRecords+864,1536)) return false;
    WorldInput input;WorldStory window;WorldScene scene;WorldActorTurnState turn;LightState light;VisibilityMask visible;NpcRouteStore routes;GameplayRandom random;
    if (!random.initializeEntropy(0x1234)) return false;
    FixedMessageCatalog catalog;TextGlyphSurface text;HgrFrame frame,underlay=frame;
    if (!text.initialize(archive,1) || !text.importPages(frame.data(),frame.data(),8192)) return false;
    auto send=[&](Common::KeyCode key,byte ascii=0) {Common::Event event;event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(key,ascii);return input.handle(event,archive,game,window,scene,turn,light,visible,routes,random,catalog,text,frame);};
    auto unchanged=[&]() {byte actual[2400];return records(game.party,game.inventory,actual) && !memcmp(actual,uiRecords,2400);};
    if (send(Common::KEYCODE_n,'n')!=kWorldInputHandled || input.atWorldCommand() || !input.wholeFrameMenu()) return false;
    Common::MemoryWriteStreamDynamic blocked(DisposeAfterUse::YES);if (input.save(blocked)) return false;
    if (send(Common::KEYCODE_ESCAPE)!=kWorldInputHandled || !input.atWorldCommand() || !unchanged() || memcmp(frame.data(),underlay.data(),8192)) return false;
    if (send(Common::KEYCODE_n,'n')!=kWorldInputHandled || send(Common::KEYCODE_RETURN)!=kWorldInputHandled || send(Common::KEYCODE_ESCAPE)!=kWorldInputHandled || !unchanged() || !input.atWorldCommand()) return false;
    // Third member is dead: source30F8 permits selecting it with arrows.
    const TurnClock clock=game.clock;const uint16 gold=game.gold,food=game.food;
    Common::MemoryWriteStreamDynamic worldBefore(DisposeAfterUse::YES),rngBefore(DisposeAfterUse::YES);if (!game.world.save(worldBefore) || !random.save(rngBefore)) return false;
    if (send(Common::KEYCODE_n,'n')!=kWorldInputHandled || send(Common::KEYCODE_RETURN)!=kWorldInputHandled || send(Common::KEYCODE_DOWN)!=kWorldInputHandled || send(Common::KEYCODE_DOWN)!=kWorldInputHandled || send(Common::KEYCODE_RETURN)!=kWorldInputHandled || !input.atWorldCommand()) return false;
    Party expectedParty;Inventory expectedItems;if (!loadParty(uiRecords,expectedParty) || !expectedItems.importRecords(uiRecords+864,1536) || !PartyOrder::exchange(ownership,0,2,expectedParty,expectedItems)) return false;
    byte actual[2400],expected[2400];if (!records(game.party,game.inventory,actual) || !records(expectedParty,expectedItems,expected) || memcmp(actual,expected,2400) || memcmp(frame.data(),underlay.data(),8192)) return false;
    Common::MemoryWriteStreamDynamic worldAfter(DisposeAfterUse::YES),rngAfter(DisposeAfterUse::YES);if (!game.world.save(worldAfter) || !random.save(rngAfter) || worldBefore.size()!=worldAfter.size() || memcmp(worldBefore.getData(),worldAfter.getData(),worldBefore.size()) || rngBefore.size()!=rngAfter.size() || memcmp(rngBefore.getData(),rngAfter.getData(),rngBefore.size())) return false;
    if (memcmp(&clock,&game.clock,sizeof(clock)) || gold!=game.gold || food!=game.food) return false;
    report="party-order\tcases=91\tfull_records=2400\tinvalid_atomic=91\tparty_roundtrip=91\tui_first_cancel=1\tui_second_cancel=1\tui_dead_arrow_swap=1\tframe_restored=1\tworld_clock_rng_unchanged=1\tcampaign=0\taudio_suite=0\n";return true;
}
}
#endif
