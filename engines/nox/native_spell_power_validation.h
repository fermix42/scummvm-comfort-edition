/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_SPELL_POWER_VALIDATION_H
#define NOX_NATIVE_SPELL_POWER_VALIDATION_H
#include "nox/native_spell_power.h"
namespace Nox {
inline bool validateNativeSpellPower(Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','S','D','M') || input.readUint32LE() != 1 || input.readUint32LE() != 34048 ||
            input.readUint32LE() != 256 || input.readUint32LE() != 4) return false;
    for (uint i = 0; i < 34048; ++i) {
        const byte power = input.readByte(), level = input.readByte(), threshold = input.readByte(), expected = input.readByte(); byte actual = 0;
        if (!Native::effectiveSpellPower(power, level, threshold, actual) || actual != expected) {
            error = Common::String::format("Native spell attenuation differs at case%u", i); return false;
        }
    }
    for (uint i = 0; i < 256; ++i) {
        const byte power = input.readByte(), lower = input.readByte(), upper = input.readByte(); const Native::SpellPowerBounds actual = Native::spellPowerBounds(power);
        if (power != i || actual.lower != lower || actual.upper != upper) { error = "Native spell variation bounds differ"; return false; }
    }
    for (uint stream = 0; stream < 4; ++stream) {
        Native::GameplayRandom random; if (!random.importState(input.readUint32LE(), 2)) return false;
        for (uint draw = 0; draw < 16; ++draw) {
            const byte power = input.readByte(), level = input.readByte(), threshold = input.readByte();
            const byte scaled = input.readByte(), lower = input.readByte(), upper = input.readByte(), value = input.readByte();
            const uint32 seed = input.readUint32LE(); const byte tail = input.readByte(); const uint16 attempts = input.readUint16LE(); Native::SpellPowerRoll actual;
            if (!Native::rollSpellPower(power, level, threshold, random, actual) || actual.scaledPower != scaled || actual.lower != lower || actual.upper != upper || actual.value != value ||
                    random.seed() != seed || random.tail() != tail || random.last() != value || random.generationAttempts() != attempts) {
                error = Common::String::format("Native retained spell power/RNG differs at stream%u draw%u", stream, draw); return false;
            }
        }
    }
    if (input.err() || input.pos() != input.size()) return false;
    report += "native-spell-power\tscaling_cases=34048\tvariation_cases=256\tretained_draws=64\thit_defense_HP=0\tmana_committed=0\tcpu_execution=0\n"; return true;
}
}
#endif
