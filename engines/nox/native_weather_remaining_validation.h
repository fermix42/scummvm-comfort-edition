/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WEATHER_REMAINING_VALIDATION_H
#define NOX_NATIVE_WEATHER_REMAINING_VALIDATION_H
#include "nox/native_world_population_validation.h"
namespace Nox { namespace Native {
inline bool validateNativeWeatherRemaining518(HdvArchive &archive,Common::String &report) {
    const bool immune=preventsPartyDamage(true);
    uint hazards=0;
    for (uint part=0;part<4;++part) for (uint protectedParty=0;protectedParty<2;++protectedParty) {
        report=Common::String::format("population517 storm quadrant damage%u protection%u",part,protectedParty);
        Common::ScopedPtr<NightActorValidation504> v(new NightActorValidation504(archive));
        if (!populationCheckpoint517(*v,byte(128-(part&1)),byte(128-(part/2)),true,protectedParty?2:0)) return false;
        Common::String conditions;for (uint i=0;i<v->game.party.memberCount();++i) conditions+='0';
        if (!WorldCheckpointFixture::conditions(v->game,conditions,50)) return false;
        const uint16 health=v->game.party.member(0)->health();WorldHazards hazard;bool defeated=false;SoundEvents sounds;
        if (!hazard.load(archive,0) || !hazard.apply(v->game.world,v->scene,v->game.party,v->messages,v->text,v->frame,defeated,false,&sounds) || defeated!=(!protectedParty && !immune)) return false;
        if (v->game.party.member(0)->health()!=((protectedParty || immune)?health:0) ||
                (protectedParty?!sounds.empty():(sounds.size()!=1 || sounds[0]!=kSpeakerMagic))) return false;++hazards;
    }
    report="population517 storm defeat";
    Common::ScopedPtr<NightActorValidation504> defeatedOwner(new NightActorValidation504(archive));
    if (!populationCheckpoint517(*defeatedOwner,128,128)) return false;
    Common::String lowHealth;for (uint i=0;i<defeatedOwner->game.party.memberCount();++i) lowHealth+='0';
    if (!WorldCheckpointFixture::conditions(defeatedOwner->game,lowHealth,50)) return false;
    WorldHazards deadly;bool dead=false;
    if (!deadly.load(archive,0) || !deadly.apply(defeatedOwner->game.world,defeatedOwner->scene,defeatedOwner->game.party,defeatedOwner->messages,defeatedOwner->text,defeatedOwner->frame,dead) || dead==immune || defeatedOwner->game.party.member(0)->health()!=(immune?50:0)) return false;
    report="population517 ordinary arm/poll/codec/delay lifecycle";
    Common::ScopedPtr<NightActorValidation504> v(new NightActorValidation504(archive));if (!populationCheckpoint517(*v,126,127,false)) return false;
    WorldWait wait;WorldActorTurnEvent event;
    if (!wait.load(archive,0) || wait.run(v->game.world,v->window,v->scene,v->turn,v->game.clock,v->light,v->visible,v->game.schedule,v->routes,v->random,v->messages,v->text,v->frame,v->game.appearance,0,event,true,4,nullptr,&v->game.party)!=kWorldWaitAdvanced || !v->turn.population.pending || !populationCodec517(v->turn)) return false;
    bool changed=false;
    if (!v->input.pollWorldEvents(v->game,v->window,v->messages,v->text,v->frame,v->scene,v->light,0,changed,&archive,&v->turn,&v->random) || v->turn.population.pending || changed) return false;
    const uint32 seed=v->random.seed();
    if (!v->input.settlePopulation(archive,v->game,v->window,v->scene,v->light,v->turn,v->random) || v->random.seed()!=seed || v->send(Common::KEYCODE_z,'Z')!=kWorldInputHandled || v->turn.population.pending || v->random.seed()!=seed) return false;
    v->turn.population.arm(2);WorldActorTurnEvent battle;battle.battle=true;battle.bank=0;battle.slot=100;battle.direction=2;battle.cell=94;
    if (!v->input.queueEncounter(battle) || !v->input.settlePopulation(archive,v->game,v->window,v->scene,v->light,v->turn,v->random) || !v->turn.population.pending || v->random.seed()!=seed || !populationCodec517(v->turn) || !v->input.finishEncounter() || !v->input.settlePopulation(archive,v->game,v->window,v->scene,v->light,v->turn,v->random) || v->turn.population.pending) return false;
    report="population517 bounded all-visible replacement";
    Common::ScopedPtr<NightActorValidation504> full(new NightActorValidation504(archive));
    if (!populationCheckpoint517(*full,126,127,false)) return false;
    byte occupied[256]={},references[187];memset(references,255,187);
    for (uint slot=100;slot<=252;slot+=4) {occupied[slot]=occupied[slot+1]=1;occupied[slot+2]=164;occupied[slot+3]=1;references[(slot-100)/4]=byte(slot);}
    if (!populationRecords517(full->game.world,occupied,true)) return false;
    GameplayRandom bounded;WorldPopulation producer;WorldPopulationState pending;pending.pending=true;pending.action=0;pending.stormAttempts=16;bool spawned=false;
    if (!bounded.importState(2,2) || !producer.load(archive,0) || !producer.run(full->game.world,full->window,references,187,1,false,1,pending,bounded,spawned) || spawned || pending.pending || pending.cursor!=100 || bounded.last()!=2 || bounded.seed()==2 || memcmp(full->game.world.currentTables()->data()+768,occupied,256)) return false;
    report="population517 normal movement arming";
    Common::ScopedPtr<NightActorValidation504> mover(new NightActorValidation504(archive));
    if (!populationCheckpoint517(*mover,126,127,false) || mover->send(Common::KEYCODE_RIGHT)!=kWorldInputHandled || !mover->turn.population.pending || mover->turn.population.action!=2) return false;
    report="population517 shared travel clears origin projectile and retains globals";
    Common::ScopedPtr<NightActorValidation504> travel(new NightActorValidation504(archive));
    if (!tailSetup514(*travel,2,98,218,94,0,0,255,255,false,true)) return false;
    Common::MemoryWriteStreamDynamic data(DisposeAfterUse::YES);if (!travel->game.world.save(data) || data.size()!=2066) return false;
    Common::Array<byte> setup;setup.resize(data.size());memcpy(setup.data(),data.getData(),data.size());setup[11+56]|=64;setup[11+59]&=~2;
    Common::MemoryReadStream source(setup.data(),setup.size());if (!travel->game.world.load(source) || !travel->input.queueEncounter(battle)) return false;
    travel->turn.population.cursor=152;travel->turn.population.stormAttempts=7;travel->turn.population.monsterChoice=3;travel->turn.population.action=2;
    travel->turn.effects.queued=1;travel->turn.effects.code=5;
    if (!travel->input.continueTempleVictory(archive,travel->game,travel->window,travel->scene,travel->turn,travel->light,travel->visible,travel->frame,0) || travel->game.world.mapId()!=58 || travel->turn.effects.queued || !travel->turn.population.pending || travel->turn.population.cursor!=152 || travel->turn.population.stormAttempts!=7 || travel->turn.population.monsterChoice!=3 || travel->turn.population.action!=2 || !populationCodec517(travel->turn)) return false;
    report=Common::String::format("weather_remaining\tquadrant_damage_protection=8\tdefeat=%u\tarm_poll_delay=1\tall_visible_bound=1\tactual_movement_arm=1\tshared_travel_cleanup=1\told_passed_prefix_rerun=0\n",uint(!immune));return true;
}
} }
#endif
