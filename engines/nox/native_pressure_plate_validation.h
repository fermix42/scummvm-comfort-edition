/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_PRESSURE_PLATE_VALIDATION_H
#define NOX_NATIVE_PRESSURE_PLATE_VALIDATION_H
#include "nox/native_world_input.h"
#include "nox/native_world_status.h"
namespace Nox {
inline bool validateNativePressurePlate490(HdvArchive &archive,Common::SeekableReadStream &fixture,Common::String &report) {
    using namespace Native;
    if (fixture.readUint32BE()!=MKTAG('N','P','4','9') || fixture.readUint32LE()!=1 || fixture.readUint32LE()!=14) return false;
    uint plates=0,occupied=0;
    for (uint i=0;i<14;++i) {
        byte type=fixture.readByte(),map=fixture.readByte(),slot=fixture.readByte(),record[8];if (fixture.read(record,8)!=8) return false;
        for (uint guard=0;guard<2;++guard) {
            report=Common::String::format("sourceplate%u occupied%u",i,guard);byte fields[256]={};World world(archive);if (!world.initialize(type,map,record[0],record[1],fields,256) || memcmp(world.currentTables()->data()+1024+slot,record,8)) return false;
            WorldScene scene;scene.actors.lookup[93]=guard?0:255;scene.objects.lookup[93]=slot;
            if (!WorldPressurePlate::apply(world,scene) || world.field(record[4])!=(guard?0:record[7]) || !WorldPressurePlate::apply(world,scene) || world.field(record[4])!=(guard?0:record[7])) return false;
            Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!world.save(saved)) return false;World restored(archive);Common::MemoryReadStream bytes(saved.getData(),saved.size());if (!restored.load(bytes) || restored.field(record[4])!=world.field(record[4])) return false;
            if (guard) ++occupied;else ++plates;
        }
    }
    if (fixture.err() || fixture.pos()!=fixture.size()) return false;
    {
        report="actual Ruin64plate setup";
        Common::ScopedPtr<NewGame> gameOwner(new NewGame(archive));NewGame &game=*gameOwner;if (!game.create("PLATE",0,0)) return false;
        byte fields[256];memcpy(fields,game.world.questFields(),256);fields[31]=0;
        Common::ScopedPtr<WorldInput> owner(new WorldInput);WorldInput &command=*owner;
        WorldStory window;WorldScene scene;WorldActorTurnState turn;WorldStatus status;StoryMapPatches story;ActorPaintRules actors;TerrainLighting lighting;OcclusionRules occlusion;TilePainter painter;TraversalRules traversal;
        LightState light(1,0,0);VisibilityMask visible;NpcRouteStore routes;GameplayRandom random;FixedMessageCatalog messages;TextGlyphSurface text;HgrFrame frame;byte previous[187]={},emptyRoutes[3584]={};
        if (!status.load(archive,3) || !status.initializeFrame(frame) || !story.load(archive) || !actors.load(archive,3) || !lighting.load(archive,3) || !occlusion.load(archive,3) || !painter.load(archive,3) || !traversal.load(archive,3) || !visible.loadRules(archive) || !routes.importPages(emptyRoutes,3584) || !random.initializeEntropy(119) || !messages.load(archive) || !text.initialize(archive,1) || !command.loadRules(archive,3)) return false;
        const int dx[]={0,0,-1,1},dy[]={1,-1,0,0};const byte near[]={76,110,94,92};byte direction=255;
        // Actual authored Ruin64 pressure plate0 at11,12;field31 declared clear pre-input.
        // Try only its four adjacent setup cells before the first tested input.
        for (uint candidate=0;candidate<4;++candidate) {
            World setup(archive);if (!setup.initialize(3,64,byte(11+dx[candidate]),byte(12+dy[candidate]),fields,256)) return false;
            byte ground;if (!setup.terrainAt(setup.x(),setup.y(),ground)) return false;TraversalProbe probe;probe.terrain=ground;TraversalDecision decision;
            if (!traversal.probe(probe,decision) || decision.blocked || decision.requiresInteraction) continue;
            Common::MemoryWriteStreamDynamic core(DisposeAfterUse::YES);if (!setup.save(core)) return false;Common::MemoryReadStream load(core.getData(),core.size());if (!game.world.load(load)) return false;
            window=WorldStory();if (!window.initialize(game.world,game.world.currentTables()->data()+1536,256) || !WorldViewport::refreshAtWorld(game.world,window,story,actors,lighting,occlusion,painter,game.appearance,0,0,previous,turn.presentation,light,visible,frame,&scene)) return false;
            if (scene.actors.lookup[near[candidate]]==255 && scene.objects.lookup[near[candidate]]==0) {direction=byte(candidate);break;}
        }
        if (direction==255 || !status.paint(game,scene,light,false,frame) || !text.importPages(frame.data(),frame.data(),8192)) return false;


        WorldMove motion;WorldWait wait;if (!motion.load(archive,3) || !wait.load(archive,3)) return false;
        Common::Event event;event.type=Common::EVENT_KEYDOWN;const Common::KeyCode keys[]={Common::KEYCODE_UP,Common::KEYCODE_DOWN,Common::KEYCODE_RIGHT,Common::KEYCODE_LEFT};event.kbd=Common::KeyState(keys[direction],0);
        Common::Array<byte> notice;WorldActorTurnEvent encounter;report="actual Ruin64plate WorldMove/Wait";
        if (motion.run(event,game.world,window,scene,turn,game.clock,light,visible,game.schedule,routes,random,messages,text,frame,notice,encounter,0,nullptr,&game.party,false,game.appearance)!=kWorldMoveAdvanced || game.world.x()!=11 || game.world.y()!=12 || game.world.field(31)!=1) return false;
        report="actual Ruin64plate subsequent stationary turn";const byte phase=game.clock.phase;
        if (wait.run(game.world,window,scene,turn,game.clock,light,visible,game.schedule,routes,random,messages,text,frame,game.appearance,0,encounter,true,4,nullptr,&game.party)!=kWorldWaitAdvanced || game.world.field(31)!=1 || game.clock.phase==phase) return false;
        report="actual Ruin64plate corecodec";Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!game.save(saved)) return false;Common::MemoryReadStream bytes(saved.getData(),saved.size());Common::ScopedPtr<NewGame> restored(new NewGame(archive));if (!restored->load(bytes) || restored->world.field(31)!=1 || restored->world.x()!=11 || restored->world.y()!=12) return false;
    }
    report=Common::String::format("pressure-plate\tauthored_candidates=%u\toccupied_center_refusals=%u\tworld_codecs=28\tactual_Ruin64_move_wait=1\tactual_corecodec=1\tcampaign=0\taudio_suite=0\n",plates,occupied);return true;
}
}
#endif
