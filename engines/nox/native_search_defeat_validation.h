/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_SEARCH_DEFEAT_VALIDATION_H
#define NOX_NATIVE_SEARCH_DEFEAT_VALIDATION_H
#include "nox/native_world_input.h"
#include "nox/native_world_status.h"
#include "nox/native_battle_presentation.h"
namespace Nox { namespace Native {
inline bool validateNativeSearchDefeat495(HdvArchive &archive,Common::String &report) {
    uint deaths=0,nonfatal=0,codecs=0;
    for (uint profile=0;profile<3;++profile) {
        report=Common::String::format("Search defeat profile%u setup",profile);
        Common::ScopedPtr<NewGame> owner(new NewGame(archive));NewGame &game=*owner;if (!game.create("TRAP",0,0)) return false;
        // Controlled two-member checkpoint, as in retained lock-party38:
        // duplicate the source-created hero record/equipment before any input.
        Common::MemoryWriteStreamDynamic partySetup(DisposeAfterUse::YES);if (!game.party.save(partySetup) || partySetup.size()!=870) return false;
        Common::Array<byte> partyBytes;partyBytes.resize(870);memcpy(partyBytes.data(),partySetup.getData(),870);
        memcpy(partyBytes.data()+6+128,partyBytes.data()+6,128);memcpy(partyBytes.data()+774+16,partyBytes.data()+774,16);
        Party twoMembers;Common::MemoryReadStream partyInput(partyBytes.data(),partyBytes.size());if (!twoMembers.load(partyInput) || twoMembers.memberCount()!=2) return false;game.party=twoMembers;
        byte members[768];for (uint i=0;i<6;++i) memcpy(members+i*128,game.party.member(i)->bytes,128);
        for (uint i=0;i<game.party.memberCount();++i) {
            byte *member=members+i*128;member[31]=member[37]=0;
            if (profile==1 && i) {member[0]=255;WRITE_LE_UINT16(member+2,0);}
            else if (profile<2 || !i) {member[0]=0;WRITE_LE_UINT16(member+2,1);}
            else if (READ_LE_UINT16(member+2)<=41) return false;
        }
        if (!game.party.applyBattleMembers(members,768,game.party.memberCount())) return false;
        byte fields[256];memcpy(fields,game.world.questFields(),256);fields[33]=10;
        Common::ScopedPtr<WorldInput> command(new WorldInput);WorldStory window;WorldScene scene;WorldActorTurnState turn;WorldStatus status;StoryMapPatches story;ActorPaintRules actors;TerrainLighting lighting;OcclusionRules occlusion;TilePainter painter;TraversalRules traversal;
        LightState light(1,0,0);VisibilityMask visible;NpcRouteStore routes;GameplayRandom random;FixedMessageCatalog messages;TextGlyphSurface text;HgrFrame frame;byte previous[187]={},emptyRoutes[3584]={};
        if (!status.load(archive,3) || !status.initializeFrame(frame) || !story.load(archive) || !actors.load(archive,3) || !lighting.load(archive,3) || !occlusion.load(archive,3) || !painter.load(archive,3) || !traversal.load(archive,3) || !visible.loadRules(archive) || !routes.importPages(emptyRoutes,3584) || !messages.load(archive) || !text.initialize(archive,1) || !command->loadRules(archive,3)) return false;
        const int dx[]={0,0,-1,1},dy[]={1,-1,0,0};const byte near[]={76,110,94,92};byte direction=255;
        for (uint candidate=0;candidate<4;++candidate) {
            World setup(archive);if (!setup.initialize(3,31,byte(33+dx[candidate]),byte(14+dy[candidate]),fields,256)) return false;
            byte ground;if (!setup.terrainAt(setup.x(),setup.y(),ground)) return false;TraversalProbe probe;probe.terrain=ground;TraversalDecision decision;
            if (!traversal.probe(probe,decision) || decision.blocked || decision.requiresInteraction) continue;
            Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES);if (!setup.save(before)) return false;Common::MemoryReadStream bytes(before.getData(),before.size());if (!game.world.load(bytes)) return false;
            window=WorldStory();if (!window.initialize(game.world,game.world.currentTables()->data()+1536,256) || !WorldViewport::refreshAtWorld(game.world,window,story,actors,lighting,occlusion,painter,game.appearance,0,0,previous,turn.presentation,light,visible,frame,&scene)) return false;
            if (scene.objects.lookup[near[candidate]]==28) {direction=byte(candidate);break;}
        }
        if (direction==255 || !status.paint(game,scene,light,false,frame) || !text.importPages(frame.data(),frame.data(),8192)) return false;
        const byte *object=game.world.currentTables()->data()+1052;if (object[2]!=8 || object[3]!=53 || (object[7]&63)!=8) return false;
        const uint32 seed=profile?0x89e4ac58U:0xbd0fd87cU;if (!random.importState(seed,2)) return false;
        // Independent retained original37 seed contract: difficulty8,skills0,
        // map!=7 yields41damage to all members or only member1.
        GameplayRandom expected=random;LockRules locks;LockState lockState={10,0};Common::Array<LockEffect> effects;
        const LockContext context={8,0,0,31,1,byte(game.party.memberCount()),false,false};
        if (!locks.load(archive) || locks.attempt(context,lockState,expected,effects)!=kLockTrapped || effects.size()!=(profile?1:game.party.memberCount())) return false;
        for (uint effect=0;effect<effects.size();++effect) if (effects[effect].kind!=LockEffect::kDamage || effects[effect].amount!=41) return false;
        const HgrFrame worldFrame=frame;const TurnClock clock=game.clock;Common::MemoryWriteStreamDynamic priorTurn(DisposeAfterUse::YES);if (!turn.save(priorTurn)) return false;
        auto send=[&](Common::KeyCode key,byte ascii=0) {Common::Event event;event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(key,ascii);return command->handle(event,archive,game,window,scene,turn,light,visible,routes,random,messages,text,frame);};
        const Common::KeyCode arrows[]={Common::KEYCODE_UP,Common::KEYCODE_DOWN,Common::KEYCODE_RIGHT,Common::KEYCODE_LEFT};
        report=Common::String::format("Search defeat profile%u admitted trap",profile);
        if (send(Common::KEYCODE_s,'s')!=kWorldInputHandled || send(arrows[direction])!=kWorldInputHandled) return false;
        if (profile==1) {
            Common::MemoryWriteStreamDynamic beforeDead(DisposeAfterUse::YES),afterDead(DisposeAfterUse::YES);
            if (!game.save(beforeDead) || send(Common::KEYCODE_2,'2')!=kWorldInputHandled || !game.save(afterDead) ||
                beforeDead.size()!=afterDead.size() || memcmp(beforeDead.getData(),afterDead.getData(),beforeDead.size()) || random.seed()!=seed) return false;
        }
        const WorldInputResult result=send(Common::KEYCODE_1,'1');
        if (result!=(profile==2?kWorldInputHandled:kWorldInputDefeat) || command->worldDefeated()!=(profile!=2) || command->atWorldCommand() || game.world.field(33)!=9 || game.party.member(0)->bytes[0]!=255 || game.party.member(0)->health()) return false;
        if (profile==0) for (uint i=0;i<game.party.memberCount();++i) if (game.party.member(i)->bytes[0]!=255 || game.party.member(i)->health()) return false;
        Common::MemoryWriteStreamDynamic afterTurn(DisposeAfterUse::YES),actualRandom(DisposeAfterUse::YES),expectedRandom(DisposeAfterUse::YES);
        if (!turn.save(afterTurn) || priorTurn.size()!=afterTurn.size() || memcmp(priorTurn.getData(),afterTurn.getData(),priorTurn.size()) || !random.save(actualRandom) || !expected.save(expectedRandom) || actualRandom.size()!=expectedRandom.size() || memcmp(actualRandom.getData(),expectedRandom.getData(),actualRandom.size()) || clock.phase!=game.clock.phase || clock.hour!=game.clock.hour || clock.minute!=game.clock.minute) return false;
        Common::MemoryWriteStreamDynamic core(DisposeAfterUse::YES),input(DisposeAfterUse::YES);if (!game.save(core) || !command->save(input)) return false;
        Common::ScopedPtr<NewGame> loaded(new NewGame(archive));Common::MemoryReadStream coreBytes(core.getData(),core.size());if (!loaded->load(coreBytes)) return false;
        Common::ScopedPtr<WorldInput> restored(new WorldInput);Common::MemoryReadStream inputBytes(input.getData(),input.size());if (!restored->load(inputBytes,archive,3) || restored->worldDefeated()!=(profile!=2) || !restored->matchesWorld(loaded->world)) return false;
        codecs+=2;
        if (profile==2) {if (!restored->noticeActive() || loaded->party.member(1)->bytes[0]==255) return false;++nonfatal;}
        else {
            if (restored->noticeActive() || memcmp(frame.data(),worldFrame.data(),8192)) return false;
            BattlePresentation presentation;if (!presentation.load(archive,3) || !presentation.paintWorldDefeat(frame)) return false;
            Common::Event event;event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(Common::KEYCODE_DOWN,0);
            if (restored->handle(event,archive,*loaded,window,scene,turn,light,visible,routes,random,messages,text,frame)!=kWorldInputDefeat || random.seed()!=expected.seed() || loaded->clock.phase!=clock.phase) return false;++deaths;
        }
    }
    report=Common::String::format("search_defeat\tactual_traps=3\tdead_selection_refused=1\tterminal_deaths=%u\tnonfatal_retry=%u\tcore_input_codecs=%u\tterminal_extra_turns=0\tcampaign=0\taudio_suite=0\n",deaths,nonfatal,codecs);return true;
}
} }
#endif
