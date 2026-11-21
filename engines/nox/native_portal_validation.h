/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_PORTAL_VALIDATION_H
#define NOX_NATIVE_PORTAL_VALIDATION_H
#include "nox/native_world_input.h"
#include "nox/native_world_status.h"
namespace Nox {
inline bool validateNativePortal491(HdvArchive &archive,Common::SeekableReadStream &fixture,Common::String &report) {
    using namespace Native;
    if (fixture.readUint32BE()!=MKTAG('N','P','4','T') || fixture.readUint32LE()!=1 || fixture.readUint32LE()!=59) return false;
    struct SourcePortal {byte type,map,slot,record[12];};Common::Array<SourcePortal> rows;
    uint policies=0,mapZero=0,edges=0,arrows=0,chimes=0;Common::String lockedText,selectedText;
    if (!WorldPortal::text(archive,false,lockedText) || !WorldPortal::text(archive,true,selectedText) || lockedText.empty() || selectedText.empty() || lockedText==selectedText || lockedText.size()>25 || selectedText.size()>25) return false;
    for (uint i=0;i<59;++i) {
        SourcePortal row;row.type=fixture.readByte();row.map=fixture.readByte();row.slot=fixture.readByte();if (fixture.read(row.record,12)!=12) return false;rows.push_back(row);
        if (!row.record[5]) ++mapZero;
        for (uint allowed=0;allowed<2;++allowed) for (uint mounted=0;mounted<2;++mounted) {
            report=Common::String::format("portal source%u permission%u mounted%u",i,allowed,mounted);byte fields[256]={};fields[row.record[9]]=allowed?row.record[11]:0;
            World world(archive);if (!world.initialize(row.type,row.map,row.record[0],row.record[1],fields,256,!mounted) || memcmp(world.currentTables()->data()+1024+row.slot,row.record,12)) return false;
            WorldScene scene;scene.objects.portal=row.slot;WorldTravelPlan plan;plan.type=255;
            const WorldPortalResult result=WorldPortal::prepare(world,scene,plan),expected=!allowed?kWorldPortalLocked:mounted && row.record[5]?kWorldPortalNeedsFoot:kWorldPortalSelected;
            if (result!=expected || (result==kWorldPortalSelected?(plan.type!=row.record[4] || plan.map!=row.record[5] || plan.x!=row.record[7] || plan.y!=row.record[8]):plan.type!=255)) return false;++policies;
        }
    }
    if (fixture.err() || fixture.pos()!=fixture.size() || mapZero!=2) return false;
    // Four unique destinations cover five authored edge-arrival producers.
    const byte edgeTypes[]={8,6,7,7},edgeMaps[]={69,45,57,78},edgeX[]={9,40,38,44},edgeY[]={43,8,44,6};
    for (uint i=0;i<4;++i) {
        report=Common::String::format("portal authored edge destination%u",i);byte fields[256]={};World world(archive);if (!world.initialize(edgeTypes[i],edgeMaps[i],edgeX[i],edgeY[i],fields,256)) return false;
        WorldStory window;WorldScene scene;StoryMapPatches story;ActorPaintRules actors;TerrainLighting lighting;OcclusionRules occlusion;TilePainter painter;WorldStatus status;HgrFrame frame;LightState light(1,0,0);VisibilityMask visible;PresentationState presentation;byte previous[187]={};
        if (!story.load(archive) || !actors.load(archive,edgeTypes[i]) || !lighting.load(archive,edgeTypes[i]) || !occlusion.load(archive,edgeTypes[i]) || !painter.load(archive,edgeTypes[i]) || !status.load(archive,edgeTypes[i]) || !status.initializeFrame(frame) || !visible.loadRules(archive) || !window.initialize(world,world.currentTables()->data()+1536,256) || !WorldViewport::refreshAtWorld(world,window,story,actors,lighting,occlusion,painter,246,0,0,previous,presentation,light,visible,frame,&scene)) return false;
        Common::Array<byte> projected;if (!window.project(world,projected) || projected.size()!=187) return false;
        Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!window.save(saved)) return false;WorldStory restored;Common::MemoryReadStream bytes(saved.getData(),saved.size());if (!restored.load(bytes,world)) return false;++edges;
    }
    for (uint profile=0;profile<3;++profile) {
        const byte type=profile==2?2:7,map=profile==2?104:54,slot=profile==2?160:56,targetX=profile==2?103:36,targetY=profile==2?19:11;
        report=Common::String::format("actual portal profile%u setup",profile);
        Common::ScopedPtr<NewGame> gameOwner(new NewGame(archive));NewGame &game=*gameOwner;if (!game.create("PORTAL",0,0)) return false;
        byte fields[256];memcpy(fields,game.world.questFields(),256);fields[60]=byte((fields[60]&~4)|(profile==1?4:0));if (profile==2) {fields[10]|=1;fields[57]&=~4;}
        game.clock.lightTicks=120;game.clock.lightActive=1;
        Common::ScopedPtr<WorldInput> command(new WorldInput);WorldStory window;WorldScene scene;WorldActorTurnState turn;WorldStatus status;StoryMapPatches story;ActorPaintRules actors;TerrainLighting lighting;OcclusionRules occlusion;TilePainter painter;TraversalRules traversal;
        LightState light(1,0,0);VisibilityMask visible;NpcRouteStore routes;GameplayRandom random;FixedMessageCatalog messages;TextGlyphSurface text;HgrFrame frame;byte previous[187]={},emptyRoutes[3584]={};
        if (!status.load(archive,type) || !status.initializeFrame(frame) || !story.load(archive) || !actors.load(archive,type) || !lighting.load(archive,type) || !occlusion.load(archive,type) || !painter.load(archive,type) || !traversal.load(archive,type) || !visible.loadRules(archive) || !routes.importPages(emptyRoutes,3584) || !random.initializeEntropy(119) || !messages.load(archive) || !text.initialize(archive,1) || !command->loadRules(archive,type)) return false;
        const int dx[]={0,0,-1,1},dy[]={1,-1,0,0};const byte near[]={76,110,94,92};byte direction=255;
        for (uint candidate=0;candidate<4;++candidate) {
            World setup(archive);if (!setup.initialize(type,map,byte(targetX+dx[candidate]),byte(targetY+dy[candidate]),fields,256)) return false;
            byte ground;if (!setup.terrainAt(setup.x(),setup.y(),ground)) return false;TraversalProbe probe;probe.terrain=ground;TraversalDecision decision;if (!traversal.probe(probe,decision) || decision.blocked || decision.requiresInteraction || (ground>=229 && ground<233)) continue;
            Common::MemoryWriteStreamDynamic initial(DisposeAfterUse::YES);if (!setup.save(initial)) return false;Common::MemoryReadStream load(initial.getData(),initial.size());if (!game.world.load(load)) return false;
            NpcSchedulePolicy schedulePolicy={false,false};if (NpcScheduler::apply(game.world,game.clock.hour,game.clock.minute,schedulePolicy,game.schedule)==kNpcScheduleUnsupported) return false;
            window=WorldStory();if (!window.initialize(game.world,game.world.currentTables()->data()+1536,256) || !WorldViewport::refreshAtWorld(game.world,window,story,actors,lighting,occlusion,painter,game.appearance,game.clock.lightActive,0,previous,turn.presentation,light,visible,frame,&scene)) return false;
            if (scene.objects.lookup[near[candidate]]==slot && scene.actors.lookup[near[candidate]]==255) {direction=byte(candidate);break;}
        }
        if (direction==255 || !status.paint(game,scene,light,false,frame) || !text.importPages(frame.data(),frame.data(),8192)) return false;
        // Declared source flee-style brief freeze isolates gate admission from
        // incidental NPC contact;this is a controlled checkpoint,not earned travel.
        turn.compact.freezeTimer=turn.npc.freezeTimer=3;
        SoundEvents sounds;command->setSoundEvents(&sounds);
        const uint16 health=game.party.member(0)->health();const TurnClock clock=game.clock;const Common::KeyCode keys[]={Common::KEYCODE_UP,Common::KEYCODE_DOWN,Common::KEYCODE_RIGHT,Common::KEYCODE_LEFT};Common::Event event;event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(keys[direction],0);
        report=Common::String::format("actual portal profile%u arrow",profile);const WorldInputResult result=command->handle(event,archive,game,window,scene,turn,light,visible,routes,random,messages,text,frame);
        report=Common::String::format("portal%u result%u map%u/%u xy%u,%u phase%u->%u HP%u->%u selected%u command%u",profile,uint(result),game.world.mapType(),game.world.mapId(),game.world.x(),game.world.y(),clock.phase,game.clock.phase,health,game.party.member(0)->health(),scene.objects.portal,uint(command->atWorldCommand()));
        // The original movement portal jumps to the destination before F134
        // charges the clock. A locked portal returns to the ordinary turn.
        const bool advancedClock=game.clock.phase!=clock.phase;
        if (result!=(profile?kWorldInputTransition:kWorldInputHandled) || !command->atWorldCommand() ||
            advancedClock!=(profile==0) || game.party.member(0)->health()!=health || command->worldDefeated()) return false;
        const byte expectedType=profile==2?0:7,expectedMap=profile==0?54:profile==1?57:0,expectedX=profile==0?36:profile==1?38:172,expectedY=profile==0?11:profile==1?44:64;
        if (game.world.mapType()!=expectedType || game.world.mapId()!=expectedMap || game.world.x()!=expectedX || game.world.y()!=expectedY) return false;++arrows;
        report=Common::String::format("actual portal profile%u core/modal codec save",profile);Common::MemoryWriteStreamDynamic core(DisposeAfterUse::YES),modal(DisposeAfterUse::YES);if (!game.save(core) || !command->save(modal)) return false;
        report=Common::String::format("actual portal profile%u core/modal codec load",profile);Common::MemoryReadStream coreBytes(core.getData(),core.size()),modalBytes(modal.getData(),modal.size());Common::ScopedPtr<NewGame> restored(new NewGame(archive));Common::ScopedPtr<WorldInput> restoredInput(new WorldInput);if (!restored->load(coreBytes) || !restoredInput->load(modalBytes,archive,expectedType) || !restoredInput->matchesWorld(restored->world) || restored->world.mapId()!=expectedMap || restored->world.x()!=expectedX || restored->world.y()!=expectedY) return false;
        report=Common::String::format("actual portal profile%u free G no repeat",profile);event.kbd=Common::KeyState(Common::KEYCODE_g,'G');const byte phase=game.clock.phase;if (command->handle(event,archive,game,window,scene,turn,light,visible,routes,random,messages,text,frame)!=kWorldInputHandled || game.clock.phase!=phase || game.world.mapId()!=expectedMap) return false;
        if (profile==1) {
            report="actual Castle57 edge stationary Wait";WorldWait wait;WorldActorTurnEvent encounter;if (!wait.load(archive,7)) return false;
            const WorldWaitResult waited=wait.run(game.world,window,scene,turn,game.clock,light,visible,game.schedule,routes,random,messages,text,frame,game.appearance,0,encounter,true,4,nullptr,&game.party);
            report=Common::String::format("Castle57 edge Wait result%u selected%u",uint(waited),scene.objects.portal);if (waited!=kWorldWaitAdvanced) return false;
            const WorldInputResult returned=command->finishTurn(archive,game,window,scene,turn,light,visible,messages,text,frame);
            report=Common::String::format("Castle57 return result%u map%u xy%u,%u",uint(returned),game.world.mapId(),game.world.x(),game.world.y());if (returned!=kWorldInputTransition || game.world.mapId()!=54 || game.world.x()!=36 || game.world.y()!=11) return false;
        }
        uint selections=0;for (uint n=0;n<sounds.size();++n) selections+=sounds[n]==kSpeakerSelection;
        if (selections!=(profile==1?2U:profile==2?1U:0U)) return false;chimes+=selections;
    }
    report=Common::String::format("portal\tsource_policies=%u\tmap0_producers=%u\tunique_edge_frames=%u\tactual_arrows=%u\tportal_chimes=%u\tcore_modal_codecs=3\tfree_G_no_repeat=3\tstationary_edge_return=1\twater_origin_damage_skipped=1\tcampaign=0\taudio_suite=0\n",policies,mapZero,edges,arrows,chimes);return true;
}
}
#endif
