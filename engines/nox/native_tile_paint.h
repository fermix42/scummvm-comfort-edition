/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_TILE_PAINT_H
#define NOX_NATIVE_TILE_PAINT_H
#include "nox/native_graphics.h"
namespace Nox { namespace Native {
class TilePainter {
public:
    TilePainter() : _ready(false) {}
    bool load(HdvArchive &archive, byte type) {
        TileAtlas atlas; Common::Array<byte> game, range; MapAssetRange assets;
        if (!atlas.load(archive, type) || !archive.read("NA/GAME", game) || !resolveMapAssets(game, type, assets) ||
            !archive.readRange(assets.resource, assets.offset + assets.size + 44, 2, range)) return false;
        _atlas = atlas; _lightRange[0] = range[0]; _lightRange[1] = range[1]; _ready = true; return true;
    }
    bool draw(HgrFrame &frame, byte requested, byte terrain, byte lightMode, byte animation,
            uint cell, uint height) const {
        if (!_ready || lightMode > 3 || animation > 3) return false;
        byte tile = requested ? requested : terrain;
        if (lightMode >= 2 && tile >= _lightRange[0] && tile < _lightRange[1]) ++tile;
        return frame.drawCell(_atlas, tile, animation, cell, height);
    }
private:
    bool _ready; TileAtlas _atlas; byte _lightRange[2];
};
} }
#endif
