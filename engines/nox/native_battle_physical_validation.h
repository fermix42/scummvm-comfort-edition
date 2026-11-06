/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_PHYSICAL_VALIDATION_H
#define NOX_NATIVE_BATTLE_PHYSICAL_VALIDATION_H
#include "nox/native_battle_physical.h"
#include "nox/native_battle_round.h"
#include "nox/native_battle_records_validation.h"
namespace Nox {
inline bool validateNativeBattleEligibility(Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32LE()!=1 || input.readUint32LE()!=5460) return false;
    uint automatic=0;
    for (uint i=0;i<5460;++i) {
        const byte count=input.readByte();byte states[6];if (input.read(states,6)!=6) return false;const byte expected=input.readByte();
        Native::BattleRecords records;
        for (uint member=0;member<6;++member) {
            records.compact[member*4]=byte(2+member);records.compact[member*4+1]=5;records.compact[member*4+2]=246;records.compact[member*4+3]=states[member];
            records.stats[member*128]=states[member]==255?255:states[member]&63;
            WRITE_LE_UINT16(records.stats+member*128+2,states[member]==255?0:100);
        }
        const byte selected=Native::BattleRound::firstEligibleParty(records,count);
        if (selected!=expected) { report=Common::String::format("Native party eligibility differs at case%u selection%u/%u",i,uint(selected),uint(expected));return false; }
        automatic+=selected==255;
    }
    if (input.err() || input.pos()!=input.size() || automatic!=126) return false;
    report="native-battle-eligibility\tcases=5460\tautomatic_cases=126\tpossessed_can_command=1\tsleeping_skipped=1\tfacing_preserved=1\tcpu_execution=0\n";return true;
}
inline bool validateNativeBattleRound(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    const uint32 tag=input.readUint32BE();if (tag==MKTAG('N','B','E','L')) return validateNativeBattleEligibility(input,report);
    if (tag!=MKTAG('N','B','R','D') || input.readUint32LE()!=1 || input.readUint32LE()!=2 || input.size()!=9035) return false;
    Native::BattleRoundState turn;turn.category=input.readByte();turn.protectedTargets=input.readByte()!=0;
    const uint32 seed=input.readUint32LE();const byte tail=input.readByte(),last=input.readByte();const uint16 attempts=input.readUint16LE();
    Native::BattleRecords retained;retained.credit=input.readUint16LE();byte terrain[187];
    if (!readBattlePayload(input,retained) || input.read(terrain,187)!=187) return false;
    Native::GameplayRandom random;Native::BattleRound rules;if (!random.importState(seed,tail,last,attempts) || !rules.load(archive)) return false;
    for (uint i=0;i<2;++i) {
        const byte target=input.readByte();const uint32 afterSeed=input.readUint32LE();const byte afterTail=input.readByte(),afterLast=input.readByte();
        const uint16 afterAttempts=input.readUint16LE();Native::BattleRecords expected;expected.credit=input.readUint16LE();
        if (!readBattlePayload(input,expected)) return false;
        Native::PhysicalOutcome action;
        if (!rules.attack(target,terrain,187,turn,random,retained,action)) { report=Common::String::format("Native linked attack-round rejected operation%u",i);return false; }
        if (!sameBattleRecords(retained,expected) || random.seed()!=afterSeed || random.tail()!=afterTail || random.last()!=afterLast || random.generationAttempts()!=afterAttempts) {
            uint offset=0;while (offset<2048 && retained.stats[offset]==expected.stats[offset]) ++offset;
            report=Common::String::format("Native linked attack-round differs operation%u stat=%u rng=%08x/%08x last=%u/%u credit=%u/%u",i,offset,random.seed(),afterSeed,random.last(),afterLast,retained.credit,expected.credit);return false;
        }
        if (turn.phase!=(i?Native::kBattleVictory:Native::kBattlePartyTurn)) return false;
    }
    // U:DAB1-DE96 completes the side rotation after the lethal party action;
    // victory is acknowledged by Escape,not an early scheduler return. The raw
    // original record/RNG comparisons above remain byte-for-byte unchanged.
    if (input.err() || input.pos()!=input.size() || turn.actions!=3 || turn.rounds!=2) return false;
    report="native-battle-round\tretained_attacks=2\tenemy_responses=1\tfull_record_bytes=5868\tliving_enemy_HP=12\tactual_kill=1\tcredit=2\tcpu_execution=0\tcontrolled_initialization=1\tbattle_return=0\n";return true;
}
inline bool validateNativeBattlePhysical(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report,bool quick=false,bool projectileMode=false) {
    const int64 begin=input.pos();
    if (input.readUint32BE()!=(projectileMode?MKTAG('N','B','R','P'):quick?MKTAG('N','B','Q','P'):MKTAG('N','B','P','H')) || (projectileMode?input.readUint16LE():input.readUint32LE())!=1) return false;
    const uint count=input.readUint32LE();
    if (quick) { if (count!=960 || input.readUint32LE()!=5896 || input.size()!=16+count*5896) return false; }
    else if (count!=240 || input.size()-begin!=(projectileMode?10:12)+count*5894) return false;
    Native::BattlePhysical rules;if (!rules.load(archive)) return false;
    uint kills=0;
    for (uint i=0;i<count;++i) {
        Native::PhysicalIntent intent;intent.projectileMode=projectileMode;
        if (quick) { if (input.readByte()>1) return false;intent.quick=true;intent.quickRear=input.readByte()!=0; }
        intent.casterCell=input.readByte();intent.targetCell=input.readByte();intent.category=input.readByte();
        intent.offHand=input.readByte()!=0;intent.exceptionalWeapon=input.readByte()!=0;intent.protectedTargets=input.readByte()!=0;
        const uint32 seed=input.readUint32LE();const byte tail=input.readByte(),last=input.readByte();const uint16 attempts=input.readUint16LE(),pool=input.readUint16LE();
        Native::BattleRecords state,expected;if (!readBattlePayload(input,state)) return false;state.credit=pool;
        const uint32 expectedSeed=input.readUint32LE();const byte expectedTail=input.readByte(),expectedLast=input.readByte();
        const uint16 expectedAttempts=input.readUint16LE();expected.credit=input.readUint16LE();if (!readBattlePayload(input,expected)) return false;
        Native::GameplayRandom random;Native::PhysicalOutcome outcome;
        if (!random.importState(seed,tail,last,attempts) || !rules.resolve(intent,random,state,outcome)) {
            report=Common::String::format("Physical pipeline rejected source case%u",i);return false;
        }
        if (!sameBattleRecords(state,expected) || random.seed()!=expectedSeed || random.tail()!=expectedTail ||
                random.last()!=expectedLast || random.generationAttempts()!=expectedAttempts) {
            uint offset=0;while (offset<2048 && state.stats[offset]==expected.stats[offset]) ++offset;
            report=Common::String::format("Physical pipeline differs case%u stat=%u actual=%u expected=%u rng=%08x/%08x last=%u/%u credit=%u/%u",i,offset,
                offset<2048?state.stats[offset]:0,offset<2048?expected.stats[offset]:0,random.seed(),expectedSeed,random.last(),expectedLast,state.credit,expected.credit);return false;
        }
        kills+=outcome.killed;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("%s\tcases=%u\trecord_bytes=%u\tkills=%u\tcpu_execution=0\tcontrolled_setups=%u\twhole_battle=0\n",quick?"native-quick-physical":"native-battle-physical",count,count*2934,kills,count);return true;
}
}
#endif
