/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_OCCLUSION_H
#define NOX_NATIVE_OCCLUSION_H
#include "nox/native_graphics.h"
#include "common/util.h"
#include "nox/native_visibility.h"
namespace Nox { namespace Native {
enum CellPaintKind { kCellClear, kCellBackground };
struct CellPaint {
    CellPaintKind kind; byte cell;
    CellPaint(CellPaintKind k, byte c) : kind(k), cell(c) {}
};
struct OcclusionContext {
    byte terrain[187],objects[187],records[256],previous[187];
    byte direction; bool presentation,ignoreBlockers;
};
class OcclusionRules {
public:
    OcclusionRules() : _ready(false), _type(255) {}
    bool load(HdvArchive &archive, byte type) {
        Common::Array<byte> game,footer; MapAssetRange range;
        if (!archive.read("NA/GAME",game) || !resolveMapAssets(game,type,range) ||
            !archive.readRange(range.resource,range.offset + range.size,64,footer)) return false;
        bool seen[187] = {};
        for (uint i = 0; i < 134; ++i) {
            const uint cell = game[0x7fa + i];
            if (cell >= 187 || seen[cell] || !(cell % 17) || cell % 17 == 16 || !(cell / 17) || cell / 17 == 10 || cell == 93) return false;
            seen[cell] = true; _order[i] = byte(cell);
        }
        memcpy(_terrainRanges,footer.data() + 14,4); memcpy(_objectRanges,footer.data() + 54,4);
        _type = type; _ready = true; return true;
    }
    bool opaque(byte terrain, bool object, byte tile, byte state, uint cell, bool &output) const {
        if (!_ready || cell >= 187) return false;
        bool result = in(terrain,_terrainRanges) || in(terrain,_terrainRanges + 2) || (_type == 7 && (terrain == 97 || terrain == 98));
        if (!result && object && (state == 33 || (state >= 16 && state < 32))) {
            if (!(_type == 3 && (tile == 91 || tile == 92)) && !in(tile,_objectRanges + 2))
                result = !in(tile,_objectRanges) || (cell != 76 && cell != 110 && cell != 94 && cell != 92);
        }
        output = result; return true;
    }
    // Source casts diagonal rays outside the central row/column and widening
    // cardinal wedges on them. Only the authored interior scan domain casts.
    static bool shadow(uint origin, byte *mask, const byte *previous, Common::Array<CellPaint> &actions) {
        const int row = origin / 17, column = origin % 17;
        if (!mask || !previous || origin >= 187 || row <= 0 || row >= 10 || column <= 0 || column >= 16 || origin == 93) return false;
        const auto hide = [&](int r, int c) {
            const uint cell = uint(r * 17 + c); mask[cell] = 1;
            if (previous[cell] != 1) actions.push_back(CellPaint(kCellClear,byte(cell)));
        };
        if (column == 8) {
            const int step = row < 5 ? -1 : 1;
            for (int r = row + step,distance = 1; r >= 0 && r <= 10; r += step,++distance)
                for (int c = MAX(0,column - distance); c <= MIN(16,column + distance); ++c) hide(r,c);
        } else if (row == 5) {
            const int step = column < 8 ? -1 : 1;
            for (int c = column + step,distance = 1; c >= 0 && c <= 16; c += step,++distance)
                for (int r = MAX(0,row - distance); r <= MIN(10,row + distance); ++r) hide(r,c);
        } else {
            const int dr = row < 5 ? -1 : 1, dc = column < 8 ? -1 : 1;
            for (int r = row + dr,c = column + dc; r >= 0 && r <= 10 && c >= 0 && c <= 16; r += dr,c += dc) hide(r,c);
        }
        return true;
    }
    bool apply(const OcclusionContext &input, VisibilityMask &visibility, Common::Array<CellPaint> &output) const {
        if (!_ready || !visibility.ready() || input.direction > 4) return false;
        byte mask[187]; memcpy(mask,visibility.cells(),187); Common::Array<CellPaint> actions;
        const auto reveal = [&](uint cell) {
            if (!mask[cell] && input.previous[cell]) actions.push_back(CellPaint(kCellBackground,byte(cell)));
        };
        for (uint i = 0; i < 187; ++i)
            if (mask[i] == 1 && input.previous[i] != 1) actions.push_back(CellPaint(kCellClear,byte(i)));
        for (uint i = 0; i < 134; ++i) {
            const uint cell = _order[i]; reveal(cell);
            if (input.ignoreBlockers) continue;
            const uint slot = input.objects[cell]; bool blocked;
            if (slot != 255 && (slot > 252 || (slot & 3))) return false;
            if (!opaque(input.terrain[cell],slot != 255,slot == 255 ? 0 : input.records[slot + 2],slot == 255 ? 0 : input.records[slot + 3],cell,blocked)) return false;
            if (blocked && !shadow(cell,mask,input.previous,actions)) return false;
        }
        if (!input.presentation) {
            const byte direction = input.direction;
            if (direction == 0 || direction == 1 || direction == 4) {
                // The source left edge lags one row behind its right edge.
                for (uint row = 0; row < 11; ++row) { reveal((row ? row - 1 : 0) * 17); reveal(row * 17 + 16); }
                if (direction == 0 || direction == 4) for (uint cell = 170; cell <= 186; ++cell) reveal(cell);
                // Preserve the source's inclusive extra first cell of row1.
                if (direction == 1 || direction == 4) for (uint cell = 0; cell < 18; ++cell) reveal(cell);
            }
            if (direction == 2 || direction == 3 || direction == 4) {
                for (uint column = 0; column < 17; ++column) { reveal(column); reveal(170 + column); }
                if (direction == 2 || direction == 4) for (uint row = 0; row < 11; ++row) reveal(row * 17);
                if (direction == 3 || direction == 4) for (uint row = 0; row < 11; ++row) reveal(row * 17 + 16);
            }
        }
        if (!visibility.importMask(mask,187)) return false;
        output = actions; return true;
    }
private:
    static bool in(byte tile, const byte *range) { return tile >= range[0] && tile < range[1]; }
    bool _ready; byte _type,_terrainRanges[4],_objectRanges[4],_order[134];
};
} }
#endif
