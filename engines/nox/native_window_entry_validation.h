/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WINDOW_ENTRY_VALIDATION_H
#define NOX_NATIVE_WINDOW_ENTRY_VALIDATION_H
#include "nox/native_world_input.h"
#include "nox/native_world_status.h"
namespace Nox { namespace Native {
inline bool validateNativeWindowEntry493(HdvArchive &archive,Common::String &report) {
    Common::Array<byte> source;if (!archive.read("NA/BS.ROUTINES.BK2",source) || source.size()<0x28dc) return false;
    const byte *neighbors=source.data()+0x28d3;uint maps=0,entries=0,codecs=0;
    const byte positions[][2]={{0,0},{255,0},{0,255},{255,255},{0,128},{255,128},{128,0},{128,255}};
    for (uint family=0;family<2;++family) {
        Common::Array<byte> data;MapResource resource;
        if (!archive.read(family?"NA/DATA.MAP.UM":"NA/DATA.MAP.SURF",data) || !resource.open(data,!family,family!=0)) return false;
        for (uint map=0;map<resource.maps().size();++map) {
            const MapDefinition &definition=resource.maps()[map];if (!definition.compressed) continue;++maps;
            for (uint position=0;position<ARRAYSIZE(positions);++position) {
                const byte x=positions[position][0],y=positions[position][1];
                report=Common::String::format("compressed map%u/%u entry%u",family?2:0,uint(definition.id),position);
                byte fields[256]={};World world(archive);WorldStory window;
                if (!world.initialize(family?2:0,definition.id,x,y,fields,256) || !window.initialize(world,world.currentTables()->data()+1536,256)) return false;
                const byte center=byte((y>>4)*16+(x>>4));const StoryMapState *state=window.state();if (!state) return false;
                // Independent immutable source neighbor table, not the changed
                // initializer's row/column expression.
                for (uint slot=0;slot<9;++slot) {
                    const byte sector=neighbors[slot]&128?byte(center+(neighbors[slot]&127)):byte(center-neighbors[slot]);
                    Common::Array<byte> expected;if (!resource.sector(definition.id,sector,expected)) return false;
                    for (uint row=0;row<16;++row) if (memcmp(state->terrain+(slot/3*16+row)*48+slot%3*16,expected.data()+row*16,16)) return false;
                }
                Common::Array<byte> viewport;if (!window.project(world,viewport) || viewport.size()!=187) return false;
                Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!window.save(saved)) return false;WorldStory restored;Common::MemoryReadStream bytes(saved.getData(),saved.size());if (!restored.load(bytes,world)) return false;
                Common::MemoryWriteStreamDynamic again(DisposeAfterUse::YES);Common::Array<byte> next;
                if (!restored.save(again) || saved.size()!=again.size() || memcmp(saved.getData(),again.getData(),saved.size()) || !restored.project(world,next) || next!=viewport) return false;
                ++entries;++codecs;
            }
        }
    }
    // Actual world C -> shortcut0 -> caster Return -> orb1 -> full shared travel.
    // The checkpoint binds the orb and readies spell63 before the first input.
    report="compressed entry actual Cast setup";
    Common::ScopedPtr<NewGame> owner(new NewGame(archive));NewGame &game=*owner;if (!game.create("WINDOW",0,0) || game.navigation.bind(0,NavigationPoint(0,0,0,32))!=kNavigationSelected) return false;
    byte slots[20];memset(slots,255,10);memset(slots+10,0,10);slots[0]=63;SpellBook book;if (!book.importState(slots,20)) return false;game.book=book;
    Common::ScopedPtr<WorldInput> input(new WorldInput);WorldStory window;WorldScene scene;WorldActorTurnState turn;WorldStatus status;StoryMapPatches story;ActorPaintRules actors;TerrainLighting lighting;OcclusionRules occlusion;TilePainter painter;
    LightState light(1,0,0);VisibilityMask visible;NpcRouteStore routes;GameplayRandom random;FixedMessageCatalog messages;TextGlyphSurface text;HgrFrame frame;byte previous[187]={},emptyRoutes[3584]={};
    if (!status.load(archive,8) || !status.initializeFrame(frame) || !story.load(archive) || !actors.load(archive,8) || !lighting.load(archive,8) || !occlusion.load(archive,8) || !painter.load(archive,8) || !visible.loadRules(archive) || !routes.importPages(emptyRoutes,3584) || !random.initializeEntropy(119) || !messages.load(archive) || !text.initialize(archive,1) || !input->loadRules(archive,8) ||
        !window.initialize(game.world,game.world.currentTables()->data()+1536,256) || !WorldViewport::refreshAtWorld(game.world,window,story,actors,lighting,occlusion,painter,game.appearance,game.clock.lightActive,0,previous,turn.presentation,light,visible,frame,&scene) || !text.importPages(frame.data(),frame.data(),8192)) return false;
    const TurnClock clock=game.clock;Common::MemoryWriteStreamDynamic party(DisposeAfterUse::YES);if (!game.party.save(party)) return false;
    const Common::KeyCode keys[]={Common::KEYCODE_c,Common::KEYCODE_0,Common::KEYCODE_RETURN,Common::KEYCODE_1};const uint16 ascii[]={'c','0',13,'1'};
    for (uint key=0;key<4;++key) {
        report=Common::String::format("compressed entry actual Cast input%u",key);Common::Event event;event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(keys[key],ascii[key]);
        const WorldInputResult result=input->handle(event,archive,game,window,scene,turn,light,visible,routes,random,messages,text,frame);
        if (result!=(key==3?kWorldInputTransition:kWorldInputHandled)) return false;
    }
    if (!input->atWorldCommand() || game.world.mapType() || game.world.mapId() || game.world.x() || game.world.y()!=32 || !window.matchesWorld(game.world) || clock.phase!=game.clock.phase || clock.hour!=game.clock.hour || clock.minute!=game.clock.minute) return false;
    Common::MemoryWriteStreamDynamic afterParty(DisposeAfterUse::YES);if (!game.party.save(afterParty) || party.size()!=afterParty.size() || memcmp(party.getData(),afterParty.getData(),party.size())) return false;
    report="compressed entry core/window/input codecs";
    Common::MemoryWriteStreamDynamic core(DisposeAfterUse::YES),savedWindow(DisposeAfterUse::YES),savedInput(DisposeAfterUse::YES);if (!game.save(core) || !window.save(savedWindow) || !input->save(savedInput)) return false;
    Common::ScopedPtr<NewGame> loaded(new NewGame(archive));Common::MemoryReadStream coreBytes(core.getData(),core.size());if (!loaded->load(coreBytes)) return false;
    WorldStory restoredWindow;Common::MemoryReadStream windowBytes(savedWindow.getData(),savedWindow.size());if (!restoredWindow.load(windowBytes,loaded->world)) return false;
    Common::ScopedPtr<WorldInput> restoredInput(new WorldInput);Common::MemoryReadStream inputBytes(savedInput.getData(),savedInput.size());if (!restoredInput->load(inputBytes,archive,0)) return false;
    Common::Event event;event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(Common::KEYCODE_g,'g');
    report="compressed entry subsequent G";if (restoredInput->handle(event,archive,*loaded,restoredWindow,scene,turn,light,visible,routes,random,messages,text,frame)!=kWorldInputHandled || !restoredInput->atWorldCommand() || loaded->world.x() || loaded->world.y()!=32) return false;
    report=Common::String::format("window_entry\tcompressed_maps=%u\touter_entries=%u\twindow_codecs=%u\tactual_cast_travel=1\tcore_input_codecs=2\tsubsequent_G=1\tcampaign=0\taudio_suite=0\n",maps,entries,codecs);return true;
}
} }
#endif
