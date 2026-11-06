/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_RECORDS_VALIDATION_H
#define NOX_NATIVE_BATTLE_RECORDS_VALIDATION_H
#include "nox/native_vortex_cast_validation.h"
namespace Nox {
inline bool readBattlePayload(Common::ReadStream &input, Native::BattleRecords &state) {
    return input.read(state.stats,2048) == 2048 && input.read(state.compact,256) == 256 &&
        input.read(state.extended,256) == 256 && input.read(state.indices,187) == 187 && input.read(state.kinds,187) == 187;
}
inline bool validateNativeBattleRecords(Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','B','R','C') || input.readUint32LE() != 1 || input.readUint32LE() != 190) return false;
    for (uint i = 0; i < 190; ++i) {
        const byte kind = input.readByte(), index = input.readByte(), flags = input.readByte();
        Native::BattleRecords state, expected; byte readStats[128], readActor[8], writtenStats[128], writtenActor[8];
        if (!readBattlePayload(input,state) || input.read(readStats,128) != 128 || input.read(readActor,8) != 8 ||
            input.read(writtenStats,128) != 128 || input.read(writtenActor,8) != 8 || !readBattlePayload(input,expected)) return false;
        Native::VortexVictim actor;
        if (!state.actor(94,actor) || actor.kind != kind || actor.index != index || state.kinds[94] != (kind | flags) ||
            memcmp(actor.stats,readStats,128) || memcmp(actor.actor,readActor,kind == 3 ? 4 : 8)) {
            error = Common::String::format("Native battle record read differs at case%u",i); return false;
        }
        // Explicit typed record-write requests qualify ownership/copy bounds;
        // they are not claims that damage/movement gameplay earned these edits.
        WRITE_LE_UINT16(actor.stats + 2,1234); actor.stats[12] = 37; actor.actor[0] = 4; actor.actor[1] = 6;
        if (memcmp(actor.stats,writtenStats,128) || memcmp(actor.actor,writtenActor,kind == 3 ? 4 : 8) ||
            !state.store(actor) || !sameBattleRecords(state,expected)) {
            error = Common::String::format("Native battle record write differs at case%u",i); return false;
        }
    }
    if (input.err() || input.pos() != input.size()) return false;
    report += "native-battle-records\tcases=190\tenemy_slots=16\tcompanion_slots=16\tparty_slots=6\tfull_storage_comparison=1\tadjacent_compact_record_preserved=1\tadmission=0\tcpu_execution=0\n";
    return true;
}
}
#endif
