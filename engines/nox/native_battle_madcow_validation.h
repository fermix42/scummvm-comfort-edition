/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_MADCOW_VALIDATION_H
#define NOX_NATIVE_BATTLE_MADCOW_VALIDATION_H
#include "nox/native_battle_summon.h"
#include "nox/native_battle_records_validation.h"
#include "nox/native_encounter_state.h"
namespace Nox {
inline bool validateNativeBattleMadCow(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','M','C')) return false;const uint32 version=input.readUint32LE();if (version!=1 && version!=2) return false;
    const uint32 count=input.readUint32LE();if (!count || count>10000 || input.size()!=12+count*(version==1?6334:6335)) return false;
    Native::BattleSummon rules;if (!rules.load(archive)) return false;uint exact=0,guarded=0,dismissed=0,refused=0,failed=0,protectedGuards=0;
    for (uint i=0;i<count;++i) {
        const byte protectedTargets=version==2?input.readByte():0;if (protectedTargets>1) return false;
        const byte sourceValid=input.readByte(),sourceRefused=input.readByte(),sourceDismissed=input.readByte(),sourceExhausted=input.readByte();
        const byte spell=input.readByte(),cost=input.readByte(),occupied=input.readByte();const uint32 seed=input.readUint32LE();
        const byte tail=input.readByte(),last=input.readByte();const uint16 attempts=input.readUint16LE();
        byte terrain[187],objects[187],blocking[4],health[32],expectedHealth[32];
        if (spell!=18 || sourceValid>1 || input.read(terrain,187)!=187 || input.read(objects,187)!=187 || input.read(blocking,4)!=4 || input.read(health,32)!=32) return false;
        Native::BattleRecords records,expected;if (!readBattlePayload(input,records)) return false;const Native::BattleRecords before=records;
        byte beforeHealth[32];memcpy(beforeHealth,health,32);
        const uint32 afterSeed=input.readUint32LE();const byte afterTail=input.readByte(),afterLast=input.readByte();
        const uint16 afterAttempts=input.readUint16LE();const byte population=input.readByte();
        if (input.read(expectedHealth,32)!=32 || !readBattlePayload(input,expected)) return false;
        Native::GameplayRandom random;Native::SummonOutcome result;
        if (!random.importState(seed,tail,last,attempts) || !rules.resolve(18,cost,141,terrain,187,objects,187,blocking,4,health,32,random,records,result,protectedTargets!=0)) {
            report=Common::String::format("Mad Cow case%u rejected",i);return false;
        }
        if (sourceValid) {
            if (sourceRefused || sourceExhausted || !sameBattleRecords(records,expected) || memcmp(health,expectedHealth,32) ||
                uint(occupied)+result.placed!=population || result.dismissed!=sourceDismissed || result.requested!=8 || result.allocated!=8 ||
                random.seed()!=afterSeed || random.tail()!=afterTail || random.last()!=afterLast || random.generationAttempts()!=afterAttempts) {
                uint stat=0;while (stat<2048 && records.stats[stat]==expected.stats[stat]) ++stat;
                uint actor=0;while (actor<256 && records.extended[actor]==expected.extended[actor]) ++actor;
                report=Common::String::format("Mad Cow source-defined case%u differs stat%u actor%u placed%u/%u dismissed%u/%u RNG%08x/%08x",i,stat,actor,result.placed,uint(population-occupied),result.dismissed,uint(sourceDismissed),random.seed(),afterSeed);return false;
            }
            ++exact;
        } else {
            // Original invalid outputs remain in the packet,not injected or
            // accepted as valid actor state. Check the explicit native fixes.
            if (!sourceRefused && !sourceExhausted && !protectedTargets) return false;
            if (sourceRefused) {
                Native::BattleRecords permitted=before;permitted.stats[4]-=cost;permitted.stats[12]=255;
                if (occupied!=16 || result.refused!=8 || result.allocated || result.placed || !sameBattleRecords(records,permitted) ||
                    memcmp(health,beforeHealth,32) || random.seed()!=seed || random.tail()!=tail || random.last()!=last || random.generationAttempts()!=attempts) {
                    report=Common::String::format("Mad Cow full-pool guard case%u overwrote actor/health/RNG or refunded mana",i);return false;
                }
            } else if (protectedTargets && !sourceExhausted) {
                if (result.dismissed || result.requested!=8 || result.allocated!=8 || result.placed!=8 || memcmp(records.extended,before.extended,128)) {
                    report=Common::String::format("Mad Cow protection case%u dismissed/overwrote a protected NPC",i);return false;
                }
                ++protectedGuards;
            } else if (!result.failedPlacement || result.allocated>8 || result.placed>=8) {
                report=Common::String::format("Mad Cow exhausted-search case%u forced an invalid placement",i);return false;
            }
            ++guarded;
        }
        if (records.stats[4]!=byte(before.stats[4]-cost) || READ_LE_UINT16(records.stats+2)!=READ_LE_UINT16(before.stats+2) || records.credit!=before.credit) return false;
        // Dismissal does not invent HP death/credit,including preexisting pools.
        if (memcmp(records.stats+768,before.stats+768,512)) return false;
        for (uint n=0;n<occupied;++n) if (memcmp(records.stats+1280+n*32,before.stats+1280+n*32,32)) return false;
        Native::BattleRecords rebuilt=records;
        if (!Native::BattleRoster::rebuild(rebuilt,1,2,16) || !sameBattleRecords(rebuilt,records)) { report=Common::String::format("Mad Cow case%u left invalid/overlapping ownership",i);return false; }
        Native::EncounterState pending;pending.formed=true;pending.records=records;pending.round.enemyCount=2;pending.round.allyCount=16;pending.round.category=8;memcpy(pending.round.summonHealth,health,32);
        Common::MemoryWriteStreamDynamic packet(DisposeAfterUse::YES);if (!pending.save(packet)) return false;Native::EncounterState cold;Common::MemoryReadStream saved(packet.getData(),packet.size());
        if (!cold.load(saved) || !sameBattleRecords(cold.records,records) || memcmp(cold.round.summonHealth,health,32)) return false;
        dismissed+=result.dismissed;refused+=result.refused;failed+=result.failedPlacement;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("native-battle-madcow\tcases=%u\toriginal_defined_exact=%u\toriginal_invalid_outputs_retained=%u\tnative_pool_search_guards=%u\tprotected_overlap_guards=%u\tdismissed=%u\trefused=%u\tfailed_placement=%u\tHP_preserved_no_XP_injection=1\tsnapshot_roundtrips=%u\tcpu_execution=0\n",count,exact,guarded,guarded,protectedGuards,dismissed,refused,failed,count);return true;
}
}
#endif
