/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ACTOR_PAINT_H
#define NOX_NATIVE_ACTOR_PAINT_H
#include "nox/native_graphics.h"
namespace Nox { namespace Native {
struct ActorDraw {
    byte cell, tile, height;
    ActorDraw(byte c, byte t, byte h) : cell(c), tile(t), height(h) {}
};
struct ActorPaintContext {
    byte slot, kind, cell, column, tile, capability, flags;
    bool large, hiddenByTurn;
    byte visibility[187];
};
struct ActorScene {
    byte terrain[187], lookup[187], kinds[187];
    bool hiddenByTurn;
    Common::Array<ActorDraw> draws;
    ActorScene() : hiddenByTurn(false) { memset(terrain, 0, sizeof(terrain)); memset(lookup, 255, sizeof(lookup)); memset(kinds, 255, sizeof(kinds)); }
};
// Original actor paint/lookup stage, distinct from admission, AI movement and
// record commits. Draw row heights are explicit for the renderer owner.
class ActorPaintRules {
public:
    ActorPaintRules() : _ready(false), _type(255) {}
    byte mapType() const { return _type; }
    bool load(HdvArchive &archive, byte type) {
        Common::Array<byte> game, data; MapAssetRange assets;
        if (!archive.read("NA/GAME", game) || !resolveMapAssets(game, type, assets) || !archive.readRange(assets.resource, assets.offset + assets.size, sizeof(_footer), data)) return false;
        memcpy(_footer, data.data(), sizeof(_footer)); _type = type; _ready = true; return true;
    }
    bool paint(const ActorPaintContext &input, ActorScene &scene) const {
        if (!_ready || input.slot > 252 || (input.slot & 3)) return false;
        scene.hiddenByTurn = input.hiddenByTurn;
        if (input.cell >= 186) return true;
        if (input.large) {
            const uint cells[] = {uint(input.cell), uint(input.cell) + 1, uint(input.cell) + 17, uint(input.cell) + 18};
            for (uint i = 0; i < 4; ++i) {
                const uint cell = cells[i];
                if (cell >= 187 || ((i == 1 || i == 3) ? input.column == 136 : input.column == 119)) continue;
                if (!input.visibility[cell]) {
                    if (scene.terrain[cell] == 76 || scene.terrain[cell] == 65 || scene.terrain[cell] == 66) scene.terrain[cell] = 64;
                    scene.draws.push_back(ActorDraw(byte(cell), byte(input.tile + i), 16));
                }
                scene.lookup[cell] = input.slot; scene.kinds[cell] = byte(input.kind | (64 >> i));
            }
            return true;
        }
        if (!input.capability && !input.hiddenByTurn && (input.flags & 128)) return true;
        const uint cell = input.cell;
        if (!input.visibility[cell]) {
            const byte terrain = scene.terrain[cell]; byte picture = input.tile, height = 16;
            const bool surface = (terrain >= _footer[36] && terrain < _footer[37]) || (terrain >= _footer[38] && terrain < _footer[39]);
            const bool surfacePicture = (_type == 2 && !input.kind) ? input.tile == 185 : !input.capability;
            if (!(input.capability & 128) && surface && surfacePicture) picture = byte(terrain + 1);
            else {
                if (terrain >= _footer[28] && terrain < _footer[29]) return true;
                if (input.tile == _footer[62] && terrain == 232 && _footer[63]) picture = _footer[63];
                else if (terrain == 76 || terrain == 65 || terrain == 66) height = 8;
            }
            scene.draws.push_back(ActorDraw(byte(cell), picture, height));
        }
        scene.hiddenByTurn = false; scene.lookup[cell] = input.slot; scene.kinds[cell] = input.kind; return true;
    }
private:
    bool _ready; byte _type, _footer[64];
};
} }
#endif
