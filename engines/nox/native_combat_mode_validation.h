/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_COMBAT_MODE_VALIDATION_H
#define NOX_NATIVE_COMBAT_MODE_VALIDATION_H
#include "nox/native_combat_mode.h"
namespace Nox {
inline bool validateNativeCombatMode(Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','Q','M','D') || input.readUint32LE()!=1) return false;
    const uint32 gates=input.readUint32LE(),choices=input.readUint32LE(),advice=input.readUint32LE();
    if (!gates || gates>100000 || !choices || choices>100000 || !advice || advice>100000) return false;
    for (uint32 i=0;i<gates;++i) {
        const byte category=input.readByte(),flags=input.readByte(),sprite=input.readByte(),forced=input.readByte(),refused=input.readByte();bool forbid;
        if (Native::CombatModeRules::forcedTactical(category,sprite,flags,forbid)!=bool(forced) || forbid!=bool(refused)) { report=Common::String::format("Mode gate%u differs",i);return false; }
    }
    for (uint32 i=0;i<choices;++i) {
        byte row[8];if (input.read(row,8)!=8) return false;
        const Native::CombatModeChoice choice=Native::CombatModeRules::choose(row[0],row[1]!=0,row[2]);
        if (choice.accepted!=bool(row[3]) || choice.mode!=row[4] || choice.display!=row[5] || choice.stepped!=row[6]) { report=Common::String::format("Mode choice%u differs",i);return false; }
    }
    for (uint32 i=0;i<advice;++i) {
        byte row[14];if (input.read(row,14)!=14) return false;Native::CombatAdvice result;
        if (!Native::CombatModeRules::advice(row+1,row[0],row[7],row[8],row[9],row[10],result) || result.average!=row[11] || result.difference!=row[12] || result.warning!=bool(row[13])) {
            report=Common::String::format("Mode advice%u differs avg%u/%u difference%u/%u warning%u/%u",i,uint(result.average),uint(row[11]),uint(result.difference),uint(row[12]),uint(result.warning),uint(row[13]));return false;
        }
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("native-combat-mode\tgates=%u\tchoices=%u\tadvice=%u\tfull_source_policy=1\tordinary_mode_UI=0\tcpu_execution=0\n",gates,choices,advice);return true;
}
}
#endif
