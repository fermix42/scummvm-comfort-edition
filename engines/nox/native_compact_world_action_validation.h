/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_COMPACT_WORLD_ACTION_VALIDATION_H
#define NOX_NATIVE_COMPACT_WORLD_ACTION_VALIDATION_H
#include "nox/native_compact_world_action.h"
#include "common/savefile.h"
namespace Nox {
inline bool validateNativeCompactWorldAction(HdvArchive &archive,Common::SeekableReadStream &input,Common::SeekableReadStream &continuation,
        Common::SaveFileManager &files,bool cold,Common::String &report,Common::String &error) {
    if (input.readUint32BE()!=MKTAG('N','C','A','C') || input.readUint32LE()!=1) return false;
    const uint groups=input.readUint32LE(),operations=input.readUint32LE();if (groups!=3456 || operations!=50555) return false;
    if (continuation.readUint32BE()!=MKTAG('N','C','A','C') || continuation.readUint32LE()!=1) return false;
    const uint checkpointGroup=continuation.readUint32LE(),checkpointOperation=continuation.readUint32LE();
    if (checkpointGroup>=groups || checkpointOperation>=16 || continuation.pos()!=continuation.size() || continuation.err()) return false;
    const char *names[4]={"nox-native-compact-world.bin","nox-native-compact-state.bin","nox-native-compact-rng.bin","nox-native-compact-effects.bin"};
    Native::CompactWorldAction rules;uint total=0,battles=0,waits=0;
    for (uint group=0;group<(cold?checkpointGroup+1:groups);++group) {
        error=Common::String::format("Compact setup rejected group%u",group);
        byte setup[9],actor[8],objects[256],fields[256]={};input.read(setup,9);input.read(actor,8);input.read(objects,256);
        const uint32 seed=input.readUint32LE();const byte tail=input.readByte(),last=input.readByte();const uint16 attempts=input.readUint16LE(),count=input.readUint16LE();
        if (!count || count>16 || setup[6]>2 || !rules.load(archive,setup[0])) return false;
        if (cold && group<checkpointGroup) { if (!input.seek(count*292,SEEK_CUR)) return false;continue; }
        Native::World world(archive);Native::CompactWorldActionState state;Native::GameplayRandom random;Native::ActorMovementEffectState effects;effects.code=7;
        if (cold) {
            Common::InSaveFile *in[4]={};bool ok=true;for (uint i=0;i<4;++i) { in[i]=files.openForLoading(names[i]);ok=ok && in[i]!=nullptr; }
            ok=ok && world.load(*in[0]) && state.load(*in[1]) && random.load(*in[2]) && effects.load(*in[3]);for (uint i=0;i<4;++i) delete in[i];if (!ok) return false;
        } else {
            if (!world.initialize(setup[0],setup[1],setup[2],setup[3],fields,256) || !random.importState(seed,tail,last,attempts)) return false;
            state.freezeTimer=setup[7];state.enabled=setup[8];Common::MemoryWriteStreamDynamic out(DisposeAfterUse::YES);if (!world.save(out)) return false;
            Common::Array<byte> imported;imported.resize(out.size());memcpy(imported.data(),out.getData(),out.size());
            memset(imported.data()+271+768,0,256);memset(imported.data()+271+1280,0,256);memcpy(imported.data()+271+768+setup[4],actor,8);memcpy(imported.data()+271+1024,objects,256);
            if (setup[6]==1) { const byte other[8]={byte(setup[2]+1),setup[3],137,0,0,0,0,0};memcpy(imported.data()+271+1288,other,8); }
            Common::MemoryReadStream ws(imported.data(),imported.size());if (!world.load(ws)) return false;
        }
        for (uint operation=0;operation<count;++operation) {
            byte expected[292];if (input.read(expected,292)!=292) return false;
            if (cold && operation<checkpointOperation) continue;
            const bool execute=!cold || operation!=checkpointOperation;byte direction=4;Native::CompactWorldActionResult result=Native::kCompactWorldAdvanced;
            if (execute) {
                Native::WorldScene scene;memset(scene.actors.terrain,setup[5],187);
                const byte *record=world.currentTables()->data()+768+setup[4];const byte x=byte(record[0]-world.x()+128),y=byte(record[1]-world.y()+128);
                if (x<120 || x>=137 || y<123 || y>=134) return false;const uint cell=(y-123)*17+x-120;
                scene.actors.lookup[cell]=setup[4];if (setup[6]==1) scene.actors.lookup[94]=8;if (setup[6]==2) scene.objects.lookup[92]=32;
                result=rules.advance(world,setup[4],scene,state,random,effects,direction);
            }
            Common::MemoryReadStream actualRng(expected+279,8);const uint32 expectedSeed=actualRng.readUint32LE();const byte expectedTail=actualRng.readByte(),expectedLast=actualRng.readByte();const uint16 expectedAttempts=actualRng.readUint16LE();
            if ((execute && (uint(result)!=expected[0] || direction!=expected[1])) || state.hidden!=bool(expected[2]) || state.appearanceChanged!=bool(expected[3]) ||
                state.freezeTimer!=expected[4] || state.stalled!=bool(expected[5]) || state.pattern!=expected[6] || memcmp(world.currentTables()->data()+768+setup[4],expected+7,8) ||
                memcmp(world.currentTables()->data()+1024,expected+15,256) || state.relativeX!=expected[271] || state.relativeY!=expected[272] || state.movementPhase!=bool(expected[273]) || state.cell!=expected[274] ||
                memcmp(state.preferences,expected+275,4) || random.seed()!=expectedSeed || random.tail()!=expectedTail || random.last()!=expectedLast || random.generationAttempts()!=expectedAttempts ||
                effects.active!=expected[287] || effects.queued!=expected[288] || effects.code!=expected[289] || state.contactDirection!=expected[290] || state.contactCell!=expected[291]) {
                error=Common::String::format("Compact phase differs group%u operation%u cold%u result%u/%u direction%u/%u position%u,%u/%u,%u cell%u/%u RNG%08X/%08X",group,operation,uint(cold),uint(result),uint(expected[0]),uint(direction),uint(expected[1]),uint(state.relativeX),uint(state.relativeY),uint(expected[271]),uint(expected[272]),uint(state.cell),uint(expected[274]),random.seed(),expectedSeed);return false;
            }
            if (!cold && group==checkpointGroup && operation==checkpointOperation) {
                if (result!=Native::kCompactWorldAdvanced || direction>=4) return false;
                for (uint i=0;i<4;++i) { Common::OutSaveFile *out=files.openForSaving(names[i],false);if (!out) return false;
                    const bool ok=i==0?world.save(*out):i==1?state.save(*out):i==2?random.save(*out):effects.save(*out);out->finalize();const bool good=ok && !out->err();delete out;if (!good) return false; }
            }
            ++total;battles+=expected[0]==2;waits+=expected[0]==1;
        }
    }
    if (input.err() || (!cold && (input.pos()!=input.size() || total!=operations || battles!=578 || waits!=32391))) return false;
    report+=Common::String::format("native-compact-world\tphase=%s\toperations=%u\tbattle_handoffs=%u\tforced_waits=%u\tactual_move_restored=%u\tcpu_execution=0\twhole_turn=0\n",cold?"cold":"warm",total,battles,waits,uint(cold));return true;
}
}
#endif
