/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_OBJECTS_H
#define NOX_NATIVE_OBJECTS_H
#include "common/array.h"
#include "common/scummsys.h"
namespace Nox { namespace Native {
struct ObjectSceneContext {
    byte x, y, partyObject, leftBoundary;
    byte visibility[187], actors[187], fields[256];
};
struct ObjectDraw { byte cell, tile, slot; ObjectDraw(byte c, byte t,byte s=255) : cell(c), tile(t),slot(s) {} };
struct ObjectScene {
    byte lookup[187], portal;
    Common::Array<ObjectDraw> draws;
    ObjectScene() : portal(255) { memset(lookup, 255, sizeof(lookup)); }
};
// Complete single-object projection/door-settle phase. Actor/visibility phases
// supply their results explicitly. It does not move actors or advance a turn.
inline bool applyObjectRecord(byte *objects, uint size, uint slot, const ObjectSceneContext &context, ObjectScene &scene) {
    if (!objects || size != 256 || slot > 252 || (slot & 3)) return false;
    byte *object = objects + slot;
    if (!object[2]) return true;
    const byte dxEncoded = byte(byte(object[0] - context.x) ^ 128), dyEncoded = byte(byte(object[1] - context.y) ^ 128);
    if (dxEncoded < 120 || dxEncoded >= 137 || dyEncoded < 123 || dyEncoded >= 134) return true;
    const uint cell = uint(93 + int(dxEncoded) - 128 + (int(dyEncoded) - 128) * 17);
    const byte tile = object[2]; byte state = object[3];
    if (tile != 119 && state == 54 && !context.visibility[cell] && size - slot < 8) return false;
    if (tile == 119) {
        const uint cells[] = {cell, cell + 1, cell + 17, cell + 18};
        for (uint i = 0; i < 4; ++i) {
            if (cells[i] >= 187 || ((i == 1 || i == 3) ? dxEncoded == 136 : dxEncoded == context.leftBoundary)) continue;
            const uint target = cells[i];
            if (!context.visibility[target]) {
                if (slot == context.partyObject && (target == 93 || target == 94 || target == 110 || target == 111)) continue;
                scene.draws.push_back(ObjectDraw(byte(target), byte(tile + i),byte(slot)));
            }
            scene.lookup[target] = byte(slot);
        }
        return true;
    }
    if (context.visibility[cell]) { scene.lookup[cell] = byte(slot); return true; }
    if (!(cell == 93 && slot == context.partyObject)) {
        scene.lookup[cell] = byte(slot);
        if (state == 56 && object[0] == context.x && object[1] == context.y) scene.portal = byte(slot);
    }
    if (state == 254 || state == 55) return true;
    if (state >= 33 && state < 42) {
        if (cell == 93) state = 41;
        else if (context.actors[cell] != 255) state = 35;
        else --state;
        if (state != 32) { object[3] = state; return true; }
        state = 16;
    }
    byte picture = tile;
    if (state == 54) {
        // Extended record parameters follow the first four bytes.
        if (context.fields[object[4]] & object[7]) ++picture;
    }
    object[3] = state; scene.draws.push_back(ObjectDraw(byte(cell), picture,byte(slot))); return true;
}
} }
#endif
