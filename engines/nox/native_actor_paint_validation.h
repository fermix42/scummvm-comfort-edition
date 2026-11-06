/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ACTOR_PAINT_VALIDATION_H
#define NOX_NATIVE_ACTOR_PAINT_VALIDATION_H
#include "nox/native_actor_paint.h"
namespace Nox {
inline bool validateNativeActorPaint(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','A','P','T') || input.readUint32LE() != 1 || input.readUint32LE() != 6668) return false;
    Native::ActorPaintRules rules; byte previous = 255; uint draws = 0;
    for (uint i = 0; i < 6668; ++i) {
        const byte type = input.readByte(); Native::ActorPaintContext context; context.slot = 0;
        context.tile = input.readByte(); context.capability = input.readByte(); const byte visibility = input.readByte(); context.large = input.readByte() != 0;
        context.cell = input.readByte(); context.column = input.readByte(); context.kind = input.readByte(); context.flags = input.readByte(); context.hiddenByTurn = input.readByte() != 0;
        const byte terrain = input.readByte(); memset(context.visibility, visibility, sizeof(context.visibility));
        byte expectedTerrain[187], expectedLookup[187], expectedKinds[187];
        if (input.read(expectedTerrain, 187) != 187 || input.read(expectedLookup, 187) != 187 || input.read(expectedKinds, 187) != 187) return false;
        const bool hidden = input.readByte() != 0; const byte count = input.readByte();
        if (type != previous) { if (!rules.load(archive, type)) return false; previous = type; }
        Native::ActorScene scene; memset(scene.terrain, terrain, sizeof(scene.terrain));
        if (!rules.paint(context, scene) || memcmp(scene.terrain, expectedTerrain, 187) || memcmp(scene.lookup, expectedLookup, 187) ||
            memcmp(scene.kinds, expectedKinds, 187) || scene.hiddenByTurn != hidden || scene.draws.size() != count) {
            error = Common::String::format("Native actor paint state differs at case%u", i); return false;
        }
        for (uint j = 0; j < count; ++j) {
            const byte cell = input.readByte(), tile = input.readByte(), height = input.readByte();
            if (scene.draws[j].cell != cell || scene.draws[j].tile != tile || scene.draws[j].height != height) { error = Common::String::format("Native actor paint draw differs at case%u", i); return false; }
        }
        draws += count;
    }
    if (draws != 8000 || input.err() || input.pos() != input.size()) return false;
    report += "native-actor-paint\tcases=6668\tstyles=8\tfull_terrain=187\tactor_lookups=187\tkind_lookups=187\tdraws=8000\tcomplete_actor_owner=0\tcpu_execution=0\n"; return true;
}
}
#endif
