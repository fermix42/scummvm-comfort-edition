/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_MOTION_VALIDATION_H
#define NOX_NATIVE_BATTLE_MOTION_VALIDATION_H
#include "nox/native_battle_motion.h"
#include "nox/native_battle_records_validation.h"
namespace Nox {
inline bool validateNativeBattleMotion(Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','M','C') || input.readUint32LE()!=2 || input.readUint32LE()!=5 || input.readUint32LE()!=1 || input.size()!=16+6*6072) return false;
    Native::BattleRecords retained;Native::GameplayRandom random;
    for (uint i=0;i<6;++i) {
        const byte direction=input.readByte();const uint32 seed=input.readUint32LE();const byte tail=input.readByte(),last=input.readByte();const uint16 attempts=input.readUint16LE();
        Native::BattleRecords before,expected;byte terrain[187];
        if (!readBattlePayload(input,before) || input.read(terrain,187)!=187) return false;
        const uint32 afterSeed=input.readUint32LE();const byte afterTail=input.readByte(),afterLast=input.readByte();const uint16 afterAttempts=input.readUint16LE();
        if (!readBattlePayload(input,expected)) return false;
        if (!i || i==5) { retained=before;random=Native::GameplayRandom();if (!random.importState(seed,tail,last,attempts)) return false; }
        else if (!sameBattleRecords(retained,before) || random.seed()!=seed || random.tail()!=tail || random.last()!=last || random.generationAttempts()!=attempts) return false;
        bool moved=false;
        if (!Native::BattleMotion::advance(direction,terrain,187,random,retained,&moved) || moved!=(i<5)) {
            report=Common::String::format("Native distant tactical guard rejected original operation%u",i);return false;
        }
        if (!sameBattleRecords(retained,expected) ||
            random.seed()!=afterSeed || random.tail()!=afterTail || random.last()!=afterLast || random.generationAttempts()!=afterAttempts) {
            report=Common::String::format("Native linked distant tactical turn differs at operation%u",i);return false;
        }
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report="native-battle-motion\tlinked_turns=5\tplayer_moves=5\tenemy_moves=5\tcontrolled_refusals=1\tfull_record_bytes=17604\trng_continuity=1\tcontrolled_initialization=2\tcpu_execution=0\tdamage_turns=0\n";return true;
}
}
#endif
