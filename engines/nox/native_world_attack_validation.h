/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_ATTACK_VALIDATION_H
#define NOX_NATIVE_WORLD_ATTACK_VALIDATION_H
#include "nox/native_world_input.h"
#include "nox/native_world_status.h"
#include "nox/native_encounter_state.h"
namespace Nox {
inline bool validateNativeWorldAttack480(HdvArchive &archive,Common::SeekableReadStream &fixture,Common::String &report) {
    using namespace Native;
    report="source world Attack admission";
    if (fixture.readUint32BE()!=MKTAG('N','A','4','8') || fixture.readUint32LE()!=1) return false;
    const uint count=fixture.readUint32LE();if (count!=2988) return false;
    for (uint i=0;i<count;++i) {
        byte params[8],tables[1792]={},expected[5];WorldScene scene;
        if (fixture.read(params,8)!=8 || fixture.read(scene.actors.lookup,187)!=187 || fixture.read(scene.actors.kinds,187)!=187 || fixture.read(tables+768,256)!=256 || fixture.read(tables+1280,256)!=256 || fixture.read(expected,5)!=5) return false;
        scene.actors.terrain[93]=params[6];const WorldAttackContext context={params[0],params[1],params[2],params[3],params[4],params[5],params[4]==119 || params[4]==123};
        WorldActorTurnEvent event;bool alarm=false;const WorldAttackResult result=WorldAttack::choose(context,params[7],scene,tables,event,alarm);
        if (uint(result)!=expected[0] || (result==kWorldAttackSelected && (event.bank!=expected[1] || event.slot!=expected[2] || event.cell!=expected[3] || alarm!=(expected[4]!=0) || event.direction!=params[7] || !event.battle))) return false;
    }
    report="source shared9417 alarm transaction";
    const uint alarms=fixture.readUint32LE();if (alarms!=24) return false;
    for (uint i=0;i<alarms;++i) {
        WorldTransportState controls;controls.alarm=fixture.readByte()!=0;controls.used=controls.alarm;byte footer[72],people[256],expected[256];
        if (fixture.read(footer,72)!=72 || fixture.read(people,256)!=256) return false;const bool alarm=fixture.readByte()!=0;
        if (fixture.read(expected,256)!=256) return false;
        WorldAttack::alarm(people,footer,controls);
        if (controls.alarm!=alarm || (alarm && !controls.used) || memcmp(people,expected,256)) return false;
    }
    if (fixture.err() || fixture.pos()!=fixture.size()) return false;
    uint handoffs=0,pending=0,returns=0;
    for (uint profile=0;profile<2;++profile) {
        report=Common::String::format("actual Attack profile%u setup",profile);
        Common::ScopedPtr<NewGame> gameOwner(new NewGame(archive));NewGame &game=*gameOwner;if (!game.create("ATTACK",0,0)) return false;
        const byte type=profile?5:8,map=profile?48:7,bank=profile?1:0,slot=profile?0:4;
        WorldTravelPlan plan;plan.type=type;plan.map=map;plan.x=plan.y=24;
        if (profile) {
            if (!WorldTravel::commit(game.world,plan)) return false;NpcSchedulePolicy policy={false,false};
            if (NpcScheduler::apply(game.world,game.clock.hour,game.clock.minute,policy,game.schedule)==kNpcScheduleUnsupported) return false;
        }
        const byte *target=game.world.currentTables()->data()+(bank?1280:768)+slot;const byte ax=target[0],ay=target[1];
        if (!target[2] || target[3]==255) return false;
        Common::ScopedPtr<WorldInput> inputOwner(new WorldInput);WorldInput &input=*inputOwner;
        WorldStory window;WorldScene scene;WorldActorTurnState turn;WorldStatus status;StoryMapPatches story;ActorPaintRules actors;TerrainLighting lighting;OcclusionRules occlusion;TilePainter painter;TraversalRules traversal;
        LightState light(1,0,0);VisibilityMask visible;NpcRouteStore routes;GameplayRandom random;FixedMessageCatalog messages;TextGlyphSurface text;HgrFrame frame;byte previous[187]={},emptyRoutes[3584]={};
        if (!status.load(archive,type) || !status.initializeFrame(frame) || !story.load(archive) || !actors.load(archive,type) || !lighting.load(archive,type) || !occlusion.load(archive,type) || !painter.load(archive,type) || !traversal.load(archive,type) || !visible.loadRules(archive) || !routes.importPages(emptyRoutes,3584) || !random.initializeEntropy(119) || !messages.load(archive) || !text.initialize(archive,1) || !input.loadRules(archive,type)) return false;
        const int dx[]={0,0,-1,1},dy[]={1,-1,0,0};const byte near[]={76,110,94,92};byte direction=255;
        // Read-only bounded setup candidates around the actual authored actor;
        // no operation/outcome is injected after the Attack command starts.
        for (uint candidate=0;candidate<4;++candidate) {
            plan.x=byte(ax+dx[candidate]);plan.y=byte(ay+dy[candidate]);if (plan.x<8 || plan.x>39 || plan.y<7 || plan.y>39 || !WorldTravel::commit(game.world,plan)) continue;
            byte ground;if (!game.world.terrainAt(plan.x,plan.y,ground)) continue;TraversalProbe probe;probe.terrain=ground;TraversalDecision decision;if (!traversal.probe(probe,decision) || decision.blocked || decision.requiresInteraction || (ground>=229 && ground<233)) continue;
            window=WorldStory();if (!window.initialize(game.world,game.world.currentTables()->data()+1536,256) || !WorldViewport::refreshAtWorld(game.world,window,story,actors,lighting,occlusion,painter,game.appearance,0,0,previous,turn.presentation,light,visible,frame,&scene)) return false;
            if (scene.actors.lookup[near[candidate]]==slot && (scene.actors.kinds[near[candidate]]&7)==(bank?4:0)) {direction=byte(candidate);break;}
        }
        if (direction==255 || !status.paint(game,scene,light,false,frame) || !text.importPages(frame.data(),frame.data(),8192)) return false;
        auto send=[&](WorldInput &owner,Common::KeyCode key,byte ascii=0) {Common::Event event;event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(key,ascii);return owner.handle(event,archive,game,window,scene,turn,light,visible,routes,random,messages,text,frame);};
        // Ordinary A first runs the existing world input-loop services. Town48
        // has two upcoming11:00 routes; completed prefetch entries are reusable,
        // so an ignored setup key does not make subsequent preparation inert.
        const TurnClock beforeClock=game.clock;
        report=Common::String::format("Attack profile%u prompt admission",profile);
        if (send(input,Common::KEYCODE_a,'A')!=kWorldInputHandled || input.atWorldCommand() || memcmp(&beforeClock,&game.clock,sizeof(TurnClock))) return false;
        Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES),rng(DisposeAfterUse::YES);
        if (!game.save(before) || !random.save(rng)) return false;
        report=Common::String::format("Attack profile%u exclusive pending key",profile);
        if (send(input,Common::KEYCODE_n,'N')!=kWorldInputHandled) return false;
        Common::MemoryWriteStreamDynamic state(DisposeAfterUse::YES);
        report=Common::String::format("Attack profile%u pending save",profile);
        if (!input.save(state) || READ_LE_UINT16(state.getData()+4)!=15) return false;
        Common::ScopedPtr<WorldInput> restoredOwner(new WorldInput);WorldInput &restored=*restoredOwner;Common::MemoryReadStream stateInput(state.getData(),state.size());
        report=Common::String::format("Attack profile%u pending load",profile);
        if (!restored.load(stateInput,archive,type) || restored.atWorldCommand()) return false;
        report=Common::String::format("Attack profile%u Escape",profile);
        if (send(restored,Common::KEYCODE_ESCAPE)!=kWorldInputHandled || !restored.atWorldCommand()) return false;
        Common::MemoryWriteStreamDynamic after(DisposeAfterUse::YES),rngAfter(DisposeAfterUse::YES);
        report=Common::String::format("Attack profile%u modal core/RNG invariants",profile);
        if (!game.save(after) || !random.save(rngAfter) || before.size()!=after.size() || memcmp(before.getData(),after.getData(),before.size()) || rng.size()!=rngAfter.size() || memcmp(rng.getData(),rngAfter.getData(),rng.size())) return false;
        ++pending;
        report="actual Attack existing encounter preparation";
        const Common::KeyCode keys[]={Common::KEYCODE_UP,Common::KEYCODE_DOWN,Common::KEYCODE_RIGHT,Common::KEYCODE_LEFT};
        if (send(restored,Common::KEYCODE_a,'A')!=kWorldInputHandled || send(restored,keys[direction])!=kWorldInputEncounter || restored.encounter().bank!=bank || restored.encounter().slot!=slot || restored.encounter().cell!=near[direction] || memcmp(&beforeClock,&game.clock,sizeof(TurnClock))) return false;
        PreparedEncounter prepared;Common::ScopedPtr<EncounterState> battleOwner(new EncounterState);EncounterState &battle=*battleOwner;
        if (!EncounterPreparation::prepare(archive,game,scene,turn,restored.encounter(),prepared) || !battle.begin(archive,game,prepared,random) || !battle.formed || !battle.round.enemyCount) return false;
        ++handoffs;
        if (profile) {
            report="retained post-return alarm owner (independent controlled return, not a won battle)";
            Common::MemoryWriteStreamDynamic queued(DisposeAfterUse::YES);if (!restored.save(queued) || READ_LE_UINT16(queued.getData()+4)!=15) return false;
            Common::ScopedPtr<WorldInput> returnOwner(new WorldInput);WorldInput &returned=*returnOwner;Common::MemoryReadStream queuedInput(queued.getData(),queued.size());if (!returned.load(queuedInput,archive,type) || returned.finishEncounter()) return false;
            // Test the return owner independently on the retained live-parent
            // (flee-compatible) world. No battle victory/defeat is manufactured.
            if (!returned.completeAttackReturn(archive,game) || !returned.completeAttackReturn(archive,game) || !returned.finishEncounter() || !returned.atWorldCommand()) return false;
            Common::MemoryWriteStreamDynamic controls(DisposeAfterUse::YES);if (!returned.save(controls) || READ_LE_UINT16(controls.getData()+4)!=5) return false;
            Common::MemoryReadStream transportBytes(controls.getData()+6,controls.size()-6);WorldTransportState alarmState;if (!alarmState.load(transportBytes) || !alarmState.used || !alarmState.alarm) return false;
            Common::ScopedPtr<WorldInput> finalOwner(new WorldInput);WorldInput &final=*finalOwner;Common::MemoryReadStream controlsInput(controls.getData(),controls.size());if (!final.load(controlsInput,archive,type) || !final.atWorldCommand()) return false;
            ++returns;
        }
    }
    report=Common::String::format("world-attack\tsource_admissions=%u\tsource_alarm_transactions=%u\tactual_encounter_formations=%u\tprompt_cancel_codecs=%u\tindependent_return_alarm_codec=%u\tcampaign=0\taudio_suite=0\n",count,alarms,handoffs,pending,returns);return true;
}
}
#endif
