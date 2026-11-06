/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_PERSONAL_LIGHT_VALIDATION_H
#define NOX_NATIVE_PERSONAL_LIGHT_VALIDATION_H
#include "nox/native_visibility.h"
namespace Nox {
inline bool validateNativePersonalLight(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','P','L','T') || input.readUint32LE() != 1 || input.readUint32LE() != 8192) return false;
    Native::VisibilityMask mask; if (!mask.loadRules(archive)) return false;
    for (uint i = 0; i < 8192; ++i) {
        const byte mode = input.readByte(), appearance = input.readByte(), ticks = input.readByte(); byte initial[187], expected[187];
        if (input.read(initial, 187) != 187 || input.read(expected, 187) != 187 || !mask.importMask(initial, 187) || !mask.revealAdditional(mode, appearance, ticks) || memcmp(mask.cells(), expected, 187)) {
            error = Common::String::format("Native additional light differs at case%u", i); return false;
        }
    }
    if (input.err() || input.pos() != input.size()) return false;
    report += "native-personal-light\tcases=8192\tmodes=4\tlight_ticks=256\tfull_cells=187\tresource_indices=90\tcpu_execution=0\n"; return true;
}
}
#endif
