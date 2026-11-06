/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_SPELL_DAMAGE_VALIDATION_H
#define NOX_NATIVE_BATTLE_SPELL_DAMAGE_VALIDATION_H
#include "nox/native_battle_spell_damage.h"
#include "nox/native_battle_records_validation.h"
namespace Nox {
inline bool validateNativeBattleSpellDamage(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','S','D') || input.readUint32LE()!=1) return false;
    const uint32 count=input.readUint32LE();if (!count || count>10000 || input.size()!=12+count*5899) return false;
    Native::BattleSpellDamage rules;if (!rules.load(archive)) return false;
    uint kills=0,instantCount=0;
    for (uint32 i=0;i<count;++i) {
        Native::SpellDamageIntent intent;intent.classification=0; // This retained raw-impact oracle initializes the incoming classifier to0.
        intent.casterCell=input.readByte();intent.targetCell=input.readByte();intent.category=input.readByte();
        const byte instant=input.readByte(),protection=input.readByte();if (instant>1 || protection>1) return false;
        intent.instant=instant!=0;intent.protectedTargets=protection!=0;intent.basePower=input.readByte();intent.threshold=input.readByte();
        const uint32 seed=input.readUint32LE();const byte tail=input.readByte(),last=input.readByte();
        const uint16 attempts=input.readUint16LE(),pool=input.readUint16LE();Native::BattleRecords battle,expected;
        if (!readBattlePayload(input,battle)) return false;battle.credit=pool;
        const uint32 afterSeed=input.readUint32LE();const byte afterTail=input.readByte(),afterLast=input.readByte();
        const uint16 afterAttempts=input.readUint16LE();expected.credit=input.readUint16LE();const uint16 damage=input.readUint16LE();
        const byte killed=input.readByte(),classification=input.readByte();if (killed>1 || !readBattlePayload(input,expected)) return false;
        Native::GameplayRandom random;Native::SpellDamageOutcome outcome;
        if (!random.importState(seed,tail,last,attempts) || !rules.resolve(intent,random,battle,outcome)) {
            report=Common::String::format("spell impact case%u rejected",i);return false;
        }
        if (!sameBattleRecords(battle,expected) || outcome.damage!=damage || outcome.killed!=(killed!=0) || outcome.classification!=classification ||
            random.seed()!=afterSeed || random.tail()!=afterTail || random.last()!=afterLast || random.generationAttempts()!=afterAttempts) {
            uint stat=0;while (stat<2048 && battle.stats[stat]==expected.stats[stat]) ++stat;
            report=Common::String::format("spell impact case%u differs stat%u damage%u/%u killed%u/%u class%u/%u rng%08x/%08x credit%u/%u",i,stat,
                outcome.damage,damage,uint(outcome.killed),uint(killed),uint(outcome.classification),uint(classification),random.seed(),afterSeed,battle.credit,expected.credit);return false;
        }
        kills+=outcome.killed?1:0;instantCount+=intent.instant?1:0;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("native-battle-spell-damage\tcases=%u\tinstant_cases=%u\tkills=%u\tincoming_classification=0\tfull_records_RNG_credit=1\tarmor_resistance_large_protection=1\tcpu_execution=0\twhole_cast=0\n",count,instantCount,kills);return true;
}
}
#endif
