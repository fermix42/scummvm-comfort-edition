/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_SUMMON_OUTCOME_H
#define NOX_NATIVE_BATTLE_SUMMON_OUTCOME_H
#include "common/array.h"
#include "common/scummsys.h"
namespace Nox { namespace Native {
struct SummonOutcome {
    uint requested,allocated,placed,dismissed,refused,failedPlacement;bool spentMana;
    Common::Array<byte> slots;
    SummonOutcome() : requested(0),allocated(0),placed(0),dismissed(0),refused(0),failedPlacement(0),spentMana(false) {}
};
} }
#endif
