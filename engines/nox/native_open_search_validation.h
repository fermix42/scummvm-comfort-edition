/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_OPEN_SEARCH_VALIDATION_H
#define NOX_NATIVE_OPEN_SEARCH_VALIDATION_H
#include "nox/native_world_input.h"
#include "nox/native_world_status.h"
namespace Nox {
inline bool validateNativeOpenSearch487(HdvArchive &archive,Common::SeekableReadStream &fixture,Common::String &report) {
    using namespace Native;
    if (fixture.readUint32BE()!=MKTAG('N','O','4','8') || fixture.readUint32LE()!=1 || fixture.err() || fixture.pos()!=fixture.size()) return false;
    Common::Array<byte> referenceCore,referenceModal;uint32 referenceSeed=0;uint pairs=0;
    for (uint profile=0;profile<2;++profile) {
        report=Common::String::format("shared Open/Search profile%u actual source setup",profile);
        Common::ScopedPtr<NewGame> gameOwner(new NewGame(archive));NewGame &game=*gameOwner;if (!game.create("OPEN",0,0)) return false;
        byte fields[256];memcpy(fields,game.world.questFields(),256);fields[33]=10;
        Common::ScopedPtr<WorldInput> owner(new WorldInput);WorldInput &command=*owner;
        WorldStory window;WorldScene scene;WorldActorTurnState turn;WorldStatus status;StoryMapPatches story;ActorPaintRules actors;TerrainLighting lighting;OcclusionRules occlusion;TilePainter painter;TraversalRules traversal;
        LightState light(1,0,0);VisibilityMask visible;NpcRouteStore routes;GameplayRandom random;FixedMessageCatalog messages;TextGlyphSurface text;HgrFrame frame;byte previous[187]={},emptyRoutes[3584]={};
        if (!status.load(archive,3) || !status.initializeFrame(frame) || !story.load(archive) || !actors.load(archive,3) || !lighting.load(archive,3) || !occlusion.load(archive,3) || !painter.load(archive,3) || !traversal.load(archive,3) || !visible.loadRules(archive) || !routes.importPages(emptyRoutes,3584) || !random.initializeEntropy(119) || !messages.load(archive) || !text.initialize(archive,1) || !command.loadRules(archive,3)) return false;
        const int dx[]={0,0,-1,1},dy[]={1,-1,0,0};const byte near[]={76,110,94,92};byte direction=255;
        // Actual authored Ruin31 locked quest chest28 at33,14;ten picks before input.
        // Try only its four adjacent setup cells before the first tested input.
        for (uint candidate=0;candidate<4;++candidate) {
            World setup(archive);if (!setup.initialize(3,31,byte(33+dx[candidate]),byte(14+dy[candidate]),fields,256)) return false;
            byte ground;if (!setup.terrainAt(setup.x(),setup.y(),ground)) return false;TraversalProbe probe;probe.terrain=ground;TraversalDecision decision;
            if (!traversal.probe(probe,decision) || decision.blocked || decision.requiresInteraction) continue;
            Common::MemoryWriteStreamDynamic core(DisposeAfterUse::YES);if (!setup.save(core)) return false;Common::MemoryReadStream load(core.getData(),core.size());if (!game.world.load(load)) return false;
            window=WorldStory();if (!window.initialize(game.world,game.world.currentTables()->data()+1536,256) || !WorldViewport::refreshAtWorld(game.world,window,story,actors,lighting,occlusion,painter,game.appearance,0,0,previous,turn.presentation,light,visible,frame,&scene)) return false;
            if (scene.objects.lookup[near[candidate]]==28) {direction=byte(candidate);break;}
        }
        if (direction==255 || game.world.currentTables()->data()[1054]!=8 || !status.paint(game,scene,light,false,frame) || !text.importPages(frame.data(),frame.data(),8192)) return false;
        auto send=[&](Common::KeyCode key,byte ascii=0) {Common::Event event;event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(key,ascii);return command.handle(event,archive,game,window,scene,turn,light,visible,routes,random,messages,text,frame);};

        const Common::KeyCode arrows[]={Common::KEYCODE_UP,Common::KEYCODE_DOWN,Common::KEYCODE_RIGHT,Common::KEYCODE_LEFT};
        const Common::KeyCode initial=profile?Common::KEYCODE_s:Common::KEYCODE_o;const byte initialAscii=profile?'S':'O';
        report=Common::String::format("shared Open/Search profile%u admit/cancel",profile);
        if (send(initial,initialAscii)!=kWorldInputHandled || send(arrows[direction])!=kWorldInputHandled || command.atWorldCommand()) return false;
        Common::MemoryWriteStreamDynamic pending(DisposeAfterUse::YES);if (!command.save(pending) || READ_LE_UINT16(pending.getData()+4)!=11) return false;
        if (send(Common::KEYCODE_v,'V')!=kWorldInputHandled || send(Common::KEYCODE_ESCAPE)!=kWorldInputHandled || !command.atWorldCommand() || game.world.currentTables()->data()[1055]!=53 || game.world.field(33)!=10) return false;
        report=Common::String::format("shared Open/Search profile%u re-admit/codec",profile);
        if (send(initial,initialAscii)!=kWorldInputHandled || send(arrows[direction])!=kWorldInputHandled) return false;
        Common::MemoryWriteStreamDynamic state(DisposeAfterUse::YES);if (!command.save(state)) return false;
        Common::MemoryReadStream bytes(state.getData(),state.size());Common::ScopedPtr<WorldInput> restored(new WorldInput);if (!restored->load(bytes,archive,3)) return false;command=*restored;
        report=Common::String::format("shared Open/Search profile%u actual lock attempt",profile);
        if (send(Common::KEYCODE_1,'1')!=kWorldInputHandled || command.atWorldCommand()) return false;
        Common::MemoryWriteStreamDynamic core(DisposeAfterUse::YES),modal(DisposeAfterUse::YES);if (!game.save(core) || !command.save(modal)) return false;
        if (!profile) {referenceCore.resize(core.size());memcpy(referenceCore.data(),core.getData(),core.size());referenceModal.resize(modal.size());memcpy(referenceModal.data(),modal.getData(),modal.size());referenceSeed=random.seed();}
        else {
            if (referenceCore.size()!=core.size() || memcmp(referenceCore.data(),core.getData(),core.size())) {report="Open/Search core mismatch";return false;}
            if (referenceSeed!=random.seed()) {report="Open/Search RNG mismatch";return false;}
            if (referenceModal.size()!=modal.size()) {report=Common::String::format("Open/Search modal sizes %u/%u",referenceModal.size(),modal.size());return false;}
            for (uint i=0;i<modal.size();++i) if (referenceModal[i]!=modal.getData()[i]) {
                report=Common::String::format("Open/Search modal first difference byte%u: %u/%u; core and RNG identical",i,uint(referenceModal[i]),uint(modal.getData()[i]));
                return false;
            }
            ++pairs;
        }
    }
    report=Common::String::format("open-search\tactual_authored_chest=1\tO_S_lock_attempt_equivalent=%u\tadmission_cancel=2\tmodal_codecs=2\tcore_RNG_modal_equal=1\tcampaign=0\taudio_suite=0\n",pairs);return true;
}
}
#endif
