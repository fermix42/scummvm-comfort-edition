/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_SUMMON_VALIDATION_H
#define NOX_NATIVE_BATTLE_SUMMON_VALIDATION_H
#include "nox/native_battle_summon.h"
#include "nox/native_battle_records_validation.h"
#include "nox/native_encounter_state.h"
#include "nox/native_summon_battle_validation.h"
namespace Nox {
inline bool validateNativeBattleSummon(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','S','U') || input.readUint32LE()!=1) return false;
    const uint32 count=input.readUint32LE(),row=input.readUint32LE();
    if (!count || count>10000 || row!=6330 || input.size()!=16+count*row) return false;
    Native::BattleSummon rules;if (!rules.load(archive)) return false;uint placed=0,full=0;
    for (uint32 i=0;i<count;++i) {
        const byte spell=input.readByte(),cost=input.readByte(),occupied=input.readByte();const uint32 seed=input.readUint32LE();
        const byte tail=input.readByte(),last=input.readByte();const uint16 attempts=input.readUint16LE();
        byte terrain[187],objects[187],blocking[4],health[32],expectedHealth[32];
        if (input.read(terrain,187)!=187 || input.read(objects,187)!=187 || input.read(blocking,4)!=4 || input.read(health,32)!=32) return false;
        Native::BattleRecords records,expected;if (!readBattlePayload(input,records)) return false;
        const uint32 afterSeed=input.readUint32LE();const byte afterTail=input.readByte(),afterLast=input.readByte();
        const uint16 afterAttempts=input.readUint16LE();const byte population=input.readByte();
        if (input.read(expectedHealth,32)!=32 || !readBattlePayload(input,expected)) return false;
        Native::GameplayRandom random;Native::SummonOutcome result;
        const byte casterCell=byte(records.compact[1]*17+records.compact[0]);
        if (!random.importState(seed,tail,last,attempts) || !rules.resolve(spell,cost,casterCell,terrain,187,objects,187,blocking,4,health,32,random,records,result)) {
            report=Common::String::format("summon case%u spell%u rejected",i,uint(spell));return false;
        }
        if (!sameBattleRecords(records,expected) || memcmp(health,expectedHealth,32) || uint(occupied)+result.placed!=population ||
            random.seed()!=afterSeed || random.tail()!=afterTail || random.last()!=afterLast || random.generationAttempts()!=afterAttempts) {
            uint stat=0;while (stat<2048 && records.stats[stat]==expected.stats[stat]) ++stat;
            uint actor=0;while (actor<256 && records.extended[actor]==expected.extended[actor]) ++actor;
            report=Common::String::format("summon case%u spell%u differs stat%u actor%u placed%u/%u rng%08x/%08x",i,uint(spell),stat,actor,result.placed,uint(population-occupied),random.seed(),afterSeed);return false;
        }
        Native::EncounterState pending;pending.formed=true;pending.records=records;
        pending.round.enemyCount=0;
        for (uint enemy=0;enemy<16;++enemy) if (records.extended[enemy*8+2]) pending.round.enemyCount=byte(enemy+1);
        pending.round.allyCount=16;pending.round.category=8;memcpy(pending.round.summonHealth,health,32);
        Common::MemoryWriteStreamDynamic packet(DisposeAfterUse::YES),again(DisposeAfterUse::YES);
        if (!pending.save(packet)) return false;Native::EncounterState restored;Common::MemoryReadStream saved(packet.getData(),packet.size());
        if (!restored.load(saved) || !sameBattleRecords(restored.records,records) || memcmp(restored.round.summonHealth,health,32) ||
            !restored.save(again) || packet.size()!=again.size() || memcmp(packet.getData(),again.getData(),packet.size())) {
            report=Common::String::format("summon snapshot case%u rejected/changed records,health or occupancy",i);return false;
        }
        placed+=result.placed;full+=occupied==16?1:0;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("native-battle-summon\tcases=%u\tspell_slots=5\tfull_family_MP_templates_placement=1\tfull_records_health_RNG=1\tsnapshot_roundtrips=%u\tplaced=%u\tfull_pool=%u\tordinary_cast_UI=0\twhole_battle=0\tcpu_execution=0\n",count,count,placed,full);
    const byte spells[]={14,16,17,18,19,20};for (uint i=0;i<6;++i) {
        Common::String linked;if (!validateNativeSummonBattle(archive,spells[i],linked)) { report=Common::String::format("summon spell%u integration: %s",uint(spells[i]),linked.c_str());return false; }
        report+=linked;
    }
    return true;
}
}
#endif
