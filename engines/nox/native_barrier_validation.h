/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BARRIER_VALIDATION_H
#define NOX_NATIVE_BARRIER_VALIDATION_H
#include "nox/native_world_input.h"
#include "nox/native_world_status.h"
namespace Nox {
inline bool validateNativeBarrier488(HdvArchive &archive,Common::SeekableReadStream &fixture,Common::String &report) {
    using namespace Native;
    if (fixture.readUint32BE()!=MKTAG('N','B','4','8') || fixture.readUint32LE()!=1 || fixture.err() || fixture.pos()!=fixture.size()) return false;
    Common::Array<byte> source;if (!archive.read("NA/GAME",source)) return false;const byte damage=source[0x155];if (!damage) return false;
    const byte types[]={2,4,7,8},maps[]={97,97,58,73},tiles[]={206,206,208,222},flags[]={0,1,2,3,128,255};uint effects=0,mounted=0,moves=0;uint protectedOutcome=255;
    FixedMessageCatalog effectMessages;if (!effectMessages.load(archive)) return false;
    for (uint t=0;t<4;++t) for (uint f=0;f<6;++f) {
        report=Common::String::format("barrier source effect type%u flags%u",types[t],flags[f]);
        Common::ScopedPtr<NewGame> game(new NewGame(archive));if (!game->create("BARRIER",0,0)) return false;byte fields[256];memcpy(fields,game->world.questFields(),256);fields[57]=flags[f];
        // Declared controlled health prerequisite, before any tested operation.
        byte partyRecords[768];for (uint i=0;i<6;++i) memcpy(partyRecords+i*128,game->party.member(i)->bytes,128);WRITE_LE_UINT16(partyRecords+2,100);WRITE_LE_UINT16(partyRecords+4,100);if (!game->party.applyBattleMembers(partyRecords,768,game->party.memberCount())) return false;
        World world(archive);if (!world.initialize(types[t],maps[t],24,24,fields,256)) return false;
        WorldHazards hazards;WorldStatus status;HgrFrame frame;TextGlyphSurface text;if (!hazards.load(archive,types[t]) || !status.load(archive,types[t]) || !status.initializeFrame(frame) || !text.initialize(archive,1)) return false;
        for (uint low=0;low<2;++low) {
            Party party=game->party;const uint16 original=party.member(0)->health();if (!original || (low && !party.damage(0,original-1))) return false;const uint16 before=party.member(0)->health();bool dead=false;SoundEvents sounds;
            if (!hazards.barrier(world,party,effectMessages,text,frame,dead,&sounds)) return false;
            const bool protectedParty=(flags[f]&2)!=0;const uint16 expected=protectedParty?before:before>damage?before-damage:0;
            if (party.member(0)->health()!=expected || dead!=(expected==0) || sounds.size()!=(protectedParty?0U:1U) || (!protectedParty && sounds[0]!=kSpeakerMagic)) return false;++effects;
        }
        for (uint picture=127;picture<=128;++picture) {
            WorldTransportMotion rules;if (!rules.load(archive,types[t])) return false;WorldTransportState state;state.used=true;state.object=0;state.appearance=state.baseAppearance=state.statusAppearance=byte(picture);
            TransportMotionContext context;context.picture=byte(picture);context.currentTerrain=255;context.near.terrain=tiles[t];context.barrierProtection=(flags[f]&2)!=0;GameplayRandom random;if (!random.initializeEntropy(119)) return false;TransportMotionOutcome out;
            if (!rules.probe(context,state,random,out) || !out.barrierTouched || out.requiresInteraction==context.barrierProtection || out.blocked) return false;++mounted;
        }
    }
    for (uint profile=0;profile<2;++profile) {
        report=Common::String::format("actual Keep73 barrier profile%u setup",profile);
        Common::ScopedPtr<NewGame> gameOwner(new NewGame(archive));NewGame &game=*gameOwner;if (!game.create("BARRIER",0,0)) return false;
        byte partyRecords[768];for (uint i=0;i<6;++i) memcpy(partyRecords+i*128,game.party.member(i)->bytes,128);WRITE_LE_UINT16(partyRecords+2,100);WRITE_LE_UINT16(partyRecords+4,100);if (!game.party.applyBattleMembers(partyRecords,768,game.party.memberCount())) return false;
        byte fields[256];memcpy(fields,game.world.questFields(),256);fields[57]=byte((fields[57]&~2)|(profile?2:0));
        Common::ScopedPtr<WorldInput> owner(new WorldInput);WorldInput &command=*owner;
        WorldStory window;WorldScene scene;WorldActorTurnState turn;WorldStatus status;StoryMapPatches story;ActorPaintRules actors;TerrainLighting lighting;OcclusionRules occlusion;TilePainter painter;TraversalRules traversal;
        LightState light(1,0,0);VisibilityMask visible;NpcRouteStore routes;GameplayRandom random;FixedMessageCatalog messages;TextGlyphSurface text;HgrFrame frame;byte previous[187]={},emptyRoutes[3584]={};
        if (!status.load(archive,8) || !status.initializeFrame(frame) || !story.load(archive) || !actors.load(archive,8) || !lighting.load(archive,8) || !occlusion.load(archive,8) || !painter.load(archive,8) || !traversal.load(archive,8) || !visible.loadRules(archive) || !routes.importPages(emptyRoutes,3584) || !random.initializeEntropy(119) || !messages.load(archive) || !text.initialize(archive,1) || !command.loadRules(archive,8)) return false;
        const int dx[]={0,0,-1,1},dy[]={1,-1,0,0};const byte near[]={76,110,94,92};byte direction=255;
        // Actual authored Keep73 barrier at12,26;field57mask2 declared before input.
        // Try only its four adjacent setup cells before the first tested input.
        for (uint candidate=0;candidate<4;++candidate) {
            World setup(archive);if (!setup.initialize(8,73,byte(12+dx[candidate]),byte(26+dy[candidate]),fields,256)) return false;
            byte ground;if (!setup.terrainAt(setup.x(),setup.y(),ground)) return false;TraversalProbe probe;probe.terrain=ground;TraversalDecision decision;
            if (!traversal.probe(probe,decision) || decision.blocked || decision.requiresInteraction) continue;
            Common::MemoryWriteStreamDynamic core(DisposeAfterUse::YES);if (!setup.save(core)) return false;Common::MemoryReadStream load(core.getData(),core.size());if (!game.world.load(load)) return false;
            window=WorldStory();if (!window.initialize(game.world,game.world.currentTables()->data()+1536,256) || !WorldViewport::refreshAtWorld(game.world,window,story,actors,lighting,occlusion,painter,game.appearance,0,0,previous,turn.presentation,light,visible,frame,&scene)) return false;
            if (scene.actors.lookup[near[candidate]]==255 && scene.actors.terrain[near[candidate]]==222 && scene.objects.lookup[near[candidate]]==255) {direction=byte(candidate);break;}
        }
        if (direction==255 || !status.paint(game,scene,light,false,frame) || !text.importPages(frame.data(),frame.data(),8192)) return false;

        WorldMove motion;if (!motion.load(archive,8)) return false;SoundEvents sounds;motion.setSoundEvents(&sounds);
        const byte x=game.world.x(),y=game.world.y();const uint16 health=game.party.member(0)->health();const uint32 seed=random.seed();const TurnClock clock=game.clock;
        Common::Event event;event.type=Common::EVENT_KEYDOWN;const Common::KeyCode keys[]={Common::KEYCODE_UP,Common::KEYCODE_DOWN,Common::KEYCODE_RIGHT,Common::KEYCODE_LEFT};event.kbd=Common::KeyState(keys[direction],0);
        Common::Array<byte> notice;WorldActorTurnEvent encounter;report=Common::String::format("actual Keep73 barrier profile%u move",profile);
        const WorldMoveResult result=motion.run(event,game.world,window,scene,turn,game.clock,light,visible,game.schedule,routes,random,messages,text,frame,notice,encounter,0,nullptr,&game.party,false,game.appearance);
        if (!profile) {
            if (result!=kWorldMoveBlocked || game.world.x()!=x || game.world.y()!=y || game.party.member(0)->health()!=(health>damage?health-damage:0) || random.seed()!=seed || memcmp(&clock,&game.clock,sizeof(clock))) return false;
        } else {
            protectedOutcome=uint(result);if (result==kWorldMoveBoundary || result==kWorldMoveUnsupported || game.world.x()!=12 || game.world.y()!=26) return false;
        }
        Common::MemoryWriteStreamDynamic core(DisposeAfterUse::YES);if (!game.save(core)) return false;Common::MemoryReadStream bytes(core.getData(),core.size());Common::ScopedPtr<NewGame> restored(new NewGame(archive));if (!restored->load(bytes) || restored->world.x()!=game.world.x() || restored->world.y()!=game.world.y() || restored->party.member(0)->health()!=game.party.member(0)->health()) return false;++moves;
    }
    report=Common::String::format("barrier\tsource_effect_cases=%u\tmounted_barrier_probes=%u\tactual_Keep73_moves=%u\tprotected_actor_result=%u\tcore_roundtrips=2\tcampaign=0\taudio_suite=0\n",effects,mounted,moves,protectedOutcome);return true;
}
}
#endif
