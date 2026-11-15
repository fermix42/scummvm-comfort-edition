/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_TAIL_VALIDATION_H
#define NOX_NATIVE_WORLD_TAIL_VALIDATION_H
#include "nox/native_encounter_clock_validation.h"
namespace Nox { namespace Native {
inline bool tailSetup514(NightActorValidation504 &v,byte type,byte map,byte x,byte y,byte ax,byte ay,byte actorSlot=64,byte noticeSlot=255,bool permission=false,bool water=false,byte preserveNpc=255) {
    if (!v.game.create("TAIL",0,0)) return false;
    v.game.clock.phase=1;v.game.clock.hour=0x12;v.game.clock.minute=0x34;v.game.clock.lightTicks=120;v.game.clock.lightActive=1;
    byte fields[256];memcpy(fields,v.game.world.questFields(),256);fields[31]=0;fields[60]=byte((fields[60]&~4)|(permission?4:0));fields[10]=byte((fields[10]&~1)|(permission?1:0));fields[57]=0;
    World initial(v.archive);if (!initial.initialize(type,map,x,y,fields,256)) return false;
    Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!initial.save(saved) || saved.size()!=269+1794+3) return false;
    Common::Array<byte> data;data.resize(saved.size());memcpy(data.data(),saved.getData(),saved.size());byte *tables=data.data()+271;
    // Controlled source-compatible pre-input record checkpoint. Retire all
    // incidental compact/NPC actors;place one legal compact record at its
    // declared slot. Original objects/terrain/entrances remain byte-identical.
    byte npcRecord[8]={};
    if (preserveNpc!=255) {if ((preserveNpc&7) || preserveNpc>248 || !tables[1282+preserveNpc] || tables[1283+preserveNpc]==255) return false;memcpy(npcRecord,tables+1280+preserveNpc,8);}
    memset(tables+768,0,256);memset(tables+1280,0,256);
    if (preserveNpc!=255) memcpy(tables+1280+preserveNpc,npcRecord,8);
    if (actorSlot!=255) {tables[768+actorSlot]=ax;tables[769+actorSlot]=ay;tables[770+actorSlot]=164;tables[771+actorSlot]=1;}
    if (noticeSlot!=255) {const byte *object=tables+1024+noticeSlot;if (object[3]!=55) return false;data[11+object[4]]=object[7]&128?object[5]:0;}
    Common::MemoryReadStream bytes(data.data(),data.size());if (!v.game.world.load(bytes)) return false;
    NpcSchedulePolicy policy={false,false};if (NpcScheduler::apply(v.game.world,v.game.clock.hour,v.game.clock.minute,policy,v.game.schedule)==kNpcScheduleUnsupported) return false;
    byte routes[3584]={},previous[187]={};v.light=LightState(1,0,0);
    if (!v.status.load(v.archive,type) || !v.status.initializeFrame(v.frame) || !v.story.load(v.archive) || !v.actors.load(v.archive,type) || !v.lighting.load(v.archive,type) || !v.occlusion.load(v.archive,type) || !v.painter.load(v.archive,type) || !v.visible.loadRules(v.archive) || !v.routes.importPages(routes,3584) || !v.random.initializeEntropy(119) || !v.messages.load(v.archive) || !v.text.initialize(v.archive,1) || !v.input.loadRules(v.archive,type) || !v.window.initialize(v.game.world,v.game.world.currentTables()->data()+1536,256) || !WorldViewport::refreshAtWorld(v.game.world,v.window,v.story,v.actors,v.lighting,v.occlusion,v.painter,v.game.appearance,v.game.clock.lightActive,0,previous,v.turn.presentation,v.light,v.visible,v.frame,&v.scene) || !v.text.importPages(v.frame.data(),v.frame.data(),8192)) return false;
    return water || WorldCheckpointFixture::standing(v.archive,v.game,v.scene);
}
inline bool tailCodecs514(NightActorValidation504 &v,bool notice=false) {
    Common::MemoryWriteStreamDynamic core(DisposeAfterUse::YES),window(DisposeAfterUse::YES),input(DisposeAfterUse::YES);
    if (!v.game.save(core) || !v.window.save(window) || !v.input.save(input) || !terrainTurnCodec511(v.turn)) return false;
    if (notice && READ_LE_UINT16(input.getData()+4)!=18) return false;
    Common::ScopedPtr<NewGame> game(new NewGame(v.archive));Common::ScopedPtr<WorldInput> command(new WorldInput);WorldStory restoredWindow;
    Common::MemoryReadStream a(core.getData(),core.size()),b(window.getData(),window.size()),c(input.getData(),input.size());
    if (!game->load(a) || !restoredWindow.load(b,game->world) || !command->load(c,v.archive,game->world.mapType()) || !command->matchesWorld(game->world) || !sameClock513(game->clock,v.game.clock) || command->noticeActive()!=v.input.noticeActive() || command->encounter().battle!=v.input.encounter().battle) return false;
    Common::MemoryWriteStreamDynamic again(DisposeAfterUse::YES);if (!command->save(again) || again.size()!=input.size() || memcmp(again.getData(),input.getData(),input.size())) return false;
    if (notice) {
        const byte *data=input.getData();const uint start=8+(data[7]?4:0);const uint size=READ_LE_UINT32(data+start);Common::MemoryReadStream legacy(data+start+4,size);Common::ScopedPtr<WorldInput> old(new WorldInput);
        if (!old->load(legacy,v.archive,v.game.world.mapType()) || !old->noticeActive() || old->encounter().battle) return false;
    }
    v.input=*command;return true;
}
inline bool validateNativeWorldTail514(HdvArchive &archive,Common::String &report) {
    const Common::KeyCode keys[]={Common::KEYCODE_UP,Common::KEYCODE_DOWN,Common::KEYCODE_RIGHT,Common::KEYCODE_LEFT};
    uint contacts=0,plates=0,codecs=0;
    for (uint stationary=0;stationary<2;++stationary) for (uint plate=0;plate<2;++plate) {
        report=Common::String::format("tail514 contact stationary%u plate%u setup",stationary,plate);
        const byte type=plate?3:8,map=plate?64:69,x=plate?11:17,y=byte(plate?(stationary?12:11):42),ay=byte(y+(stationary?1:2));
        Common::ScopedPtr<NightActorValidation504> owner(new NightActorValidation504(archive));NightActorValidation504 &v=*owner;
        if (!tailSetup514(v,type,map,x,y,x,ay)) return false;const TurnClock before=v.game.clock;ClockAdvance expected;if (!advanceTurnClock(before,expected)) return false;
        report=Common::String::format("tail514 actual contact stationary%u plate%u",stationary,plate);
        if (stationary) {
            WorldWait wait;WorldActorTurnEvent event;if (!wait.load(archive,type) || wait.run(v.game.world,v.window,v.scene,v.turn,v.game.clock,v.light,v.visible,v.game.schedule,v.routes,v.random,v.messages,v.text,v.frame,v.game.appearance,0,event,true,4,nullptr,&v.game.party)!=kWorldWaitBattle || !v.input.queueEncounter(event)) return false;
        } else if (v.send(Common::KEYCODE_DOWN)!=kWorldInputEncounter) return false;
        if (!v.input.encounter().battle || v.input.encounter().clockPending || !sameClock513(expected.after,v.game.clock) || !v.input.completeEncounterClock(v.game.world,v.game.clock) || !sameClock513(expected.after,v.game.clock) || (plate && !(v.game.world.field(31)&1)) || !tailCodecs514(v)) return false;
        ++contacts;++codecs;if (plate) ++plates;
    }
    {
        report="tail514 notice queued-contact setup";Common::ScopedPtr<NightActorValidation504> owner(new NightActorValidation504(archive));NightActorValidation504 &v=*owner;
        if (!tailSetup514(v,6,1,24,36,24,38,64,52)) return false;const TurnClock before=v.game.clock;ClockAdvance expected;if (!advanceTurnClock(before,expected)) return false;
        report="tail514 actual notice plus queued-contact arrow";
        const WorldInputResult opened=v.send(Common::KEYCODE_DOWN);
        if (opened!=kWorldInputHandled || !v.input.noticeActive() || v.input.encounter().battle || !sameClock513(before,v.game.clock) || !tailCodecs514(v,true)) return false;++codecs;
        report="tail514 actual notice closure then queued contact";WorldInputResult completed=kWorldInputHandled;uint keysSent=0;
        while (v.input.noticeActive() && keysSent++<32) {completed=v.send(Common::KEYCODE_g,'G');if (completed!=kWorldInputHandled && completed!=kWorldInputEncounter) return false;}
        if (completed!=kWorldInputEncounter || v.input.noticeActive() || !v.input.encounter().battle || v.input.encounter().clockPending || !sameClock513(expected.after,v.game.clock) || !tailCodecs514(v)) return false;++contacts;++codecs;
    }
    uint portals=0;
    for (uint permitted=0;permitted<2;++permitted) {
        report=Common::String::format("tail514 portal/contact prerequisite%u",permitted);Common::ScopedPtr<NightActorValidation504> owner;byte direction=255;
        const int dx[]={0,0,-1,1},dy[]={1,-1,0,0};
        for (uint i=0;i<4 && !owner;++i) {Common::ScopedPtr<NightActorValidation504> candidate(new NightActorValidation504(archive));if (tailSetup514(*candidate,7,54,byte(36+dx[i]),byte(11+dy[i]),byte(36-dx[i]),byte(11-dy[i]),64,255,permitted!=0) && candidate->scene.objects.lookup[93-dx[i]-17*dy[i]]==56) {direction=byte(i);owner.reset(candidate.release());}}
        if (!owner) return false;NightActorValidation504 &v=*owner;const TurnClock before=v.game.clock;ClockAdvance expected;if (!advanceTurnClock(before,expected)) return false;
        report=Common::String::format("tail514 actual portal/contact permission%u",permitted);const WorldInputResult result=v.send(keys[direction]);
        if (result!=(permitted?kWorldInputTransition:kWorldInputEncounter) || v.input.encounter().battle==bool(permitted) || !sameClock513(v.game.clock,permitted?before:expected.after) || v.game.world.mapId()!=(permitted?57:54) || (permitted && !v.input.atWorldCommand()) || !tailCodecs514(v)) return false;
        ++portals;++codecs;
    }
    {
        report="tail514 normal entrance/contact setup";Common::ScopedPtr<NightActorValidation504> owner(new NightActorValidation504(archive));NightActorValidation504 &v=*owner;
        if (!tailSetup514(v,5,47,9,15,9,17)) return false;const TurnClock before=v.game.clock;ClockAdvance expected;if (!advanceTurnClock(before,expected)) return false;
        report="tail514 actual entrance discards queued contact after clock";
        if (v.send(Common::KEYCODE_DOWN)!=kWorldInputTransition || v.game.world.mapId()!=46 || v.input.encounter().battle || !sameClock513(expected.after,v.game.clock) || !tailCodecs514(v)) return false;++codecs;
    }
    {
        report="tail514 water hazard and queued contact setup";Common::ScopedPtr<NightActorValidation504> owner(new NightActorValidation504(archive));NightActorValidation504 &v=*owner;
        if (!tailSetup514(v,2,98,218,95,218,96,164,255,false,true) || v.scene.actors.terrain[93]!=231) return false;
        Common::String conditions;for (uint i=0;i<v.game.party.memberCount();++i) conditions+='0';
        if (!WorldCheckpointFixture::conditions(v.game,conditions,10)) return false; // Pre-operation declared HP;source water damage17.
        const uint16 health=v.game.party.member(0)->health();const TurnClock before=v.game.clock;ClockAdvance expected;if (!advanceTurnClock(before,expected)) return false;
        WorldWait wait;WorldActorTurnEvent contact;if (!wait.load(archive,2)) return false;
        report="tail514 actual stationary contact applies water damage/defeat after clock";
        const WorldWaitResult result=wait.run(v.game.world,v.window,v.scene,v.turn,v.game.clock,v.light,v.visible,v.game.schedule,v.routes,v.random,v.messages,v.text,v.frame,v.game.appearance,0,contact,true,4,nullptr,&v.game.party);
        if (result!=kWorldWaitDefeat || !contact.battle || v.game.party.member(0)->health()>=health || !sameClock513(expected.after,v.game.clock)) {report=Common::String::format("tail514 water result%u contact%u terrain%u health%u->%u phase%u->%u",uint(result),uint(contact.battle),uint(v.scene.actors.terrain[93]),uint(health),uint(v.game.party.member(0)->health()),uint(before.phase),uint(v.game.clock.phase));return false;}
    }
    report=Common::String::format("world_tail\tactual_paid_contacts=%u\tactual_contact_plates=%u\tactual_notice_deferred_contact=1\tactual_portal_contact_policies=%u\tactual_entrance_contact_discard=1\tactual_water_contact_defeat=1\tcomponent_codec_sets=%u\tlegacy_notice_default=1\tbattle_execution=0\tcampaign=0\told_audio_animation_suite=0\n",contacts,plates,portals,codecs);return true;
}
} }
#endif
