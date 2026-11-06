/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_ACTOR_TURN_VALIDATION_H
#define NOX_NATIVE_WORLD_ACTOR_TURN_VALIDATION_H
#include "nox/native_world_actor_turn.h"
#include "common/savefile.h"
namespace Nox {
inline bool validateNativeWorldActorTurn(HdvArchive &archive,Common::SeekableReadStream &input,Common::SeekableReadStream &continuation,
        Common::SaveFileManager &files,bool cold,Common::String &report,Common::String &error) {
    if (input.readUint32BE()!=MKTAG('N','A','W','T') || input.readUint32LE()!=1) return false;
    const uint groups=input.readUint32LE(),expectedTotal=input.readUint32LE();if (groups!=64 || expectedTotal>1024) return false;
    if (continuation.readUint32BE()!=MKTAG('N','A','W','C') || continuation.readUint32LE()!=1) return false;
    const uint checkpointGroup=continuation.readUint32LE(),checkpointOperation=continuation.readUint32LE();if (checkpointGroup>=groups || checkpointOperation>=16) return false;
    Native::WorldActorTurn rules;Native::FixedMessageCatalog catalog;if (!rules.load(archive,8) || !catalog.load(archive)) return false;
    const char *names[7]={"nox-native-turn-world.bin","nox-native-turn-state.bin","nox-native-turn-schedule.bin","nox-native-turn-store.bin","nox-native-turn-rng.bin","nox-native-turn-text.bin","nox-native-turn-terrain.bin"};
    uint total=0,battles=0;
    for (uint group=0;group<(cold?checkpointGroup+1:groups);++group) {
        Native::WorldScene prior;
        byte setup[5],tables[1792],fields[256],visibility[187],initial[7];input.read(setup,5);input.read(tables,1792);input.read(fields,256);input.read(prior.actors.terrain,187);input.read(visibility,187);
        input.read(prior.objects.lookup,187);input.read(prior.actors.lookup,187);input.read(prior.actors.kinds,187);
        const uint32 seed=input.readUint32LE();const byte tail=input.readByte(),last=input.readByte();const uint16 attempts=input.readUint16LE();input.read(initial,7);const uint count=input.readUint16LE();if (!count || count>16) return false;
        Native::World world(archive);Native::WorldActorTurnState state;Native::NpcScheduleState schedule;Native::NpcRouteStore store;Native::GameplayRandom random;Native::TextGlyphSurface surface;
        const bool selected=!cold || group==checkpointGroup;
        if (selected) {
            if (cold) {
                Common::InSaveFile *in[7]={};bool ok=true;for (uint i=0;i<7;++i) { in[i]=files.openForLoading(names[i]);ok=ok && in[i]!=nullptr; }
                ok=ok && world.load(*in[0]) && state.load(*in[1]) && schedule.load(*in[2]) && store.load(*in[3]) && random.load(*in[4]) && surface.load(*in[5],archive) &&
                    in[6]->read(prior.actors.terrain,187)==187 && in[6]->read(prior.objects.lookup,187)==187 && in[6]->read(prior.actors.lookup,187)==187 && in[6]->read(prior.actors.kinds,187)==187;
                for (uint i=0;i<7;++i) delete in[i];if (!ok) return false;
            } else {
                if (!world.initialize(8,7,24,38,fields,256) || !schedule.initialize(archive) || !random.importState(seed,tail,last,attempts) || !surface.initialize(archive,1)) return false;
                Common::MemoryWriteStreamDynamic out(DisposeAfterUse::YES);if (!world.save(out)) return false;Common::Array<byte> imported;imported.resize(out.size());memcpy(imported.data(),out.getData(),out.size());memcpy(imported.data()+271,tables,1792);
                Common::MemoryReadStream ws(imported.data(),imported.size());if (!world.load(ws)) return false;
                byte pages[3584]={};if (!store.importPages(pages,3584)) return false;
                state.presentation.facing=initial[0];state.presentation.hidden=initial[1]!=0;state.compact.freezeTimer=initial[2];state.compact.enabled=initial[3];
                state.effects.active=initial[4];state.effects.queued=initial[5];state.effects.code=initial[6];
            }
        }
        for (uint operation=0;operation<count;++operation) {
            byte expected[2815];if (input.read(expected,2815)!=2815) return false;const uint drawCount=input.readUint16LE();if (drawCount>1000) return false;
            Common::Array<byte> draws;draws.resize(drawCount*3);if (input.read(draws.data(),draws.size())!=draws.size()) return false;
            if (!selected || (cold && operation<checkpointOperation)) continue;
            const bool execute=!cold || operation!=checkpointOperation;Native::WorldScene scene;Native::WorldActorTurnEvent event;
            if (execute && !rules.run(world,prior,visibility,187,state,schedule,store,random,catalog,surface,scene,event)) {
                error=Common::String::format("Ordered actor turn rejected group%u operation%u",group,operation);return false;
            }
            Common::MemoryReadStream rng(expected+2797,8);const uint32 afterSeed=rng.readUint32LE();const byte afterTail=rng.readByte(),afterLast=rng.readByte();const uint16 afterAttempts=rng.readUint16LE();
            uint field=0;const byte *owned=world.currentTables()->data();
            if (memcmp(owned,expected+1,1792)) field=1;
            else if (memcmp(world.questFields(),expected+1793,256)) field=2;
            else if (execute && memcmp(scene.actors.terrain,expected+2049,187)) field=3;
            else if (execute && memcmp(scene.objects.lookup,expected+2236,187)) field=4;
            else if (execute && memcmp(scene.actors.lookup,expected+2423,187)) field=5;
            else if (execute && memcmp(scene.actors.kinds,expected+2610,187)) field=6;
            else if (random.seed()!=afterSeed || random.tail()!=afterTail || random.last()!=afterLast || random.generationAttempts()!=afterAttempts) field=7;
            else if (state.presentation.facing!=expected[2805] || state.presentation.hidden!=bool(expected[2806]) || state.compact.freezeTimer!=expected[2807] || state.compact.enabled!=expected[2808]) field=8;
            else if (state.effects.active!=expected[2809] || state.effects.queued!=expected[2810] || state.effects.code!=expected[2811]) field=9;
            else if (execute && (event.battle!=bool(expected[0]) || event.slot!=expected[2812] || event.direction!=expected[2813] || event.cell!=expected[2814])) field=10;
            else if (execute && scene.draws.size()!=drawCount) field=11;
            if (!field && execute) for (uint i=0;i<drawCount;++i) if (scene.draws[i].cell!=draws[i*3] || scene.draws[i].tile!=draws[i*3+1] || scene.draws[i].height!=draws[i*3+2]) { field=12;break; }
            if (field) { uint at=0;if (field==1) while (at<1792 && owned[at]==expected[1+at]) ++at;
                error=Common::String::format("Ordered actor turn differs group%u operation%u field%u offset%u got%u expected%u draws%u/%u facing%u/%u",group,operation,field,at,uint(owned[at]),uint(expected[1+at]),scene.draws.size(),drawCount,uint(state.presentation.facing),uint(expected[2805]));return false; }
            if (execute) prior=scene;
            if (!cold && group==checkpointGroup && operation==checkpointOperation) {
                for (uint i=0;i<7;++i) { Common::OutSaveFile *out=files.openForSaving(names[i],false);if (!out) return false;
                    const bool ok=i==0?world.save(*out):i==1?state.save(*out):i==2?schedule.save(*out):i==3?store.save(*out):i==4?random.save(*out):i==5?surface.save(*out):
                        out->write(prior.actors.terrain,187)==187 && out->write(prior.objects.lookup,187)==187 && out->write(prior.actors.lookup,187)==187 && out->write(prior.actors.kinds,187)==187;
                    out->finalize();const bool good=ok && !out->err();delete out;if (!good) return false; }
            }
            ++total;battles+=expected[0]!=0;
        }
    }
    if (input.err() || (!cold && (input.pos()!=input.size() || total!=expectedTotal))) return false;
    report+=Common::String::format("native-world-actor-turn\tphase=%s\toperations=%u\tbattle_handoffs=%u\tordered_occupancy=1\tactual_turn_restored=%u\tcpu_execution=0\tordinary_player_loop=0\n",cold?"cold":"warm",total,battles,uint(cold));return true;
}
}
#endif
