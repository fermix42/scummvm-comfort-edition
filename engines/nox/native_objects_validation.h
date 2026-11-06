/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_OBJECTS_VALIDATION_H
#define NOX_NATIVE_OBJECTS_VALIDATION_H
#include "nox/native_objects.h"
namespace Nox {
inline bool validateNativeObjects(Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','O','B','J') || input.readUint32LE() != 1 || input.readUint32LE() != 4718) return false;
    uint draws = 0;
    for (uint i = 0; i < 4718; ++i) {
        Native::ObjectSceneContext context; context.x = input.readByte(); context.y = input.readByte(); context.partyObject = input.readByte(); context.leftBoundary = input.readByte();
        byte objects[256], expected[256], lookup[187];
        if (input.read(context.visibility, 187) != 187 || input.read(context.actors, 187) != 187 || input.read(context.fields, 256) != 256 ||
            input.read(objects, 256) != 256 || input.read(expected, 256) != 256 || input.read(lookup, 187) != 187) return false;
        const byte portal = input.readByte(), count = input.readByte(); Native::ObjectScene scene;
        if (!Native::applyObjectRecord(objects, sizeof(objects), 0, context, scene) || memcmp(objects, expected, sizeof(objects)) ||
            memcmp(scene.lookup, lookup, sizeof(lookup)) || scene.portal != portal || scene.draws.size() != count) {
            error = Common::String::format("Native object phase differs at controlled case%u", i); return false;
        }
        for (uint j = 0; j < count; ++j) {
            const byte cell = input.readByte(), tile = input.readByte();
            if (scene.draws[j].cell != cell || scene.draws[j].tile != tile) { error = Common::String::format("Native object draw differs at case%u", i); return false; }
        }
        draws += count;
    }
    if (input.err() || input.pos() != input.size()) return false;
    report += Common::String::format("native-objects\tcases=4718\tfull_records=256\tlookup_cells=187\tdraws=%u\tcomplete_actor_phase=0\tcpu_execution=0\n", draws); return true;
}
}
#endif
