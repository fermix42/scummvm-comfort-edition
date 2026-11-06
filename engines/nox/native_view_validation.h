/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_VIEW_VALIDATION_H
#define NOX_NATIVE_VIEW_VALIDATION_H
#include "nox/native_world.h"
namespace Nox {
inline bool validateNativeView(HdvArchive &archive, Common::SeekableReadStream &input,
        Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','V','P','R') || input.readUint32LE() != 1 || input.readUint32LE() != 7) return false;
    for (uint i = 0; i < 7; ++i) {
        const byte type = input.readByte(), map = input.readByte(), x = input.readByte(), y = input.readByte();
        byte expected[187], fields[256] = {}; Common::Array<byte> projected;
        Native::World world(archive);
        if (input.read(expected, sizeof(expected)) != sizeof(expected) || !world.initialize(type, map, x, y, fields, sizeof(fields)) ||
            !world.projectTerrain(projected) || projected.size() != sizeof(expected) || memcmp(projected.data(), expected, sizeof(expected))) {
            error = Common::String::format("Native terrain projection differs from historical map%u", uint(map)); return false;
        }
    }
    Native::World empty(archive); Common::Array<byte> untouched; untouched.push_back(77);
    if (empty.projectTerrain(untouched) || untouched.size() != 1 || untouched[0] != 77 || input.err() || input.pos() != input.size()) return false;
    report += "native-view\thistorical_maps=7\tterrain_cells=1309\tunready_atomic=1\tvisibility_and_composition=0\tcpu_execution=0\n"; return true;
}
}
#endif
