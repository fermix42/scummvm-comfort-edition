/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_COMBAT_VALIDATION_H
#define NOX_NATIVE_NPC_COMBAT_VALIDATION_H
#include "nox/native_cast.h"
#include "nox/native_battle_targets.h"
#include "nox/native_battle_npc_decision.h"
#include "nox/native_battle_records_validation.h"
namespace Nox {
inline bool validateNativeNpcCast(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','N','C') || input.readUint32LE()!=1) return false;
    const uint32 count=input.readUint32LE();if (!count || count>10000 || input.size()!=12+count*14) return false;
    Native::CastRules rules;if (!rules.load(archive)) return false;uint replenished=0;
    for (uint i=0;i<count;++i) {
        byte row[14];if (input.read(row,14)!=14) return false;byte mana=row[5];Native::NpcCastPreparation result;
        if (!rules.prepareNpc(row[0],row[1],row[2],row[6],row[3],row[4]&128,mana,result) ||
            result.cast.spell!=row[7] || result.cast.manaCost!=row[8] || result.cast.basePower!=row[9] ||
            result.powerColumn!=row[10] || result.mode!=row[11] || mana!=row[12] || result.replenished!=(row[13]!=0)) {
            report=Common::String::format("NPC cast preflight case%u differs spell%u/%u column%u/%u mana%u/%u",i,uint(result.cast.spell),uint(row[7]),uint(result.powerColumn),uint(row[10]),uint(mana),uint(row[12]));return false;
        }
        replenished+=result.replenished?1:0;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("native-npc-cast\tcases=%u\tselector_map_mode_power_mana=1\treplenished=%u\tfull_AI=0\tcpu_execution=0\n",count,replenished);return true;
}
inline bool validateNativeNpcDecision(Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','N','D') || input.readUint32LE()!=1) return false;
    const uint32 count=input.readUint32LE();if (!count || count>10000 || input.size()!=12+count*5887) return false;
    uint outcomes[3]={0,0,0};
    for (uint i=0;i<count;++i) {
        const byte caster=input.readByte();const uint32 seed=input.readUint32LE();const byte tail=input.readByte(),last=input.readByte();
        const uint16 attempts=input.readUint16LE();Native::BattleRecords records,expected;if (!readBattlePayload(input,records)) return false;
        const byte action=input.readByte(),target=input.readByte();const uint32 afterSeed=input.readUint32LE();
        const byte afterTail=input.readByte(),afterLast=input.readByte();const uint16 afterAttempts=input.readUint16LE();
        if (!readBattlePayload(input,expected) || action>2) return false;Native::GameplayRandom random;Native::NpcBattleDecision result;
        if (!random.importState(seed,tail,last,attempts) || !Native::BattleNpcDecision::choose(caster,random,records,result) ||
            byte(result.action)!=action || result.target!=target || !sameBattleRecords(records,expected) || random.seed()!=afterSeed ||
            random.tail()!=afterTail || random.last()!=afterLast || random.generationAttempts()!=afterAttempts) {
            report=Common::String::format("NPC decision case%u differs action%u/%u target%u/%u RNG%08x/%08x",i,uint(result.action),uint(action),uint(result.target),uint(target),random.seed(),afterSeed);return false;
        }
        ++outcomes[action];
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("native-npc-decision\tcases=%u\tnearest_alternate_engager_conditional_RNG=1\tpursuit=%u\tphysical=%u\tcast=%u\tfull_NPC_turn=0\tcpu_execution=0\n",count,outcomes[0],outcomes[1],outcomes[2]);return true;
}
inline bool validateNativeNpcTargets(Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','N','T') || input.readUint32LE()!=1) return false;
    const uint32 count=input.readUint32LE();if (!count || count>10000 || input.size()!=12+count*2937) return false;
    for (uint i=0;i<count;++i) {
        const byte caster=input.readByte(),target=input.readByte(),alternative=input.readByte();Native::BattleRecords records;
        if (!readBattlePayload(input,records)) return false;byte selected=255,other=255;
        if (!Native::BattleTargets::nearest(records,caster,255,false,false,selected,&other) || selected!=target || other!=alternative) {
            report=Common::String::format("NPC target case%u differs selected%u/%u alternate%u/%u",i,uint(selected),uint(target),uint(other),uint(alternative));return false;
        }
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("native-npc-targets\tcases=%u\tpool_affiliation_large_status_scan=1\tfull_AI=0\tcpu_execution=0\n",count);return true;
}
}
#endif
