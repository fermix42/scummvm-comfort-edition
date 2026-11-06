/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_VORTEX_TARGETS_VALIDATION_H
#define NOX_NATIVE_VORTEX_TARGETS_VALIDATION_H
#include "nox/native_vortex_targets.h"
namespace Nox {
inline bool validateNativeVortexTargets(Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','V','T','X') || input.readUint32LE() != 1 || input.readUint32LE() != 9700) return false;
    uint total = 0, maximum = 0;
    for (uint i = 0; i < 9700; ++i) {
        const byte x = input.readByte(), y = input.readByte(), direction = input.readByte(); const uint count = input.readUint16LE();
        if (count > 187 || direction > 3) return false;
        Native::BattleActorCell cells[187];
        for (uint j = 0; j < count; ++j) {
            const uint cell = input.readByte(); if (cell >= 187 || cells[cell].present) return false;
            cells[cell].present = true; cells[cell].x = input.readByte(); cells[cell].y = input.readByte(); cells[cell].state = input.readByte(); cells[cell].kindFlags = input.readByte();
        }
        const uint expectedCount = input.readByte(); byte expectedData[114], expectedCells[187], actualData[114] = {}, actualCells[187]; memset(actualCells, 255, sizeof(actualCells));
        if (input.read(expectedData, 114) != 114 || input.read(expectedCells, 187) != 187) return false;
        Common::Array<Native::SpellTarget> targets;
        if (!Native::collectVortexTargets(x, y, Native::Direction(direction), cells, 187, targets) || targets.size() != expectedCount) {
            error = Common::String::format("Native Vortex target count differs at case%u", i); return false;
        }
        for (uint j = 0; j < targets.size(); ++j) {
            actualData[j * 3] = targets[j].distance; actualData[j * 3 + 1] = targets[j].x; actualData[j * 3 + 2] = targets[j].y; actualCells[j] = targets[j].cell;
        }
        if (memcmp(actualData, expectedData, 114) || memcmp(actualCells, expectedCells, 187)) {
            error = Common::String::format("Native Vortex target order/geometry differs at case%u", i); return false;
        }
        total += targets.size(); maximum = MAX<uint>(maximum, targets.size());
    }
    Native::BattleActorCell unsupported[187];
    for (uint i = 0; i < 38; ++i) { unsupported[i].present = true; unsupported[i].x = 5; unsupported[i].y = 0; }
    Common::Array<Native::SpellTarget> retained; retained.push_back(Native::SpellTarget(93, 5, 5, 0));
    if (Native::collectVortexTargets(5, 5, Native::kNorth, unsupported, 187, retained) || retained.size() != 1 || retained[0].cell != 93) return false;
    if (input.err() || input.pos() != input.size() || total != 2071 || maximum != 37) return false;
    report += "native-vortex-targets\tcases=9700\tselected=2071\tmaximum_qualified=37\tunsafe_context_rejected=1\tdamage=0\tcpu_execution=0\n"; return true;
}
}
#endif
