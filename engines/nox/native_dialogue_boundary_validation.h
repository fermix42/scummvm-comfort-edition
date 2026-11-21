/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_DIALOGUE_BOUNDARY_VALIDATION_H
#define NOX_NATIVE_DIALOGUE_BOUNDARY_VALIDATION_H
#include "nox/native_world_input.h"
#include "nox/native_world_status.h"
namespace Nox {
inline bool validateNativeDialogueBoundary486(HdvArchive &archive,Common::SeekableReadStream &fixture,Common::String &report) {
    using namespace Native;
    if (fixture.readUint32BE()!=MKTAG('N','Q','4','8') || fixture.readUint32LE()!=1 || fixture.readUint32LE()!=2 || fixture.readUint32LE()!=6 || fixture.err() || fixture.pos()!=fixture.size()) return false;
    Common::ScopedPtr<NewGame> baseline(new NewGame(archive));if (!baseline->create("HEADER",0,0)) return false;
    byte fields[256];memcpy(fields,baseline->world.questFields(),256);fields[174]=90;
    uint repairs=0,teachers=0,modals=0;Common::String expectedJob[2];
    for (uint i=0;i<2;++i) {
        report=Common::String::format("authored JOB resource%u",i);
        ConversationContext context;context.map=i?9:1;context.mapType=6;context.hour=0x10;context.serviceAccess=3;context.adultMode=true;
        Common::ScopedPtr<Conversation> conversation(new Conversation);GameplayRandom random;if (!random.initializeEntropy(119)) return false;
        if (!conversation->begin(archive,i?"NA/DATA.TLK.L005":"NA/DATA.TLK.L001",i?8:128,fields,256,context) || conversation->submit("JOB",&random)!=kConversationReply || conversation->reply().empty() || conversation->fields()[174]!=90) return false;
        expectedJob[i]=conversation->reply();
        Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!conversation->save(saved)) return false;
        Common::MemoryReadStream bytes(saved.getData(),saved.size());Common::ScopedPtr<Conversation> restored(new Conversation);
        if (!restored->load(bytes,archive) || restored->fields()[174]!=90 || restored->submit("NAME",&random)!=kConversationReply || restored->reply().empty()) return false;
        ++repairs;
    }
    const char *resources[]={"NA/DATA.TLK.L001","NA/DATA.TLK.L00B","NA/DATA.TLK.L00E","NA/DATA.TLK.L00F","NA/DATA.TLK.L00F","NA/DATA.TLK.L011"};
    const byte actors[]={0,40,16,64,72,96},masks[]={1,2,1,4,16,8},maps[]={1,28,38,46,46,48};
    for (uint i=0;i<6;++i) {
        report=Common::String::format("authored complete teacher header%u",i);
        ConversationContext context;context.map=maps[i];context.mapType=i>=3?5:6;context.hour=0x10;context.serviceAccess=2;context.adultMode=true;
        Common::ScopedPtr<Conversation> conversation(new Conversation);GameplayRandom random;if (!random.initializeEntropy(119)) return false;
        // L001 actor0 is a qualified placeholder opening, not a normal workflow
        // claim. This directly checks its authored valid TRAI header only.
        if (!conversation->begin(archive,resources[i],actors[i],fields,256,context) || conversation->submit("TRAI",&random)!=kConversationReply || !conversation->trainingPending() || conversation->fields()[18]!=10 || conversation->fields()[19] || conversation->fields()[22]!=masks[i] || conversation->fields()[174]!=90 || !conversation->completeTraining() || conversation->trainingPending()) return false;
        ++teachers;
    }
    for (uint profile=0;profile<2;++profile) {
        report=Common::String::format("scheduled JOB modal%u setup",profile);
        const byte map=profile?9:1,slot=profile?8:128;
        Common::ScopedPtr<NewGame> gameOwner(new NewGame(archive));NewGame &game=*gameOwner;if (!game.create("HEADER",0,0)) return false;game.gold=10;
        World setup(archive);if (!setup.initialize(6,map,24,24,fields,256)) return false;
        Common::MemoryWriteStreamDynamic initial(DisposeAfterUse::YES);if (!setup.save(initial)) return false;Common::MemoryReadStream initialBytes(initial.getData(),initial.size());if (!game.world.load(initialBytes)) return false;
        NpcSchedulePolicy policy={false,false};if (NpcScheduler::apply(game.world,game.clock.hour,game.clock.minute,policy,game.schedule)==kNpcScheduleUnsupported) return false;
        const byte *actor=game.world.currentTables()->data()+1280+slot;const byte x=actor[0],y=actor[1];if (!actor[2] || (actor[5]&127)!=16) return false;
        WorldStory window;WorldScene scene;WorldActorTurnState turn;WorldStatus status;StoryMapPatches story;ActorPaintRules actorsRules;TerrainLighting lighting;OcclusionRules occlusion;TilePainter painter;TraversalRules traversal;
        LightState light(1,0,0);VisibilityMask visible;FixedMessageCatalog messages;TextGlyphSurface text;HgrFrame frame;byte previous[187]={};
        if (!status.load(archive,6) || !status.initializeFrame(frame) || !story.load(archive) || !actorsRules.load(archive,6) || !lighting.load(archive,6) || !occlusion.load(archive,6) || !painter.load(archive,6) || !traversal.load(archive,6) || !visible.loadRules(archive) || !messages.load(archive) || !text.initialize(archive,1)) return false;
        const int dx[]={0,0,-1,1},dy[]={1,-1,0,0};const byte near[]={76,110,94,92};byte direction=255;
        for (uint candidate=0;candidate<4;++candidate) {
            WorldTravelPlan plan;plan.type=6;plan.map=map;plan.x=byte(x+dx[candidate]);plan.y=byte(y+dy[candidate]);
            if (plan.x<8 || plan.x>39 || plan.y<7 || plan.y>39 || !WorldTravel::commit(game.world,plan)) continue;
            byte ground;if (!game.world.terrainAt(plan.x,plan.y,ground)) return false;TraversalProbe probe;probe.terrain=ground;TraversalDecision decision;if (!traversal.probe(probe,decision) || decision.blocked || decision.requiresInteraction) continue;
            window=WorldStory();if (!window.initialize(game.world,game.world.currentTables()->data()+1536,256) || !WorldViewport::refreshAtWorld(game.world,window,story,actorsRules,lighting,occlusion,painter,game.appearance,0,0,previous,turn.presentation,light,visible,frame,&scene)) return false;
            if (scene.actors.lookup[near[candidate]]==slot && (scene.actors.kinds[near[candidate]]&135)==4) {direction=byte(candidate);break;}
        }
        if (direction==255 || !status.paint(game,scene,light,false,frame)) return false;
        Common::ScopedPtr<WorldConversation> modal(new WorldConversation);GameplayRandom random;if (!random.initializeEntropy(119) || !modal->choose(game.world,frame)) return false;
        const byte initialField=game.world.field(174);
        auto send=[&](Common::KeyCode key,byte ascii=0) {Common::Event event;event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(key,ascii);byte message=255;return modal->handle(event,archive,game,scene,frame,message,nullptr,&random);};
        auto drain=[&](bool close) {for (uint n=0;n<96;++n) {if (!modal->active()) return close;if (modal->stage()==3) return !close;if (send(Common::KEYCODE_RETURN,13)==kWorldConversationUnsupported) return false;}return false;};
        auto word=[&](const char *value) {for (uint i=0;value[i];++i) if (send(Common::KeyCode(Common::KEYCODE_a+value[i]-'A'),byte(value[i]))!=kWorldConversationHandled) return false;return send(Common::KEYCODE_RETURN,13)!=kWorldConversationUnsupported;};
        const Common::KeyCode keys[]={Common::KEYCODE_UP,Common::KEYCODE_DOWN,Common::KEYCODE_RIGHT,Common::KEYCODE_LEFT};
        report=Common::String::format("scheduled JOB modal%u opening",profile);
        if (send(keys[direction])!=kWorldConversationHandled || !modal->plainDialogue() || modal->actorSlot()!=slot || !drain(false)) return false;
        report=Common::String::format("scheduled JOB modal%u opening payment",profile);
        // Actual source tavern opening: Y, then option1 costs one gold;
        // payment phase4 consumes one further ordinary key to resume queries.
        if (send(Common::KEYCODE_y,'Y')!=kWorldConversationHandled || !drain(false) || send(Common::KEYCODE_1,'1')!=kWorldConversationHandled || !drain(false) || game.gold!=9 || send(Common::KEYCODE_RETURN,13)!=kWorldConversationHandled) return false;
        report=Common::String::format("scheduled JOB modal%u actual JOB",profile);
        if (!word("JOB")) {report+=Common::String::format(" input rejected stage%u reply[%s]",modal->stage(),modal->visibleReply().c_str());return false;}
        if (modal->visibleReply()!=expectedJob[profile]) {report+=Common::String::format(" stage%u got[%s] expected[%s]",modal->stage(),modal->visibleReply().c_str(),expectedJob[profile].c_str());return false;}
        report=Common::String::format("scheduled modal%u JOB codec save",profile);
        Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!modal->save(saved)) return false;Common::MemoryReadStream savedBytes(saved.getData(),saved.size());Common::ScopedPtr<WorldConversation> restored(new WorldConversation);report=Common::String::format("scheduled modal%u JOB codec load",profile);if (!restored->load(savedBytes,archive)) return false;*modal=*restored;
        report=Common::String::format("scheduled modal%u post-codec JOB drain",profile);if (!drain(false)) return false;
        report=Common::String::format("scheduled modal%u NAME",profile);if (!word("NAME") || !drain(false)) return false;
        report=Common::String::format("scheduled modal%u BYE",profile);if (!word("BYE")) return false;
        report=Common::String::format("scheduled modal%u closing",profile);if (!drain(true)) return false;
        report=Common::String::format("scheduled modal%u field174 before=%u after=%u",profile,initialField,game.world.field(174));if (game.world.field(174)!=initialField) return false;
        ++modals;
    }
    report=Common::String::format("dialogue-boundary\tauthored_JOB_repairs=%u\tcomplete_teacher_headers=%u\tscheduled_modal_JOB_NAME_close=%u\tfield174_preserved=1\tmodal_codecs=2\tcampaign=0\taudio_suite=0\n",repairs,teachers,modals);return true;
}
}
#endif
