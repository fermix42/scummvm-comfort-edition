/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_LIGHT_VALIDATION_H
#define NOX_NATIVE_LIGHT_VALIDATION_H
#include "nox/native_visibility.h"
namespace Nox {
inline bool validateNativeLight(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','L','I','T') || input.readUint32LE() != 1 || input.readUint32LE() != 10240) return false;
    Native::VisibilityMask mask; if (!mask.loadRules(archive)) return false;
    for (uint i = 0; i < 10240; ++i) {
        Native::LightState before; before.mode = input.readByte(); before.level = input.readByte(); before.transitionTicks = input.readByte();
        byte initial[187], expected[187];
        if (input.read(initial, 187) != 187) return false;
        const byte mode = input.readByte(), level = input.readByte(), ticks = input.readByte(); Native::LightState actual;
        if (input.read(expected, 187) != 187 || !mask.importMask(initial, 187) || !mask.prepareBaseLight(before, actual) ||
            actual.mode != mode || actual.level != level || actual.transitionTicks != ticks || memcmp(mask.cells(), expected, sizeof(expected))) {
            error = Common::String::format("Native base light transition differs at case%u", i); return false;
        }
    }
    byte before[187]; memcpy(before, mask.cells(), 187); Native::LightState untouched(77, 78, 79);
    if (mask.prepareBaseLight(Native::LightState(4, 0, 0), untouched) || untouched.mode != 77 || untouched.level != 78 || untouched.transitionTicks != 79 || memcmp(before, mask.cells(), 187) || input.err() || input.pos() != input.size()) return false;
    report += "native-light\tcases=10240\tmodes=4\tlevels=5\ttransition_ticks=256\tfull_cells=187\tatomic_invalid_state=1\tcomplete_visibility=0\tcpu_execution=0\n"; return true;
}
}
#endif
