/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_VORTEX_CAST_VALIDATION_H
#define NOX_NATIVE_VORTEX_CAST_VALIDATION_H
#include "nox/native_vortex_cast.h"
#include "nox/native_vortex_checkpoint.h"
namespace Nox {
inline bool readBattleRecords(Common::ReadStream &input, Native::BattleRecords &state) {
    if (input.read(state.stats, 2048) != 2048 || input.read(state.compact, 256) != 256 ||
        input.read(state.extended, 256) != 256 || input.read(state.indices, 187) != 187 || input.read(state.kinds, 187) != 187) return false;
    state.credit = input.readUint16LE(); return !input.err();
}
inline bool sameBattleRecords(const Native::BattleRecords &a, const Native::BattleRecords &b) {
    return a.credit == b.credit && !memcmp(a.stats,b.stats,2048) && !memcmp(a.compact,b.compact,256) &&
        !memcmp(a.extended,b.extended,256) && !memcmp(a.indices,b.indices,187) && !memcmp(a.kinds,b.kinds,187);
}
inline bool validateNativeBoundVortex(HdvArchive &archive, const Native::SpellBook &boundBook,
        Common::SeekableReadStream &input, Common::String &report, Common::String &error, bool coldBook) {
    if (input.readUint32BE() != MKTAG('N','V','C','A') || input.readUint32LE() != 1 || input.readUint32LE() != 288 ||
        !input.seek(12 + 24 * 5897)) return false;
    const byte direction = input.readByte(), protection = input.readByte(); byte oracleBook[20], actualBook[20];
    Native::BattleRecords battle, expected;
    if (input.read(oracleBook,20) != 20 || !boundBook.exportState(actualBook,20) || memcmp(actualBook,oracleBook,20) ||
        direction != Native::kNorth || protection || !readBattleRecords(input,battle)) return false;
    const byte result = input.readByte(), targets = input.readByte(), killed = input.readByte();
    if (!readBattleRecords(input,expected) || battle.stats[4] != 110 || READ_LE_UINT16(battle.stats + 770) != 12) return false;
    Native::CastRules rules; Native::VortexEffects effects; Native::VortexCastOutcome outcome;
    if (!rules.load(archive) || !effects.load(archive) ||
        !Native::castVortex(rules,effects,boundBook,1,143,8,false,Native::kNorth,battle,outcome) ||
        uint(outcome.result) != result || outcome.targets != targets || outcome.killed != killed || !sameBattleRecords(battle,expected) ||
        battle.stats[4] != 23 || READ_LE_UINT16(battle.stats + 770) != 0) { error = "Native bound-book Vortex effect differs"; return false; }
    // Retain the actual cast result for the next attempted cast. No healing,
    // MP reset or oracle assignment occurs between these gameplay operations.
    Native::BattleRecords retained = battle;
    if (!Native::castVortex(rules,effects,boundBook,1,143,8,false,Native::kNorth,battle,outcome) ||
        outcome.result != Native::kCastManaRefusal || !sameBattleRecords(battle,retained) ||
        !boundBook.exportState(actualBook,20) || memcmp(actualBook,oracleBook,20)) return false;
    report += Common::String::format("native-binding-vortex-link\tphase=%s\tactual_binding_ready_book=1\tcontrolled_battle_entry=1\toriginal_living_Rat_HP=12\tactual_native_kill=1\tMP=23\tretained_mana_refusal=1\tearned_travel=0\tcpu_execution=0\n",coldBook ? "restored-book" : "warm");
    return true;
}
inline bool validateNativeVortexCast(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error,
        Common::SaveFileManager &saves, bool cold) {
    if (input.readUint32BE() != MKTAG('N','V','C','A') || input.readUint32LE() != 1 || input.readUint32LE() != 288) return false;
    Native::CastRules rules; Native::VortexEffects effects;
    if (!rules.load(archive) || !effects.load(archive)) return false;
    Native::BattleRecords retained; uint kills = 0, refusals = 0, empty = 0;
    Native::VortexCheckpoint restored;
    if (cold) {
        Common::InSaveFile *saved = saves.openForLoading("nox-native-vortex-checkpoint.bin");
        if (!saved) return false; const bool ok = restored.load(*saved); delete saved;
        if (!ok) return false;
    }
    for (uint i = 0; i < 288; ++i) {
        const byte direction = input.readByte(), protection = input.readByte(); byte bookBytes[20];
        if (input.read(bookBytes,20) != 20) return false;
        Native::SpellBook book; Native::BattleRecords before, expected;
        if (!book.importState(bookBytes,20) || !readBattleRecords(input,before)) return false;
        const byte result = input.readByte(), targets = input.readByte(), killed = input.readByte();
        if (!readBattleRecords(input,expected)) return false;
        if (cold && (i < 49 || i > 50)) continue;
        // Initialization occurs once per three-operation chain. Subsequent
        // operations retain the actual native result, never the expected one.
        if (cold && i == 49) {
            byte actualBook[20];
            if (!restored.book.exportState(actualBook,20) || memcmp(actualBook,bookBytes,20) || restored.direction != direction ||
                restored.protection != protection || restored.casterCell != 143 || restored.category != 8 || !sameBattleRecords(restored.battle,before)) return false;
            retained = restored.battle; book = restored.book;
        } else if (!(i % 3)) retained = before;
        else if (!sameBattleRecords(retained,before)) { error = "Native retained cast predecessor differs"; return false; }
        Native::VortexCastOutcome outcome;
        if (!Native::castVortex(rules,effects,book,1,143,8,protection != 0,Native::Direction(direction),retained,outcome) ||
            uint(outcome.result) != result || outcome.targets != targets || outcome.killed != killed || !sameBattleRecords(retained,expected)) {
            error = Common::String::format("Native logical Vortex cast differs at case%u",i); return false;
        }
        kills += outcome.killed; refusals += outcome.result == Native::kCastManaRefusal;
        empty += outcome.result == Native::kCastPrepared && !outcome.targets;
        if (!cold && i == 48) {
            Native::VortexCheckpoint checkpoint; checkpoint.battle = retained; checkpoint.book = book;
            checkpoint.casterCell = 143; checkpoint.category = 8; checkpoint.protection = protection; checkpoint.direction = direction;
            Common::OutSaveFile *saved = saves.openForSaving("nox-native-vortex-checkpoint.bin",false);
            if (!saved) return false; bool ok = checkpoint.save(*saved); saved->finalize(); ok &= !saved->err(); delete saved;
            if (!ok) return false;
        }
    }
    if (input.err() || input.pos() != input.size()) return false;
    if (cold) report += "native-vortex-checkpoint\tphase=cold\tactual_result_restored=1\tcontinued_cast=1\tcontinued_mana_refusal=1\tcpu_execution=0\n";
    else {
        report += Common::String::format("native-vortex-cast\tcases=288\tchains=96\tkilled=%u\tmana_refusals=%u\tempty_paid_casts=%u\tfull_battle_records=1\tordinary_input=0\tcpu_execution=0\n",kills,refusals,empty);
        report += "native-vortex-checkpoint\tphase=warm\tactual_native_result_saved=1\tcomplete_battle_save=0\n";
    }
    return true;
}
}
#endif
