/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_SPELL_POWER_H
#define NOX_NATIVE_SPELL_POWER_H
#include "nox/native_random.h"
#include "common/util.h"
namespace Nox { namespace Native {
struct SpellPowerBounds { byte lower, upper; };
struct SpellPowerRoll { byte scaledPower, lower, upper, value; };
inline bool effectiveSpellPower(byte basePower, byte defenderLevel, byte spellThreshold, byte &output) {
    if (defenderLevel > 127) return false;
    byte power = basePower;
    if (defenderLevel >= spellThreshold && defenderLevel - spellThreshold >= 2) {
        const uint shifts = defenderLevel - spellThreshold - 1;
        power = shifts >= 8 ? 0 : byte(power >> shifts);
    }
    output = power; return true;
}
inline SpellPowerBounds spellPowerBounds(byte power) {
    const uint spread = power / 10;
    SpellPowerBounds bounds = { byte(power - spread), byte(MIN<uint>(255, power + spread)) }; return bounds;
}
// Power draw only. Hit/armor/status/HP/death handling and mana commit belong
// to the complete battle operation; this helper does not apply an outcome.
inline bool rollSpellPower(byte basePower, byte defenderLevel, byte spellThreshold, GameplayRandom &random, SpellPowerRoll &output) {
    SpellPowerRoll next;
    if (!effectiveSpellPower(basePower, defenderLevel, spellThreshold, next.scaledPower)) return false;
    const SpellPowerBounds bounds = spellPowerBounds(next.scaledPower); next.lower = bounds.lower; next.upper = bounds.upper;
    GameplayRandom candidate = random;
    if (!candidate.next(next.lower, next.upper, next.value)) return false;
    random = candidate; output = next; return true;
}
} }
#endif
