/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_POPULATION_VALIDATION_H
#define NOX_NATIVE_WORLD_POPULATION_VALIDATION_H
#include "nox/native_world_tail_validation.h"
#include "nox/native_world_animation.h"
namespace Nox { namespace Native {
inline bool populationRecords517(World &world,const byte *compact,bool clearObjects=false,byte protection=0) {
    Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!world.save(saved) || saved.size()!=2066) return false;
    Common::Array<byte> data;data.resize(saved.size());memcpy(data.data(),saved.getData(),saved.size());
    memcpy(data.data()+271+768,compact,256);data[11+57]=protection;
    if (clearObjects) memset(data.data()+271+1024,0,512);
    Common::MemoryReadStream input(data.data(),data.size());return world.load(input);
}
inline bool populationWindow517(World &world,WorldStory &window,const byte *terrain,byte localX,byte localY) {
    WorldStory initial;if (!initial.initialize(world,world.currentTables()->data()+1536,256)) return false;
    Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!initial.save(saved) || saved.size()!=3083) return false;
    Common::Array<byte> data;data.resize(saved.size());memcpy(data.data(),saved.getData(),saved.size());
    data[9]=byte(world.x()-localX);data[10]=byte(world.y()-localY);memcpy(data.data()+11,terrain,2304);
    Common::MemoryReadStream input(data.data(),data.size());return window.load(input,world);
}
inline bool populationCodec517(const WorldActorTurnState &state,bool extended=true) {
    Common::MemoryWriteStreamDynamic a(DisposeAfterUse::YES),b(DisposeAfterUse::YES);if (!state.save(a)) return false;
    if ((READ_LE_UINT16(a.getData()+4)==6)!=extended) return false;
    Common::MemoryReadStream input(a.getData(),a.size());WorldActorTurnState next;
    return next.load(input) && next.save(b) && a.size()==b.size() && !memcmp(a.getData(),b.getData(),a.size());
}
inline bool populationCheckpoint517(NightActorValidation504 &v,byte x=126,byte y=127,bool actor=true,byte protection=0) {
    if (!tailSetup514(v,0,0,128,128,0,0,255,255,false,true)) return false;
    byte compact[256]={},terrain[2304];memset(terrain,117,sizeof(terrain));
    if (actor) {compact[100]=x;compact[101]=y;compact[102]=225;compact[103]=41;}
    if (!populationRecords517(v.game.world,compact,true,protection) || !populationWindow517(v.game.world,v.window,terrain,16,16)) return false;
    v.scene=WorldScene();memset(v.scene.actors.terrain,117,187);byte mask[187]={};if (!v.visible.importMask(mask,187)) return false;
    if (actor && WorldActorPhaseRules::visible(x,y)) {
        // Retained clipped footprint, declared before operations exactly as
        // in the original raster oracle. New movement uses the real painter.
        const byte kinds[]={64,32,16,8};
        for (uint part=0;part<4;++part) {
            const uint column=x-120+(part&1),row=y-123+(part/2);
            if (column>=17 || row>=11) continue;const uint cell=row*17+column;
            v.scene.actors.lookup[cell]=100;v.scene.actors.kinds[cell]=kinds[part];
        }
    }
    return true;
}
inline bool validateNativeWorldPopulation517(HdvArchive &archive,Common::SeekableReadStream &fixture,Common::SeekableReadStream &raster,Common::String &report) {
    report="population517 generator fixture header";
    if (fixture.readUint32BE()!=MKTAG('N','P','O','P') || fixture.readUint32LE()!=1) return false;
    const uint count=fixture.readUint32LE();if (!count || count>256) return false;uint spawns=0,codecs=0;
    for (uint index=0;index<count;++index) {
        report=Common::String::format("population517 original generator case%u",index);
        byte context[10],state[5],terrain[2304],records[256],visible[187],expectedState[5],expectedRecords[256];
        if (fixture.read(context,10)!=10 || fixture.read(state,5)!=5) return false;
        const uint32 seed=fixture.readUint32LE();const byte tail=fixture.readByte();
        if (fixture.read(terrain,2304)!=2304 || fixture.read(records,256)!=256 || fixture.read(visible,187)!=187 || fixture.read(expectedState,5)!=5) return false;
        const uint32 expectedSeed=fixture.readUint32LE();const byte expectedTail=fixture.readByte(),expectedLast=fixture.readByte();const uint expectedAttempts=fixture.readUint16LE();
        if (fixture.read(expectedRecords,256)!=256) return false;const bool expectedSpawn=fixture.readByte()!=0;
        byte fields[256]={};fields[64]=context[9];World world(archive);WorldStory window;WorldPopulation population;GameplayRandom random;WorldActorTurnState turn;
        turn.population.pending=state[0]!=0;turn.population.action=state[1];turn.population.cursor=state[2];turn.population.stormAttempts=state[3];turn.population.monsterChoice=state[4];
        if (!world.initialize(context[0],context[1],context[2],context[3],fields,256) || !populationRecords517(world,records) ||
                !populationWindow517(world,window,terrain,context[4],context[5]) || !population.load(archive,context[0]) || !random.importState(seed,tail)) return false;
        bool spawned=false;
        if (!population.run(world,window,visible,187,context[6],context[7]!=0,context[8],turn.population,random,spawned)) return false;
        const byte actualState[]={byte(turn.population.pending),turn.population.action,turn.population.cursor,turn.population.stormAttempts,turn.population.monsterChoice};
        if (memcmp(actualState,expectedState,5) || random.seed()!=expectedSeed || random.tail()!=expectedTail || random.last()!=expectedLast || random.generationAttempts()!=expectedAttempts ||
                memcmp(world.currentTables()->data()+768,expectedRecords,256) || spawned!=expectedSpawn) {
            report=Common::String::format("population517 generator%u mismatch state %u/%u/%u/%u/%u expected %u/%u/%u/%u/%u RNG %u/%u last%u/%u spawned%u/%u",index,
                uint(actualState[0]),uint(actualState[1]),uint(actualState[2]),uint(actualState[3]),uint(actualState[4]),uint(expectedState[0]),uint(expectedState[1]),uint(expectedState[2]),uint(expectedState[3]),uint(expectedState[4]),
                random.seed(),expectedSeed,uint(random.last()),uint(expectedLast),uint(spawned),uint(expectedSpawn));return false;
        }
        if (!populationCodec517(turn,turn.population.extended())) return false;++codecs;if (spawned) ++spawns;
    }
    if (fixture.err() || fixture.pos()!=fixture.size()) return false;
    report="population517 historical default turn codec";WorldActorTurnState old;
    if (!populationCodec517(old,false)) return false;++codecs;
    report="population517 original storm raster header";
    if (raster.readUint32BE()!=MKTAG('N','P','R','A') || raster.readUint32LE()!=1 || raster.readUint32LE()!=3) return false;
    uint frames=0,pixels=0;
    for (uint group=0;group<3;++group) {
        const byte x=raster.readByte(),y=raster.readByte(),cells=raster.readByte();
        Common::ScopedPtr<NightActorValidation504> v(new NightActorValidation504(archive));
        if (!populationCheckpoint517(*v,x,y)) return false;WorldAnimation animation;Common::ScopedPtr<WorldAnimationState> state(new WorldAnimationState);
        if (!animation.load(archive,0)) return false;WorldAnimationContext context;byte mask[187];memset(mask,1,187);
        for (uint cell=0;cell<187;++cell) if (v->scene.actors.lookup[cell]==100) mask[cell]=0;
        for (uint frame=0;frame<4;++frame) {
            report=Common::String::format("population517 original storm raster%u/%u",group,frame);
            const byte phase=raster.readByte();const byte target=state->target;Common::Array<WorldAnimationDraw> draws;
            if (!animation.run(v->game.world,v->scene,mask,187,v->light,context,*state,draws) || draws.size()!=cells) return false;
            for (uint i=0;i<cells;++i) {
                const byte cell=raster.readByte(),tile=raster.readByte();byte expected[32];if (raster.read(expected,32)!=32) return false;
                if (draws[i].cell!=cell || draws[i].tile!=tile || draws[i].phase!=phase) return false;
                for (uint row=0;row<16;++row) {
                    const uint offset=HgrFrame::rowOffset(8+(cell/17)*16+row)+(cell%17)*2;
                    if (memcmp(state->pages[target].data()+offset,expected+row*2,2)) return false;
                }
                ++pixels;
            }
            ++frames;
        }
    }
    if (raster.err() || raster.pos()!=raster.size()) return false;
    const byte xs[]={126,120,136,120,136,128},ys[]={127,123,123,133,133,128};uint movements=0;
    for (uint i=0;i<6;++i) {
        report=Common::String::format("population517 storm ordered actor phase%u",i);
        Common::ScopedPtr<NightActorValidation504> v(new NightActorValidation504(archive));if (!populationCheckpoint517(*v,xs[i],ys[i])) return false;
        WorldActorTurn phase;WorldScene after;WorldActorTurnEvent contact;
        if (!phase.load(archive,0) || !phase.run(v->game.world,v->scene,v->visible.cells(),187,v->turn,v->game.schedule,v->routes,v->random,v->messages,v->text,after,contact,4,&v->window) || contact.battle) return false;
        const byte *actor=v->game.world.currentTables()->data()+868;
        const byte nx=byte(xs[i]+1),ny=byte(ys[i]-1),oldCell=WorldActorPhaseRules::cell(xs[i],ys[i]);
        if (actor[0]!=nx || actor[1]!=ny || actor[2]!=225 || actor[3]!=41 || v->turn.presentation.retainedCell!=(ny>=123 && nx<137?byte(oldCell-16):oldCell)) return false;
        for (uint cell=0;cell<187;++cell) {
            const int col=int(cell%17)+120,row=int(cell/17)+123;
            const bool occupied=WorldActorPhaseRules::visible(nx,ny) && col>=nx && col<=int(nx)+1 && row>=ny && row<=int(ny)+1;
            if ((after.actors.lookup[cell]==100)!=occupied) return false;
        }
        ++movements;
    }
    uint gates=0;
    for (uint gate=0;gate<3;++gate) {
        report=Common::String::format("population517 storm wait/freeze/outside gate%u",gate);
        Common::ScopedPtr<NightActorValidation504> v(new NightActorValidation504(archive));
        if (!populationCheckpoint517(*v,gate==2?137:126,127)) return false;
        if (gate==0) v->turn.compact.freezeTimer=3;else if (gate==1) v->turn.compact.enabled=0;
        const uint32 seed=v->random.seed();WorldActorTurn phase;WorldScene after;WorldActorTurnEvent event;
        if (!phase.load(archive,0) || !phase.run(v->game.world,v->scene,v->visible.cells(),187,v->turn,v->game.schedule,v->routes,v->random,v->messages,v->text,after,event,4,&v->window)) return false;
        const byte *actor=v->game.world.currentTables()->data()+868;
        if (actor[0]!=(gate==2?137:126) || actor[1]!=127 || v->random.seed()!=seed || (gate==0 && v->turn.compact.freezeTimer!=2)) return false;++gates;
    }
    uint hazards=0;
    for (uint part=0;part<4;++part) for (uint protectedParty=0;protectedParty<2;++protectedParty) {
        report=Common::String::format("population517 storm quadrant damage%u protection%u",part,protectedParty);
        Common::ScopedPtr<NightActorValidation504> v(new NightActorValidation504(archive));
        if (!populationCheckpoint517(*v,byte(128-(part&1)),byte(128-(part/2)),true,protectedParty?2:0)) return false;
        Common::String conditions;for (uint i=0;i<v->game.party.memberCount();++i) conditions+='0';
        if (!WorldCheckpointFixture::conditions(v->game,conditions,50)) return false;
        const uint16 health=v->game.party.member(0)->health();WorldHazards hazard;bool defeated=false;SoundEvents sounds;
        if (!hazard.load(archive,0) || !hazard.apply(v->game.world,v->scene,v->game.party,v->messages,v->text,v->frame,defeated,false,&sounds) || defeated!=!protectedParty) return false;
        if (protectedParty?(v->game.party.member(0)->health()!=health || !sounds.empty()):(v->game.party.member(0)->health()!=0 || sounds.size()!=1 || sounds[0]!=kSpeakerMagic)) return false;++hazards;
    }
    report="population517 storm defeat";
    Common::ScopedPtr<NightActorValidation504> defeatedOwner(new NightActorValidation504(archive));
    if (!populationCheckpoint517(*defeatedOwner,128,128)) return false;
    Common::String lowHealth;for (uint i=0;i<defeatedOwner->game.party.memberCount();++i) lowHealth+='0';
    if (!WorldCheckpointFixture::conditions(defeatedOwner->game,lowHealth,50)) return false;
    WorldHazards deadly;bool dead=false;
    if (!deadly.load(archive,0) || !deadly.apply(defeatedOwner->game.world,defeatedOwner->scene,defeatedOwner->game.party,defeatedOwner->messages,defeatedOwner->text,defeatedOwner->frame,dead) || !dead || defeatedOwner->game.party.member(0)->health()) return false;
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
    report=Common::String::format("world_population\toriginal_generator_cases=%u\tspawned=%u\tturn_codecs=%u\toriginal_raster_frames=%u\traster_cells=%u\tordered_storm_moves=%u\tstorm_gates=%u\tdamage_protection_cases=%u\tarm_poll_delay_lifecycle=1\tactual_movement_arm=1\tshared_travel_cleanup=1\tcampaign=0\told_suites=0\n",count,spawns,codecs,frames,pixels,movements,gates,hazards);return true;
}
} }
#endif
