/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_DIG_VALIDATION_H
#define NOX_NATIVE_DIG_VALIDATION_H
#include "nox/native_world_input.h"
#include "nox/native_world_status.h"
namespace Nox {
inline bool validateNativeDig484(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    using namespace Native;
    report="source Dig policy";
    if (input.readUint32BE()!=MKTAG('N','D','4','8') || input.readUint32LE()!=1 || input.readUint32LE()!=824) return false;
    for (uint i=0;i<824;++i) {
        byte row[8];if (input.read(row,8)!=8) return false;
        const byte result=!WorldDig::available(row[3])?28:row[4]?254:WorldDig::result(row[0],row[1],row[2]!=0);
        if (result!=row[5] || (result!=254 && result!=255)!=(row[6]!=0) || row[7]!=(result==255?41:16)) return false;
    }
    if (input.err() || input.pos()!=input.size() || WorldDig::result(3,91,false)!=18 || WorldDig::result(3,92,false)!=18) return false;
    uint refused=0,opened=0,pending=0;
    for (uint profile=0;profile<2;++profile) {
        report=Common::String::format("Dig profile%u authored setup",profile);
        Common::ScopedPtr<NewGame> gameOwner(new NewGame(archive));NewGame &game=*gameOwner;if (!game.create("DIG",0,0)) return false;
        byte fields[256];memcpy(fields,game.world.questFields(),256);fields[52]=byte((fields[52]&~8)|(profile?8:0));
        Common::ScopedPtr<WorldInput> owner(new WorldInput);WorldInput &command=*owner;
        WorldStory window;WorldScene scene;WorldActorTurnState turn;WorldStatus status;StoryMapPatches story;ActorPaintRules actors;TerrainLighting lighting;OcclusionRules occlusion;TilePainter painter;TraversalRules traversal;
        LightState light(1,0,0);VisibilityMask visible;NpcRouteStore routes;GameplayRandom random;FixedMessageCatalog messages;TextGlyphSurface text;HgrFrame frame;byte previous[187]={},emptyRoutes[3584]={};
        if (!status.load(archive,3) || !status.initializeFrame(frame) || !story.load(archive) || !actors.load(archive,3) || !lighting.load(archive,3) || !occlusion.load(archive,3) || !painter.load(archive,3) || !traversal.load(archive,3) || !visible.loadRules(archive) || !routes.importPages(emptyRoutes,3584) || !random.initializeEntropy(119) || !messages.load(archive) || !text.initialize(archive,1) || !command.loadRules(archive,3)) return false;
        const int dx[]={0,0,-1,1},dy[]={1,-1,0,0};const byte near[]={76,110,94,92};byte direction=255;
        // Actual authored Ruin31 coffin8 at10,15, declared shovel field predicate.
        // Try only its four adjacent setup cells before the first tested input.
        for (uint candidate=0;candidate<4;++candidate) {
            World setup(archive);if (!setup.initialize(3,31,byte(10+dx[candidate]),byte(15+dy[candidate]),fields,256)) return false;
            byte ground;if (!setup.terrainAt(setup.x(),setup.y(),ground)) return false;TraversalProbe probe;probe.terrain=ground;TraversalDecision decision;
            if (!traversal.probe(probe,decision) || decision.blocked || decision.requiresInteraction) continue;
            Common::MemoryWriteStreamDynamic core(DisposeAfterUse::YES);if (!setup.save(core)) return false;Common::MemoryReadStream load(core.getData(),core.size());if (!game.world.load(load)) return false;
            window=WorldStory();if (!window.initialize(game.world,game.world.currentTables()->data()+1536,256) || !WorldViewport::refreshAtWorld(game.world,window,story,actors,lighting,occlusion,painter,game.appearance,0,0,previous,turn.presentation,light,visible,frame,&scene)) return false;
            if (scene.objects.lookup[near[candidate]]==8) {direction=byte(candidate);break;}
        }
        if (direction==255 || game.world.currentTables()->data()[1034]!=92 || !status.paint(game,scene,light,false,frame) || !text.importPages(frame.data(),frame.data(),8192)) return false;
        auto send=[&](Common::KeyCode key,byte ascii=0) {Common::Event event;event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(key,ascii);return command.handle(event,archive,game,window,scene,turn,light,visible,routes,random,messages,text,frame);};
        const TurnClock clock=game.clock;const uint32 seed=random.seed();
        if (!profile) {
            report="Dig missing shovel source refusal";
            if (send(Common::KEYCODE_d,'D')!=kWorldInputHandled || !command.atWorldCommand() || !command.takeRefusalSound() || game.world.currentTables()->data()[1035]!=16) return false;
            ++refused;
        } else {
            report="Dig exclusive prompt/codec/cancel";
            if (send(Common::KEYCODE_d,'D')!=kWorldInputHandled || command.atWorldCommand()) return false;
            Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES);if (!game.save(before)) return false;
            if (send(Common::KEYCODE_n,'N')!=kWorldInputHandled || send(Common::KEYCODE_v,'V')!=kWorldInputHandled) return false;
            Common::MemoryWriteStreamDynamic state(DisposeAfterUse::YES);if (!command.save(state) || READ_LE_UINT16(state.getData()+4)!=16) return false;
            Common::ScopedPtr<WorldInput> restored(new WorldInput);Common::MemoryReadStream bytes(state.getData(),state.size());if (!restored->load(bytes,archive,3) || restored->atWorldCommand()) return false;command=*restored;
            if (send(Common::KEYCODE_ESCAPE)!=kWorldInputHandled || !command.atWorldCommand()) return false;
            Common::MemoryWriteStreamDynamic after(DisposeAfterUse::YES);if (!game.save(after) || before.size()!=after.size() || memcmp(before.getData(),after.getData(),before.size())) return false;
            ++pending;report="Dig actual authored coffin/input/cache";
            const Common::KeyCode keys[]={Common::KEYCODE_UP,Common::KEYCODE_DOWN,Common::KEYCODE_RIGHT,Common::KEYCODE_LEFT};
            // Source Dig writes41, then the existing visible/unoccupied object
            // presentation phase settles it to40 during the requested refresh.
            if (visible.cells()[near[direction]] || scene.actors.lookup[near[direction]]!=255) return false;
            const WorldInputResult begin=send(Common::KEYCODE_d,'D'),finish=send(keys[direction]);const bool sound=command.takeRefusalSound();
            const byte settled=game.world.currentTables()->data()[1035];
            if (begin!=kWorldInputHandled || finish!=kWorldInputHandled || !command.atWorldCommand() || sound || settled!=40) {report=Common::String::format("Dig authored begin%u finish%u idle%u sound%u settled%u",uint(begin),uint(finish),uint(command.atWorldCommand()),uint(sound),uint(settled));return false;}
            report="Dig opened core/cache codec";
            Common::MemoryWriteStreamDynamic core(DisposeAfterUse::YES),cache(DisposeAfterUse::YES);if (!game.save(core) || !window.save(cache)) return false;
            Common::ScopedPtr<NewGame> resumed(new NewGame(archive));WorldStory retained;Common::MemoryReadStream coreBytes(core.getData(),core.size()),cacheBytes(cache.getData(),cache.size());
            if (!resumed->load(coreBytes) || resumed->world.currentTables()->data()[1035]!=40 || !(resumed->world.field(52)&8) || !retained.load(cacheBytes,resumed->world)) return false;
            report="Dig subsequent location consumer";
            if (send(Common::KEYCODE_g,'G')!=kWorldInputHandled) return false;
            ++opened;
        }
        if (memcmp(&clock,&game.clock,sizeof(TurnClock)) || random.seed()!=seed) return false;
    }
    report=Common::String::format("dig\tsource_cases=824\tmissing_shovel=%u\tauthored_coffin_input_cache=%u\tprompt_cancel_codec=%u\talias_refusals=2\tcampaign=0\taudio_suite=0\n",refused,opened,pending);return true;
}
}
#endif
