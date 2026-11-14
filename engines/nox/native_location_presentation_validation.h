/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_LOCATION_PRESENTATION_VALIDATION_H
#define NOX_NATIVE_LOCATION_PRESENTATION_VALIDATION_H
#include "nox/native_world_input.h"
#include "nox/native_world_status.h"
namespace Nox {
inline bool validateNativeLocationPresentation489(HdvArchive &archive,Common::SeekableReadStream &fixture,Common::String &report) {
    using namespace Native;
    if (fixture.readUint32BE()!=MKTAG('N','L','4','8') || fixture.readUint32LE()!=1 || fixture.err() || fixture.pos()!=fixture.size()) return false;
    InformationCatalog catalog;Common::String help;if (!catalog.load(archive) || !catalog.help(help) || !help.contains("Comfort Edition") || !help.contains("Tab/Space again: inventory.") || !help.contains("In combat, Tab: inventory.") || !help.contains("N: reorder party.") || !help.contains("V: cycle sound mode.")) return false;
    const byte types[]={6,7,7},maps[]={2,4,5};uint labels=0;
    for (uint i=0;i<3;++i) {
        report=Common::String::format("CE category location type%u map%u",types[i],maps[i]);
        Common::ScopedPtr<NewGame> gameOwner(new NewGame(archive));NewGame &game=*gameOwner;if (!game.create("LOCATION",0,0)) return false;byte fields[256];memcpy(fields,game.world.questFields(),256);
        World setup(archive);if (!setup.initialize(types[i],maps[i],24,24,fields,256)) return false;Common::MemoryWriteStreamDynamic initial(DisposeAfterUse::YES);if (!setup.save(initial)) return false;Common::MemoryReadStream source(initial.getData(),initial.size());if (!game.world.load(source)) return false;
        Common::String label;if (!catalog.location(game.world,label) || label!=(i?"Castle":"Town")) return false;
        WorldStatus status;HgrFrame frame;TextGlyphSurface text;FixedMessageCatalog messages;Common::ScopedPtr<WorldInput> command(new WorldInput);GameplayRandom random;
        if (!status.load(archive,types[i]) || !status.initializeFrame(frame) || !text.initialize(archive,1) || !messages.load(archive) || !command->loadRules(archive,types[i]) || !random.initializeEntropy(119)) return false;
        WorldStory window;WorldScene scene;WorldActorTurnState turn;LightState light(1,0,0);VisibilityMask visible;NpcRouteStore routes;
        Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES);if (!game.save(before)) return false;const uint32 seed=random.seed();HgrFrame previous=frame;
        Common::Event event;event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(Common::KEYCODE_g,'G');
        report=Common::String::format("CE category actual G type%u map%u",types[i],maps[i]);
        if (command->handle(event,archive,game,window,scene,turn,light,visible,routes,random,messages,text,frame)!=kWorldInputHandled || !command->atWorldCommand() || !memcmp(previous.data(),frame.data(),8192) || random.seed()!=seed) return false;
        Common::MemoryWriteStreamDynamic after(DisposeAfterUse::YES);if (!game.save(after) || before.size()!=after.size() || memcmp(before.getData(),after.getData(),before.size())) return false;
        Common::MemoryReadStream bytes(after.getData(),after.size());Common::ScopedPtr<NewGame> restored(new NewGame(archive));Common::String retained;if (!restored->load(bytes) || !catalog.location(restored->world,retained) || retained!=label) return false;++labels;
    }
    report=Common::String::format("location-presentation\tCE_category_labels=%u\tactual_G_render_core_RNG_unchanged=3\tcore_roundtrips=3\tCE_help_bindings=5\toriginal_caption_parity=unknown\tcampaign=0\taudio_suite=0\n",labels);return true;
}
}
#endif
