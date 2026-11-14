/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_CALLBACKS_VALIDATION_H
#define NOX_NATIVE_CALLBACKS_VALIDATION_H
#include "nox/native_world_conversation.h"
#include "nox/native_world_scene_phase.h"
#include "nox/native_world_travel.h"
namespace Nox {
inline bool validateNativeCallbacks479(HdvArchive &archive,Common::SeekableReadStream &fixture,Common::String &report) {
    using namespace Native;
    report="original callback transactions";
    if (fixture.readUint32BE()!=MKTAG('N','C','4','9') || fixture.readUint32LE()!=1) return false;
    const uint cases=fixture.readUint32LE();if (cases!=512) return false;
    Common::Array<byte> authored;if (!archive.readRange("NA/SRTN.NPC.TALK",0xfc6,12,authored)) return false;
    for (uint i=0;i<cases;++i) {
        const byte map=fixture.readByte();byte fields[256],objects[256],expectedFields[256],expectedObjects[256];
        if (fixture.read(fields,256)!=256 || fixture.read(objects,256)!=256 || fixture.read(expectedFields,256)!=256 || fixture.read(expectedObjects,256)!=256) return false;
        const bool requested=ConversationCallbacks::requested(map,fields);bool redraw=false;
        if (!ConversationCallbacks::apply(map,fields,objects,authored.data(),redraw) || redraw!=requested || memcmp(fields,expectedFields,256) || memcmp(objects,expectedObjects,256)) return false;
    }
    if (fixture.err() || fixture.pos()!=fixture.size()) return false;
    report="authored callback pending and resolved ownership";
    struct Case {byte type,map,actor;const char *resource,*query;};
    const Case sources[]={{5,48,112,"NA/DATA.TLK.L011","RED HAND"},{8,68,0,"NA/DATA.TLK.L018","NO"},{8,67,24,"NA/DATA.TLK.L018",""},{6,74,0,"NA/DATA.TLK.L01A","KAYLA"},{2,97,16,"NA/DATA.TLK.L061","DAD"},{2,97,16,"NA/DATA.TLK.L061","FATHER"},{2,99,0,"NA/DATA.TLK.L063","VHA"},{2,99,0,"NA/DATA.TLK.L063","OBLIVION"},{2,104,8,"NA/DATA.TLK.L068","COMPANION"}};
    uint owners=0;
    for (uint i=0;i<ARRAYSIZE(sources);++i) {
        const Case &c=sources[i];report=Common::String::format("authored callback owner%u",i);
        byte fields[256]={};if (c.map==68) fields[58]=16;
        Common::ScopedPtr<World> owner(new World(archive));World &world=*owner;
        if (!world.initialize(c.type,c.map,24,24,fields,256)) return false;
        Common::ScopedPtr<Conversation> conversationOwner(new Conversation);Conversation &conversation=*conversationOwner;
        ConversationContext context;context.map=c.map;context.mapType=c.type;context.adultMode=true;context.hour=9;GameplayRandom random;if (!random.initializeEntropy(119)) return false;
        if (!conversation.begin(archive,c.resource,c.actor,fields,256,context)) return false;
        const ConversationResult response=*c.query?conversation.submit(c.query,&random):conversation.opening(&random);
        if (response!=kConversationReply || !conversation.worldCallbacksPending()) return false;
        Common::MemoryWriteStreamDynamic pending(DisposeAfterUse::YES),worldBefore(DisposeAfterUse::YES);if (!conversation.save(pending) || !world.save(worldBefore)) return false;
        Common::MemoryReadStream pendingInput(pending.getData(),pending.size());Common::ScopedPtr<Conversation> restoredOwner(new Conversation);Conversation &restored=*restoredOwner;
        if (!restored.load(pendingInput,archive) || !restored.worldCallbacksPending() || restored.submit("JOB",&random)!=kConversationInvalid) return false;
        byte expectedFields[256],expectedObjects[256];memcpy(expectedFields,restored.fields(),256);memcpy(expectedObjects,world.currentTables()->data()+1024,256);bool expectedRefresh=false;
        if (!ConversationCallbacks::apply(c.map,expectedFields,expectedObjects,authored.data(),expectedRefresh)) return false;
        bool refresh=false;if (!world.applyConversationCallbacks(restored,refresh) || !refresh || restored.worldCallbacksPending() || memcmp(restored.fields(),expectedFields,256) || memcmp(world.currentTables()->data()+1024,expectedObjects,256)) return false;
        Common::MemoryWriteStreamDynamic resolved(DisposeAfterUse::YES),changed(DisposeAfterUse::YES);if (!restored.save(resolved) || !world.save(changed)) return false;
        Common::MemoryReadStream resolvedInput(resolved.getData(),resolved.size());Common::ScopedPtr<Conversation> finalOwner(new Conversation);Conversation &final=*finalOwner;
        Common::MemoryReadStream changedInput(changed.getData(),changed.size());Common::ScopedPtr<World> reloadedOwner(new World(archive));World &reloaded=*reloadedOwner;
        if (!final.load(resolvedInput,archive) || final.worldCallbacksPending() || !reloaded.load(changedInput) || !reloaded.applyConversationCallbacks(final,refresh) || refresh) return false;
        Common::MemoryWriteStreamDynamic untouched(DisposeAfterUse::YES);if (!reloaded.save(untouched) || changed.size()!=untouched.size() || memcmp(changed.getData(),untouched.getData(),changed.size())) return false;
        ++owners;
    }
    report="literal dollar body";Common::Array<byte> literal;const char *body="Price $10";for (uint i=0;body[i];++i) literal.push_back(byte(body[i])|128);literal.push_back(0);Common::String rendered;
    if (!plainDialogueBody(literal,0,rendered) || rendered!=body) return false;
    report="actual Town74 response and world-conversation repaint continuation";
    WorldTravelPlan plan;plan.type=6;plan.map=74;plan.x=plan.y=24;
    Common::ScopedPtr<NewGame> gameOwner(new NewGame(archive));NewGame &game=*gameOwner;if (!game.create("CALLBACK",0,0) || !WorldTravel::commit(game.world,plan)) return false;
    NpcSchedulePolicy policy={true,true};if (NpcScheduler::apply(game.world,9,0,policy,game.schedule)==kNpcScheduleUnsupported) return false;
    const byte *npc=game.world.currentTables()->data()+1280;const byte px=npc[0],py=byte(npc[1]+1);
    plan.x=px;plan.y=py;if (!WorldTravel::commit(game.world,plan)) return false;
    WorldStory window;StoryMapPatches story;ActorPaintRules actors;TerrainLighting lighting;VisibilityMask visibility;WorldScene scene;PresentationState presentation;LightState light(1,0,0);HgrFrame frame;
    if (!window.initialize(game.world,game.world.currentTables()->data()+1536,256) || !story.load(archive) || !actors.load(archive,6) || !lighting.load(archive,6) || !visibility.loadRules(archive) || !WorldScenePhase::run(game.world,window,story,actors,lighting,0,246,0,presentation,light,visibility,scene) || scene.actors.lookup[76]!=0 || scene.actors.kinds[76]!=4) return false;
    Common::ScopedPtr<WorldConversation> uiOwner(new WorldConversation);WorldConversation &ui=*uiOwner;GameplayRandom random;if (!random.initializeEntropy(119) || !ui.choose(game.world,frame)) return false;
    auto send=[&](WorldConversation &target,Common::KeyCode key,byte ascii=0) {Common::Event e;e.type=Common::EVENT_KEYDOWN;e.kbd=Common::KeyState(key,ascii);byte message=255;return target.handle(e,archive,game,scene,frame,message,nullptr,&random);};
    if (send(ui,Common::KEYCODE_UP)==kWorldConversationUnsupported) return false;
    for (uint n=0;n<64 && ui.stage()==2;++n) if (send(ui,Common::KEYCODE_SPACE,' ')==kWorldConversationUnsupported) return false;
    if (ui.stage()!=3) return false;
    const char *query="KAYLA";for (uint i=0;query[i];++i) if (send(ui,Common::KEYCODE_a,byte(query[i]))==kWorldConversationUnsupported) return false;
    if (send(ui,Common::KEYCODE_RETURN)==kWorldConversationUnsupported || (*game.world.currentTables())[1027]!=36 || (*game.world.currentTables())[1095]!=36) return false;
    Common::MemoryWriteStreamDynamic savedUI(DisposeAfterUse::YES);if (!ui.save(savedUI) || savedUI.size()<6 || READ_LE_UINT16(savedUI.getData()+4)!=8) return false;
    Common::ScopedPtr<WorldConversation> continuedOwner(new WorldConversation);WorldConversation &continued=*continuedOwner;Common::MemoryReadStream savedUIInput(savedUI.getData(),savedUI.size());if (!continued.load(savedUIInput,archive)) return false;
    for (uint n=0;n<64 && continued.active();++n) if (send(continued,continued.stage()==3?Common::KEYCODE_RETURN:Common::KEYCODE_SPACE,continued.stage()==3?0:' ')==kWorldConversationUnsupported) return false;
    if (continued.active() || !continued.takeWorldRefresh() || continued.takeWorldRefresh() || (game.world.field(46)&128) || (*game.world.currentTables())[1027]!=36) return false;
    report=Common::String::format("callbacks\tsource_transactions=%u\tauthored_pending_resolved_owners=%u\tnormal_modal_codec=1\texit_refresh_once=1\tliteral_dollar=1\tcampaign=0\taudio_suite=0\n",cases,owners);return true;
}
}
#endif
