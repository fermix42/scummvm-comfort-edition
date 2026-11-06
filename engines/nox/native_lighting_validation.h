/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_LIGHTING_VALIDATION_H
#define NOX_NATIVE_LIGHTING_VALIDATION_H
#include "nox/native_lighting.h"
namespace Nox {
inline bool validateNativeTerrainLighting(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','T','L','T') || input.readUint32LE() != 1 || input.readUint32LE() != 1824) return false;
    Native::VisibilityMask mask; if (!mask.loadRules(archive)) return false;
    for (uint mapCase = 0; mapCase < 76; ++mapCase) {
        Native::World world(archive); Native::TerrainLighting rules;
        for (uint context = 0; context < 24; ++context) {
            const byte type = input.readByte(), map = input.readByte(), x = input.readByte(), y = input.readByte(), floor = input.readByte(), mode = input.readByte();
            byte initial[187], expected[187], fields[256] = {};
            if (!context) { if (!world.initialize(type, map, x, y, fields, sizeof(fields)) || !rules.load(archive, type)) return false; }
            if (world.mapType() != type || world.mapId() != map || world.x() != x || world.y() != y ||
                input.read(initial, 187) != 187 || input.read(expected, 187) != 187 || !mask.importMask(initial, 187) || !rules.apply(world, floor, mode, mask) || memcmp(mask.cells(), expected, 187)) {
                error = Common::String::format("Native terrain lighting differs at map%u context%u", uint(map), context); return false;
            }
        }
    }
    if (input.err() || input.pos() != input.size()) return false;
    report += "native-terrain-light\tcases=1824\tmaps=76\tfloors=4\tfull_cells=187\touter_sources=1\tcomplete_scene=0\tcpu_execution=0\n"; return true;
}
}
#endif
