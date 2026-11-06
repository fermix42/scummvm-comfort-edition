/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_PRESENTATION_VALIDATION_H
#define NOX_NATIVE_WORLD_PRESENTATION_VALIDATION_H
#include "nox/native_world_presentation.h"
namespace Nox {
inline bool validateNativeWorldPresentation(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','S','C','N') || input.readUint32LE() != 1 || input.readUint32LE() != 228) return false;
    uint drawCount = 0;
    for (uint i = 0; i < 228; ++i) {
        const byte type = input.readByte(), map = input.readByte(), x = input.readByte(), y = input.readByte();
        Native::PresentationState state; state.facing = input.readByte(); state.hidden = input.readByte() != 0;
        byte fields[256], visibility[187];
        if (input.read(fields, 256) != 256 || input.read(visibility, 187) != 187) return false;
        Native::World world(archive); Native::ActorPaintRules rules;
        if (!world.initialize(type, map, x, y, fields, sizeof(fields)) || !rules.load(archive, type)) return false;
        for (uint step = 0; step < 2; ++step) {
            byte tables[1792], terrain[187], objects[187], actors[187], kinds[187], expectedFields[256];
            if (input.read(tables, 1792) != 1792 || input.read(terrain, 187) != 187 || input.read(objects, 187) != 187 || input.read(actors, 187) != 187 || input.read(kinds, 187) != 187 || input.read(expectedFields, 256) != 256) return false;
            const byte facing = input.readByte(), hidden = input.readByte(), portal = input.readByte(); const uint draws = input.readUint16LE(); Native::WorldScene scene;
            if (!Native::WorldPresentation::run(world, rules, visibility, sizeof(visibility), state, scene) ||
                memcmp(world.currentTables()->data(), tables, 1792) || memcmp(scene.actors.terrain, terrain, 187) || memcmp(scene.objects.lookup, objects, 187) ||
                memcmp(scene.actors.lookup, actors, 187) || memcmp(scene.actors.kinds, kinds, 187) || memcmp(world.questFields(), expectedFields, 256) ||
                state.facing != facing || state.hidden != (hidden != 0) || scene.objects.portal != portal || scene.draws.size() != draws) {
                error = Common::String::format("Native world presentation differs at map%u profile%u step%u", uint(map), i % 3, step); return false;
            }
            for (uint j = 0; j < draws; ++j) {
                const byte cell = input.readByte(), tile = input.readByte(), height = input.readByte();
                if (scene.draws[j].cell != cell || scene.draws[j].tile != tile || scene.draws[j].height != height) {
                    error = Common::String::format("Native ordered scene draw differs at map%u profile%u step%u draw%u", uint(map), i % 3, step, j); return false;
                }
            }
            drawCount += draws;
        }
    }
    if (drawCount != 980 || input.err() || input.pos() != input.size()) return false;
    report += "native-world-presentation\tcases=228\tpresentations=456\tmaps=76\tordered_draws=980\tfull_records_and_lookups=1\tretained_state=1\tcomplete_ai=0\tcpu_execution=0\n"; return true;
}
}
#endif
