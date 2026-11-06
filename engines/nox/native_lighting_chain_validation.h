/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_LIGHTING_CHAIN_VALIDATION_H
#define NOX_NATIVE_LIGHTING_CHAIN_VALIDATION_H
#include "nox/native_lighting.h"
namespace Nox {
inline bool validateNativeLightingChain(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','L','C','H') || input.readUint32LE() != 1 || input.readUint32LE() != 760) return false;
    Native::VisibilityMask mask; if (!mask.loadRules(archive)) return false;
    for (uint mapCase = 0; mapCase < 76; ++mapCase) {
        Native::World world(archive); Native::TerrainLighting rules;
        for (uint context = 0; context < 10; ++context) {
            const byte type = input.readByte(), map = input.readByte(), x = input.readByte(), y = input.readByte();
            Native::LightState state; state.mode = input.readByte(); state.level = input.readByte(); state.transitionTicks = input.readByte();
            const byte floor = input.readByte(), appearance = input.readByte(), extra = input.readByte(); byte initial[187], fields[256] = {};
            if (!context && (!world.initialize(type, map, x, y, fields, sizeof(fields)) || !rules.load(archive, type))) return false;
            if (world.mapType() != type || world.mapId() != map || world.x() != x || world.y() != y || input.read(initial, 187) != 187 || !mask.importMask(initial, 187)) return false;
            for (uint step = 0; step < 2; ++step) {
                const byte mode = input.readByte(), level = input.readByte(), ticks = input.readByte(); byte expected[187];
                if (input.read(expected, 187) != 187 || !Native::prepareSceneLighting(world, rules, floor, appearance, extra, state, mask) ||
                    state.mode != mode || state.level != level || state.transitionTicks != ticks || memcmp(mask.cells(), expected, 187)) {
                    error = Common::String::format("Native linked lighting differs at map%u context%u step%u", uint(map), context, step); return false;
                }
            }
        }
    }
    if (input.err() || input.pos() != input.size()) return false;
    report += "native-lighting-chain\tcases=760\tpreparations=1520\tmaps=76\tfull_cells=187\tretained_state=1\tcomplete_scene=0\tcpu_execution=0\n"; return true;
}
}
#endif
