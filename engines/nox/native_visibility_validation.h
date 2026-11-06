/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_VISIBILITY_VALIDATION_H
#define NOX_NATIVE_VISIBILITY_VALIDATION_H
#include "nox/native_visibility.h"
namespace Nox {
inline bool validateNativeVisibility(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','V','I','S') || input.readUint32LE() != 1 || input.readUint32LE() != 1065) return false;
    Native::VisibilityMask mask;
    if (!mask.loadRules(archive)) return false;
    uint changed = 0;
    for (uint i = 0; i < 1065; ++i) {
        const int column = input.readSint16LE(), row = input.readSint16LE(); byte initial[187], expected[187];
        if (input.read(initial, 187) != 187 || input.read(expected, 187) != 187 || !mask.importMask(initial, 187) || !mask.revealSource(column, row) || memcmp(mask.cells(), expected, 187)) {
            error = Common::String::format("Native visibility reveal differs at case%u", i); return false;
        }
        changed += memcmp(initial, expected, sizeof(initial)) != 0;
    }
    Native::VisibilityMask unready; byte initial[187]; memset(initial, 1, sizeof(initial));
    if (!unready.importMask(initial, 187) || unready.revealSource(8, 5) || memcmp(unready.cells(), initial, sizeof(initial)) || !changed || input.err() || input.pos() != input.size()) return false;
    report += Common::String::format("native-visibility\tcases=1065\tfull_cells=187\tchanged_cases=%u\tunready_atomic=1\tcomplete_visibility=0\tcpu_execution=0\n", changed); return true;
}
}
#endif
