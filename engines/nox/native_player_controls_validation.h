/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_CONTROLS_VALIDATION_H
#define NOX_NATIVE_CONTROLS_VALIDATION_H
#include "nox/native_world_input.h"
#include "nox/native_world_status.h"
namespace Nox {
inline bool validateNativeControls476(HdvArchive &archive,Common::SeekableReadStream &in,Common::String &report) {
    using namespace Native;
    if (in.readUint32BE()!=MKTAG('N','C','T','L') || in.readUint32LE()!=1 || in.readUint32LE()!=2304) return false;
    for (uint i=0;i<2304;++i) {
        byte row[7];if (in.read(row,7)!=7) return false;byte count=row[0];TurnClock clock;clock.phase=1;clock.hour=0x23;clock.minute=0x59;clock.hourlyCounter=9;clock.lightActive=row[1];clock.lightTicks=row[2];
        report=Common::String::format("torch source case%u",i);const TorchResult result=PlayerTorch::ignite(count,clock);
        const byte message=result==kTorchEmpty?17:result==kTorchIgnited?1:255;
        if (count!=row[3] || clock.lightActive!=row[4] || clock.lightTicks!=row[5] || message!=row[6] || clock.phase!=1 || clock.hour!=0x23 || clock.minute!=0x59 || clock.hourlyCounter!=9) return false;
    }
    byte icons[256];if (in.read(icons,256)!=256) return false;
    for (uint i=0;i<256;++i) if (PlayerIcon::next(byte(i))!=icons[i]) return false;
    report="equals/plus command separation";Common::KeyState key(Common::KEYCODE_EQUALS,'=');if (!PlayerIcon::requested(key)) return false;
    key.ascii='+';key.flags=Common::KBD_SHIFT;if (PlayerIcon::requested(key)) return false;
    key.ascii=0;if (PlayerIcon::requested(key)) return false;
    key.flags=0;if (!PlayerIcon::requested(key)) return false;
    if (in.readUint32LE()!=469) return false;
    for (uint i=0;i<469;++i) {const uint16 current=in.readUint16LE(),maximum=in.readUint16LE();const byte expected=in.readByte();report=Common::String::format("bar ratio%u",i);if (PartyRoster::segments(current,maximum)!=expected) return false;}
    if (in.readUint32LE()!=3600) return false;
    for (uint i=0;i<3600;++i) {
        byte row[9];if (in.read(row,9)!=9 || row[7]>=187) return false;WorldScene scene;scene.actors.terrain[93]=row[4];scene.actors.kinds[row[7]]=row[5];scene.actors.terrain[row[7]]=row[6];
        report=Common::String::format("wait safety case%u",i);if (HourWait::safe(row[0],row[1],row[2],row[3]!=0,scene)!=(row[8]!=0)) return false;
    }
    HourWait wait;if (!wait.load(archive) || in.readUint32LE()!=36) return false;
    for (uint i=0;i<36;++i) {
        byte before[6],expected[6];if (in.read(before,6)!=6) return false;const byte hours=in.readByte();const uint32 seed=in.readUint32LE();const byte tail=in.readByte();
        if (in.read(expected,6)!=6) return false;const uint32 afterSeed=in.readUint32LE();const byte afterTail=in.readByte(),last=in.readByte();const uint16 attempts=in.readUint16LE(),length=in.readUint16LE();
        Common::String replyExpected;for (uint j=0;j<length;++j) replyExpected+=char(in.readByte());
        TurnClock clock;clock.phase=before[0];clock.hour=before[1];clock.minute=before[2];clock.lightTicks=before[3];clock.lightActive=before[4];clock.hourlyCounter=before[5];byte counter=before[5];GameplayRandom random;
        Common::String reply;report=Common::String::format("hour transition%u",i);
        if (!random.importState(seed,tail) || wait.apply(hours,clock,counter,random,reply)!=kHourWaitApplied || memcmp(&clock,expected,6) || counter!=clock.hourlyCounter || random.seed()!=afterSeed || random.tail()!=afterTail || random.last()!=last || random.generationAttempts()!=attempts || reply!=replyExpected) return false;
    }
    if (in.err() || in.pos()!=in.size()) return false;
    report="six-member roster raster";
    Common::MemoryWriteStreamDynamic partyData(DisposeAfterUse::YES);partyData.writeUint32BE(MKTAG('N','P','A','R'));partyData.writeUint16LE(1);
    const byte hp[]={50,30,20,0,1,100},mp[]={15,5,0,20,30,30},hpBars[]={4,3,2,0,1,7},mpBars[]={4,2,0,0,7,7};
    for (uint i=0;i<6;++i) {byte record[128]={};record[0]=i==1?1:i==2?2:i==3?255:0;record[2]=hp[i];record[4]=mp[i];record[18]=30;record[72]=100;record[75]=byte('A'+i)|128;partyData.write(record,128);}
    byte equipment[96]={};partyData.write(equipment,96);Party party;Common::MemoryReadStream partySource(partyData.getData(),partyData.size());
    PartyRoster roster;HgrFrame rosterFrame,rosterUnderlay=rosterFrame;
    if (!party.load(partySource) || party.memberCount()!=6 || !roster.begin(archive,party,rosterFrame)) return false;
    for (uint i=0;i<6;++i) for (uint y=18+i*16;y<=19+i*16;++y) for (uint segment=1;segment<=7;++segment) {
        const byte pattern=segment&1?0x55:0x2a;
        const byte health=hpBars[i]>=segment?pattern:(!hpBars[i] && segment==1)?0x40:0;
        const byte magic=mpBars[i]>=segment?pattern:(!mpBars[i] && segment==1)?1:0;
        if (rosterFrame.data()[HgrFrame::rowOffset(y)+32-segment]!=health || rosterFrame.data()[HgrFrame::rowOffset(y)+31+segment]!=magic) return false;
    }
    if (!roster.close(rosterFrame) || memcmp(rosterFrame.data(),rosterUnderlay.data(),8192)) return false;
    report="world icon transport ownership";
    for (uint picture=246;picture<254;++picture) for (uint used=0;used<2;++used) {
        WorldTransportState controls;controls.used=used!=0;controls.appearance=controls.baseAppearance=controls.statusAppearance=byte(picture);byte appearance=byte(picture);
        if (!PlayerIcon::world(appearance,controls,true) || appearance!=icons[picture] || controls.baseAppearance!=appearance || controls.statusAppearance!=appearance) return false;
        byte objects[256]={},npcs[256]={},footer[72]={};objects[0]=24;objects[1]=24;objects[2]=127;
        TransportControlContext context={7,0,24,24,0,objects,npcs,footer};TransportControlOutcome outcome;
        if (WorldTransportControl::command(true,context,controls,outcome)!=kTransportChanged || controls.object!=0 || controls.appearance!=127) return false;
        byte refused=127;const byte hero=appearance;
        if (PlayerIcon::world(refused,controls,false) || refused!=127 || controls.baseAppearance!=hero) return false;
        Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!controls.save(saved)) return false;WorldTransportState loaded;Common::MemoryReadStream source(saved.getData(),saved.size());
        if (!loaded.load(source) || WorldTransportControl::command(false,context,loaded,outcome)!=kTransportChanged || loaded.object!=255 || loaded.appearance!=hero || loaded.baseAppearance!=hero) return false;
    }
    report="tactical icon records";
    for (uint member=0;member<6;++member) for (uint picture=246;picture<254;++picture) {
        BattleRecords records;for (uint i=0;i<6;++i) {records.stats[i*128+2]=50;records.stats[i*128+24]=246;records.compact[i*4+2]=246;}
        records.stats[member*128+24]=records.compact[member*4+2]=byte(picture);BattleRecords expected=records;
        expected.stats[member*128+24]=expected.compact[member*4+2]=icons[picture];
        if (!PlayerIcon::battle(records,6,byte(member)) || memcmp(records.stats,expected.stats,2048) || memcmp(records.compact,expected.compact,256) || memcmp(records.extended,expected.extended,256) || memcmp(records.indices,expected.indices,187) || memcmp(records.kinds,expected.kinds,187) || records.credit!=expected.credit) return false;
    }
    report="native control scene initialization";
    NewGame game(archive);if (!game.create("CONTROL",0,0)) return false;
    // Declared pre-operation torch setup through the normal native world codec.
    Common::MemoryWriteStreamDynamic initial(DisposeAfterUse::YES);if (!game.world.save(initial)) return false;
    Common::Array<byte> setup;setup.resize(initial.size());memcpy(setup.data(),initial.getData(),initial.size());setup[11+32]=2;
    Common::MemoryReadStream setupInput(setup.data(),setup.size());if (!game.world.load(setupInput)) return false;
    game.clock.lightActive=game.clock.lightTicks=0;
    WorldInput input;WorldStory window;WorldScene scene;WorldActorTurnState turn;WorldStatus status;StoryMapPatches story;ActorPaintRules actors;TerrainLighting lighting;OcclusionRules occlusion;TilePainter painter;
    LightState light(1,0,0);VisibilityMask visible;NpcRouteStore routes;GameplayRandom random;FixedMessageCatalog messages;TextGlyphSurface text;HgrFrame frame;byte previous[187]={},emptyRoutes[3584]={};
    if (!status.load(archive,8) || !status.initializeFrame(frame) || !window.initialize(game.world,game.world.currentTables()->data()+1536,256) || !story.load(archive) || !actors.load(archive,8) || !lighting.load(archive,8) || !occlusion.load(archive,8) || !painter.load(archive,8) || !visible.loadRules(archive) || !routes.importPages(emptyRoutes,3584) || !random.initializeEntropy(119) || !messages.load(archive) || !text.initialize(archive,1) || !input.loadRules(archive,8) || !WorldViewport::refreshAtWorld(game.world,window,story,actors,lighting,occlusion,painter,game.appearance,0,0,previous,turn.presentation,light,visible,frame,&scene) || !status.paint(game,scene,light,false,frame) || !text.importPages(frame.data(),frame.data(),8192)) return false;
    auto send=[&](Common::KeyCode key,byte ascii=0) {Common::Event e;e.type=Common::EVENT_KEYDOWN;e.kbd=Common::KeyState(key,ascii);return input.handle(e,archive,game,window,scene,turn,light,visible,routes,random,messages,text,frame);};
    report="roster cancellation and inventory handoff";
    Common::MemoryWriteStreamDynamic beforeMenu(DisposeAfterUse::YES),beforeRandom(DisposeAfterUse::YES);if (!game.save(beforeMenu) || !random.save(beforeRandom)) return false;HgrFrame underlay=frame;
    if (send(Common::KEYCODE_TAB)!=kWorldInputHandled || input.atWorldCommand() || !input.wholeFrameMenu()) return false;
    Common::MemoryWriteStreamDynamic blocked(DisposeAfterUse::YES);if (input.save(blocked)) return false;
    if (send(Common::KEYCODE_ESCAPE)!=kWorldInputHandled || !input.atWorldCommand() || memcmp(frame.data(),underlay.data(),8192)) return false;
    for (uint shortcut=0;shortcut<2;++shortcut) {
        if (send(Common::KEYCODE_TAB)!=kWorldInputHandled || send(shortcut?Common::KEYCODE_SPACE:Common::KEYCODE_TAB)!=kWorldInputHandled || input.atWorldCommand() || send(Common::KEYCODE_ESCAPE)!=kWorldInputHandled || !input.atWorldCommand() || memcmp(frame.data(),underlay.data(),8192)) return false;
    }
    Common::MemoryWriteStreamDynamic afterMenu(DisposeAfterUse::YES),afterRandom(DisposeAfterUse::YES);if (!game.save(afterMenu) || !random.save(afterRandom) || beforeMenu.size()!=afterMenu.size() || memcmp(beforeMenu.getData(),afterMenu.getData(),beforeMenu.size()) || beforeRandom.size()!=afterRandom.size() || memcmp(beforeRandom.getData(),afterRandom.getData(),beforeRandom.size())) return false;
    report="all world hero render controls";
    for (uint i=0;i<8;++i) {
        const byte old=game.appearance;const TurnClock clock=game.clock;
        if (send(Common::KEYCODE_EQUALS,'=')!=kWorldInputHandled || game.appearance!=icons[old] || !input.atWorldCommand() || !status.paint(game,scene,light,false,frame) || memcmp(&clock,&game.clock,6)) return false;
        const byte grounds[]={1,65,66,76};for (uint j=0;j<4;++j) {WorldScene sample=scene;sample.actors.lookup[93]=255;sample.actors.terrain[93]=grounds[j];HgrFrame image=frame;if (!status.paint(game,sample,light,false,image)) return false;}
    }
    report="torch production controls";const TurnClock beforeTorch=game.clock;
    if (send(Common::KEYCODE_i,'I')!=kWorldInputHandled || game.world.field(32)!=1 || game.clock.lightTicks!=120 || game.clock.lightActive!=1 || game.clock.phase!=beforeTorch.phase || game.clock.hour!=beforeTorch.hour || game.clock.minute!=beforeTorch.minute || input.takeRefusalSound()) return false;
    const TurnClock lit=game.clock;if (send(Common::KEYCODE_i,'I')!=kWorldInputHandled || game.world.field(32)!=1 || memcmp(&lit,&game.clock,6)) return false;
    report="hour prompt cancellation";underlay=frame;
    if (send(Common::KEYCODE_w,'W')!=kWorldInputHandled || input.atWorldCommand() || input.save(blocked) || send(Common::KEYCODE_ESCAPE)!=kWorldInputHandled || !input.atWorldCommand() || memcmp(frame.data(),underlay.data(),8192) || memcmp(&lit,&game.clock,6)) return false;
    report="hour wait production controls";const uint hour=((game.clock.hour>>4)*10+(game.clock.hour&15)+1)%24;const byte minute=game.clock.minute,phase=game.clock.phase;
    if (send(Common::KEYCODE_w,'W')!=kWorldInputHandled || send(Common::KEYCODE_1,'1')!=kWorldInputHandled || !input.atWorldCommand() || game.clock.hour!=byte((hour/10)*16+hour%10) || game.clock.minute!=minute || game.clock.phase!=phase || game.clock.lightTicks || game.clock.lightActive || game.clock.hourlyCounter!=game.world.field(20)) return false;
    if (send(Common::KEYCODE_i,'I')!=kWorldInputHandled || game.world.field(32)!=0 || game.clock.lightTicks!=120 || send(Common::KEYCODE_i,'I')!=kWorldInputHandled || game.world.field(32)!=0 || input.takeRefusalSound()) return false;
    report="native control core and input persistence";
    if (send(Common::KEYCODE_EQUALS,'=')!=kWorldInputHandled || game.appearance!=247) return false;
    Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!game.save(saved)) return false;NewGame loaded(archive);Common::MemoryReadStream source(saved.getData(),saved.size());
    if (!loaded.load(source) || loaded.appearance!=game.appearance || loaded.world.field(32)!=game.world.field(32) || memcmp(&loaded.clock,&game.clock,6)) return false;
    Common::MemoryWriteStreamDynamic controlState(DisposeAfterUse::YES);if (!input.save(controlState)) return false;WorldInput restored;Common::MemoryReadStream state(controlState.getData(),controlState.size());if (!restored.load(state,archive,game.world.mapType()) || !restored.matchesWorld(game.world) || !restored.atWorldCommand()) return false;
    report="controls\ttorch_cases=2304\ticon_arithmetic=256\tbar_ratios=469\tsafety_cases=3600\thour_rng_cases=36\tworld_heroes=8\ttransport_codec_chains=16\ttactical_member_icons=48\troster_inventory_handoffs=2\tmodal_restore=1\tclock_core_control_roundtrip=1\tcampaign=0\taudio_suite=0\n";return true;
}
}
#endif
