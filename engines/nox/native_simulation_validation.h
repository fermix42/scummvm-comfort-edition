/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_SIMULATION_VALIDATION_H
#define NOX_NATIVE_SIMULATION_VALIDATION_H
#include "nox/native_world_input.h"
#include "nox/native_world_status.h"
#include "nox/native_exploration_save.h"
namespace Nox {
inline bool validateNativeSimulation477(HdvArchive &archive,Common::SeekableReadStream &fixture,Common::String &report) {
    using namespace Native;
    report="daylight source fixture";
    if (fixture.readUint32BE()!=MKTAG('N','S','4','7') || fixture.readUint32LE()!=1) return false;
    const uint cases=fixture.readUint32LE();if (cases!=4224) return false;
    for (uint i=0;i<cases;++i) {
        byte row[11];if (fixture.read(row,11)!=11) return false;
        TurnClock clock;clock.hour=row[2];clock.minute=row[3];LightState light(row[5],row[6],row[7]);
        if (!Daylight::advance(row[0],row[1]!=0,row[4],clock,light) || light.mode!=row[8] || light.level!=row[9] || light.transitionTicks!=row[10]) return false;
    }
    if (fixture.err() || fixture.pos()!=fixture.size()) return false;
    Common::Array<byte> data;if (!archive.read("NA/GAME",data)) return false;
    uint mapCases=0;for (uint map=0;map<128;++map) {
        bool dark=false,expected=false;if (!Daylight::permanent(byte(map),data.data()+0x10b1,data.size()-0x10b1,dark)) return false;
        for (uint i=0x10b1;i<data.size() && !(data[i]&128);++i) if (data[i]==map && map) expected=true;
        if (dark!=expected) return false;++mapCases;
    }
    report="actual daylight input scene";NewGame game(archive);if (!game.create("DAYLIGHT",0,0)) return false;
    Common::ScopedPtr<WorldInput> inputOwner(new WorldInput);WorldInput &input=*inputOwner;WorldStory window;WorldScene scene;WorldActorTurnState turn;WorldStatus status;StoryMapPatches story;ActorPaintRules actors;TerrainLighting lighting;OcclusionRules occlusion;TilePainter painter;
    LightState light(1,0,0);VisibilityMask visible;NpcRouteStore routes;GameplayRandom random;FixedMessageCatalog messages;TextGlyphSurface text;HgrFrame frame;byte previous[187]={},emptyRoutes[3584]={};
    if (!status.load(archive,8) || !status.initializeFrame(frame) || !window.initialize(game.world,game.world.currentTables()->data()+1536,256) || !story.load(archive) || !actors.load(archive,8) || !lighting.load(archive,8) || !occlusion.load(archive,8) || !painter.load(archive,8) || !visible.loadRules(archive) || !routes.importPages(emptyRoutes,3584) || !random.initializeEntropy(119) || !messages.load(archive) || !text.initialize(archive,1) || !input.loadRules(archive,8) || !WorldViewport::refreshAtWorld(game.world,window,story,actors,lighting,occlusion,painter,game.appearance,0,0,previous,turn.presentation,light,visible,frame,&scene) || !text.importPages(frame.data(),frame.data(),8192)) return false;
    Common::Event event;event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(Common::KEYCODE_F12,0);
    const byte hours[]={5,0x20};
    for (uint i=0;i<2;++i) {
        game.clock.hour=hours[i];game.clock.minute=0x30;game.clock.phase=0;light=LightState(i?1:3,2,9);
        const TurnClock before=game.clock;
        if (input.handle(event,archive,game,window,scene,turn,light,visible,routes,random,messages,text,frame)!=kWorldInputIgnored || light.mode!=(i?2:0) || light.level!=(i?4:0) || light.transitionTicks || memcmp(&before,&game.clock,sizeof(TurnClock))) return false;
        // Continue the existing renderer transition without advancing gameplay.
        LightState expected=light;VisibilityMask mask=visible;if (!mask.prepareBaseLight(light,expected)) return false;
        byte painted[187]={};if (!WorldViewport::refreshAtWorld(game.world,window,story,actors,lighting,occlusion,painter,game.appearance,0,0,painted,turn.presentation,light,visible,frame,&scene) || light.mode!=expected.mode || light.level!=expected.level || light.transitionTicks!=expected.transitionTicks) return false;
    }
    report="daylight native core persistence";
    Common::MemoryWriteStreamDynamic core(DisposeAfterUse::YES);if (!game.save(core)) return false;
    Common::MemoryReadStream coreInput(core.getData(),core.size());NewGame restored(archive);if (!restored.load(coreInput)) return false;
    WorldAnimationState animation;EncounterState battle;if (!animation.synchronize(frame)) return false;
    Common::MemoryWriteStreamDynamic state(DisposeAfterUse::YES);if (!ExplorationSave::save(state,restored,window,scene,turn,routes,random,text,frame,visible,light,input,animation,battle)) return false;
    WorldStory restoredWindow;WorldScene restoredScene;VisibilityMask restoredVisible;LightState restoredLight;WorldActorTurnState restoredTurn;HgrFrame restoredFrame;Common::ScopedPtr<WorldInput> restoredInputOwner(new WorldInput);WorldInput &restoredInput=*restoredInputOwner;NpcRouteStore restoredRoutes;
    Common::MemoryReadStream saved(state.getData(),state.size());
    GameplayRandom restoredRandom;TextGlyphSurface restoredText;WorldAnimationState restoredAnimation;EncounterState restoredBattle;
    if (!ExplorationSave::load(saved,archive,restored,restoredWindow,restoredScene,restoredTurn,restoredRoutes,restoredRandom,restoredText,restoredFrame,restoredVisible,restoredLight,restoredInput,restoredAnimation,restoredBattle) || restoredLight.mode!=light.mode || restoredLight.level!=light.level || restoredLight.transitionTicks!=light.transitionTicks) return false;
    report=Common::String::format("simulation\tdaylight_source_cases=%u\tmap_policy_cases=%u\tactual_input_transitions=2\tviewport_continuations=2\tlight_codec_roundtrip=1\tcampaign=0\taudio_suite=0\n",cases,mapCases);return true;
}
}
#endif
