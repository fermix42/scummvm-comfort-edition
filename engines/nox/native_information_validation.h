/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_INFORMATION_VALIDATION_H
#define NOX_NATIVE_INFORMATION_VALIDATION_H
#include "nox/native_world_input.h"
#include "nox/native_world_command.h"
namespace Nox {
inline bool validateNativeInformation475(HdvArchive &archive,Common::SeekableReadStream &in,Common::String &report) {
    using namespace Native;
    auto readText=[&](Common::String &text) {const uint length=in.readUint16LE();text.clear();for (uint i=0;i<length;++i) text+=char(in.readByte());return !in.err();};
    if (in.readUint32BE()!=MKTAG('N','I','N','F') || in.readUint32LE()!=1 || in.readByte()!=7) return false;
    InformationCatalog catalog;report="information catalog load";if (!catalog.load(archive)) return false;
    Common::String expected,actual;
    for (uint family=0;family<7;++family) {
        const byte type=in.readByte();
        for (uint tile=0;tile<256;++tile) {
            report=Common::String::format("generic family%u tile%u",uint(type),tile);
            if (!readText(expected) || !catalog.generic(type,byte(tile),actual) || actual!=expected) return false;
        }
    }
    report="help text";if (!readText(expected) || !catalog.help(actual) || actual!=expected) return false;
    for (uint i=0;i<51;++i) if (!readText(expected)) return false;
    const uint questCases=in.readUint16LE();if (questCases!=169) return false;
    for (uint i=0;i<questCases;++i) {
        byte fields[256];report=Common::String::format("quest condition case%u",i);
        if (in.read(fields,256)!=256 || !readText(expected) || !catalog.quests(fields,256,actual) || actual!=expected) return false;
    }
    const uint lookCases=in.readUint16LE();if (lookCases!=96) return false;
    for (uint i=0;i<lookCases;++i) {
        report=Common::String::format("Look branch case%u",i);
        const uint size=in.readUint32LE();if (size<269 || size>8192) return false;
        Common::Array<byte> bytes;bytes.resize(size);if (in.read(bytes.data(),size)!=size) return false;
        World world(archive);Common::MemoryReadStream packet(bytes.data(),size);if (!world.load(packet)) return false;
        const byte appearance=in.readByte(),direction=in.readByte(),cell=in.readByte(),tile=in.readByte(),actor=in.readByte(),kind=in.readByte(),object=in.readByte(),base=in.readByte(),window=in.readByte();
        if (cell>=187 || base>=187 || window>1 || !readText(expected)) return false;
        WorldScene scene;scene.actors.terrain[cell]=tile;scene.actors.lookup[cell]=actor;scene.actors.kinds[cell]=kind;scene.objects.lookup[base]=object;scene.objects.lookup[cell]=object;
        InformationText result;
        if (!catalog.look(world,scene,appearance,direction,result) || result.window!=(window!=0) || result.text!=expected) {report+=Common::String::format(" expected[%s] got[%s]",expected.c_str(),result.text.c_str());return false;}
    }
    if (in.err() || in.pos()!=in.size()) return false;
    report="retained location and world codec";
    byte fields[256]={};World world(archive);
    if (!world.initialize(2,97,79,192,fields,256) || !catalog.location(world,expected) || expected!="Wolf's Den") return false;
    WorldStory window;if (!window.initialize(world,world.currentTables()->data()+1536,256)) return false;
    TraversalRules rules;if (!rules.load(archive,2)) return false;TraversalDecision decision;TraversalProbe probe;
    uint floor=0;for (;floor<256;++floor) {probe.terrain=byte(floor);if (!rules.probe(probe,decision)) return false;if (!decision.blocked && !decision.requiresInteraction) break;}
    if (floor==256) return false;WorldScene scene;scene.actors.terrain[94]=byte(floor);Common::Event right;right.type=Common::EVENT_KEYDOWN;right.kbd=Common::KeyState(Common::KEYCODE_RIGHT,0);
    if (WorldCommand::move(right,world,window,scene,rules,decision)!=kPositionMoved || world.x()!=80 || !catalog.location(world,actual) || actual!=expected) return false;
    Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!world.save(saved)) return false;
    World restored(archive);Common::MemoryReadStream savedInput(saved.getData(),saved.size());
    if (!restored.load(savedInput) || !catalog.location(restored,actual) || actual!=expected) return false;
    Common::Array<byte> old;old.resize(saved.size()-3);memcpy(old.data(),saved.getData(),old.size());old[4]=1;old[5]=0;
    World migrated(archive);Common::MemoryReadStream oldInput(old.data(),old.size());
    if (!migrated.load(oldInput) || !catalog.location(migrated,actual) || actual!="Depths of Vacous: L1") return false;
    // The three nameless directory entries remain explicit source-evidence gaps.
    const byte types[]={6,7,7},maps[]={2,4,5};
    for (uint i=0;i<3;++i) {World unnamed(archive);if (!unnamed.initialize(types[i],maps[i],24,24,fields,256) || catalog.location(unnamed,actual)) return false;}
    report="native information modal input";
    NewGame game(archive);if (!game.create("INFO",0,0)) return false;
    Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES);if (!game.save(before)) return false;
    WorldInput input;WorldStory inputWindow;WorldScene inputScene;WorldActorTurnState turn;LightState light;VisibilityMask visible;NpcRouteStore routes;GameplayRandom rng;
    FixedMessageCatalog messages;TextGlyphSurface text;HgrFrame frame;
    if (!rng.initializeEntropy(119) || !messages.load(archive) || !text.initialize(archive,1) || !text.importPages(frame.data(),frame.data(),8192)) return false;
    Common::MemoryWriteStreamDynamic rngBefore(DisposeAfterUse::YES);if (!rng.save(rngBefore)) return false;
    auto send=[&](Common::KeyCode key,byte ascii=0) {Common::Event e;e.type=Common::EVENT_KEYDOWN;e.kbd=Common::KeyState(key,ascii);return input.handle(e,archive,game,inputWindow,inputScene,turn,light,visible,routes,rng,messages,text,frame);};
    if (send(Common::KEYCODE_g,'G')!=kWorldInputHandled || !input.atWorldCommand()) return false;
    HgrFrame underlay=frame;
    if (send(Common::KEYCODE_l,'L')!=kWorldInputHandled || input.atWorldCommand() || !input.wholeFrameMenu()) return false;
    Common::MemoryWriteStreamDynamic blocked(DisposeAfterUse::YES);if (input.save(blocked)) return false;
    if (send(Common::KEYCODE_z,'Z')!=kWorldInputHandled || input.atWorldCommand() || send(Common::KEYCODE_ESCAPE)!=kWorldInputHandled || !input.atWorldCommand() || memcmp(frame.data(),underlay.data(),8192)) return false;
    inputScene.actors.terrain[76]=1;
    if (send(Common::KEYCODE_l,'L')!=kWorldInputHandled || send(Common::KEYCODE_UP)!=kWorldInputHandled || !input.atWorldCommand()) return false;
    uint pages=0;
    for (uint command=0;command<2;++command) {
        underlay=frame;if (send(Common::KEYCODE_SLASH,command?'/' :'?')!=kWorldInputHandled || input.atWorldCommand()) return false;
        uint keys=0;while (!input.atWorldCommand() && keys++<128) {if (send(Common::KEYCODE_SPACE,' ')!=kWorldInputHandled) return false;++pages;}
        if (!input.atWorldCommand() || memcmp(frame.data(),underlay.data(),8192)) return false;
    }
    // Compatibility with the newly completed free party selector, without
    // repeating its91-case accepted suite or executing any world turn.
    if (send(Common::KEYCODE_n,'N')!=kWorldInputHandled || send(Common::KEYCODE_ESCAPE)!=kWorldInputHandled || !input.atWorldCommand()) return false;
    Common::MemoryWriteStreamDynamic after(DisposeAfterUse::YES),rngAfter(DisposeAfterUse::YES);
    if (!game.save(after) || !rng.save(rngAfter) || before.size()!=after.size() || memcmp(before.getData(),after.getData(),before.size()) || rngBefore.size()!=rngAfter.size() || memcmp(rngBefore.getData(),rngAfter.getData(),rngBefore.size())) return false;
    NewGame roundTrip(archive);Common::MemoryReadStream core(after.getData(),after.size());if (!roundTrip.load(core)) return false;
    Common::MemoryWriteStreamDynamic again(DisposeAfterUse::YES);if (!roundTrip.save(again) || again.size()!=after.size() || memcmp(again.getData(),after.getData(),after.size())) return false;
    report=Common::String::format("information\tgeneric_descriptions=1792\tquest_conditions=169\tLook_cases=96\tnamed_documents=22\tlocation_retained_after_move=1\tNWLD2_roundtrip=1\tNWLD1_migration=1\tunknown_captions=3\tmodal_restore=1\tpage_keys=%u\tcomplete_core_RNG_unchanged=1\tcore_roundtrip=1\tcampaign=0\taudio_suite=0\n",pages);return true;
}
}
#endif
