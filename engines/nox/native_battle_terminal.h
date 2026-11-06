/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_TERMINAL_H
#define NOX_NATIVE_BATTLE_TERMINAL_H
#include "nox/native_vortex_cast.h"
namespace Nox { namespace Native {
enum BattleExitDecision { kBattleExitVictory,kBattleExitCancelled,kBattleExitFled,kBattleExitRefused };
class BattleTerminal {
public:
    // Source uses actor affiliation/state,not HP or the admitted enemy count.
    static bool hostiles(const BattleRecords &records) {
        for (uint i=0;i<32;++i) {
            const byte *actor=records.extended+i*8;
            if (actor[4]!=3 && actor[3] && actor[3]!=255) return true;
        }
        return false;
    }
    static bool defeated(const BattleRecords &records,byte count) {
        if (!count || count>6) return false;
        for (uint i=0;i<count;++i) if (records.stats[i*128]!=255) return false;
        return true;
    }
    static BattleExitDecision decideExit(const BattleRecords &records,bool scan,byte control,byte answer,byte &after,bool &prompt,bool &refusal) {
        after=control;prompt=scan && hostiles(records);refusal=false;
        if (!prompt) return kBattleExitVictory;
        if (answer>=225 && answer<251) answer&=223;
        if (answer!=217) return kBattleExitCancelled;
        if (control&128) { refusal=true;return kBattleExitRefused; }
        after=3;return kBattleExitFled;
    }
};
} }
#endif
