/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_TERMINAL_VALIDATION_H
#define NOX_NATIVE_BATTLE_TERMINAL_VALIDATION_H
#include "nox/native_battle_terminal.h"
namespace Nox {
inline bool validateNativeBattleTerminal(Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','T','E') || input.readUint32LE()!=1) return false;
    const uint32 exits=input.readUint32LE(),deaths=input.readUint32LE();
    if (!exits || exits>10000 || !deaths || deaths>10000 || input.size()!=16+exits*10+deaths*8) return false;
    for (uint i=0;i<exits;++i) {
        byte row[10];if (input.read(row,10)!=10 || row[0]>=32 || row[3]>1) return false;
        Native::BattleRecords records;records.extended[row[0]*8+3]=row[2];records.extended[row[0]*8+4]=row[1];
        byte control=0;bool prompt=false,refusal=false;
        const Native::BattleExitDecision result=Native::BattleTerminal::decideExit(records,row[3]!=0,row[4],row[5],control,prompt,refusal);
        if (uint(result)!=row[6] || uint(prompt)!=row[7] || uint(refusal)!=row[8] || control!=row[9]) {
            report=Common::String::format("terminal exit case%u differs result%u/%u prompt%u/%u refusal%u/%u control%u/%u",i,uint(result),uint(row[6]),uint(prompt),uint(row[7]),uint(refusal),uint(row[8]),uint(control),uint(row[9]));return false;
        }
    }
    for (uint i=0;i<deaths;++i) {
        byte row[8];if (input.read(row,8)!=8 || !row[0] || row[0]>6 || row[7]>1) return false;
        Native::BattleRecords records;for (uint member=0;member<row[0];++member) records.stats[member*128]=row[member+1];
        if (Native::BattleTerminal::defeated(records,row[0])!=(row[7]!=0)) return false;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("native-battle-terminal\texit_cases=%u\tdefeat_cases=%u\tall32_actor_affiliation_status=1\tconfirmation_before_no_flee_refusal=1\tparty_status_only_defeat=1\tfull_flee_defeat_UI=0\tcpu_execution=0\n",exits,deaths);return true;
}
}
#endif
