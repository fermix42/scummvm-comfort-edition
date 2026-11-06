/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_VISIBILITY_H
#define NOX_NATIVE_VISIBILITY_H
#include "nox/resources.h"
namespace Nox { namespace Native {
struct LightState {
    byte mode, level, transitionTicks;
    LightState(byte m = 0, byte l = 0, byte t = 0) : mode(m), level(l), transitionTicks(t) {}
};
// Native visibility mask and the original data-defined reveal kernel. The
// complete light/terrain/actor scheduler selects reveal sources separately.
class VisibilityMask {
public:
    VisibilityMask() : _ready(false) { memset(_cells, 0, sizeof(_cells)); }
    bool ready() const { return _ready; }
    bool loadRules(HdvArchive &archive) {
        Common::Array<byte> data;
        if (!archive.read("NA/BS.AUX.RTN.BK2", data) || data.size() != 16787) return false;
        // Resource has a one-byte file prefix before its data/code image.
        Common::Array<byte> masks[5];
        for (uint i = 0; i < 5; ++i) {
            const uint encoded = uint(data[1 + 0x9c + i]) | uint(data[1 + 0xa1 + i]) << 8;
            if (encoded < 0xd0a6 || encoded >= 0xd0f1) return false;
            uint cursor = encoded - 0xd000 + 1; masks[i].resize(187); memset(masks[i].data(), 1, 187);
            for (;;) {
                if (cursor >= 1 + 0xf1) return false;
                const byte first = data[cursor++]; if (first == 255) break;
                if (cursor >= 1 + 0xf1) return false;
                const byte end = data[cursor++]; if (first >= end || end > 187) return false;
                for (uint cell = first; cell < end; ++cell) masks[i][cell] = 0;
            }
        }
        Common::Array<byte> loader;
        if (!archive.read("NA/LOADER.P", loader) || loader.size() != 10514) return false;
        for (uint i = 0; i < 90; ++i) if (loader[0x25e0 + i] >= 187) return false;
        memcpy(_offsets, data.data() + 1 + 0x37d, 37);
        memcpy(_boundaries, data.data() + 1 + 0x3a2, 11);
        memcpy(_rowSteps, data.data() + 1 + 0x3ad, 37);
        memcpy(_additional, loader.data() + 0x25e0, sizeof(_additional));
        for (uint i = 0; i < 5; ++i) _masks[i] = masks[i];
        _ready = true; return true;
    }
    bool importMask(const byte *cells, uint size) {
        if (!cells || size != sizeof(_cells)) return false;
        memcpy(_cells, cells, sizeof(_cells)); return true;
    }
    const byte *cells() const { return _cells; }
    bool revealAdditional(byte mode, byte appearance, byte lightTicks) {
        if (!_ready || mode > 3) return false;
        if (mode == 1 || (appearance != 119 && !lightTicks)) return true;
        const uint count = lightTicks & 128 ? 90 : 40;
        for (uint i = 0; i < count; ++i) _cells[_additional[i]] = 0;
        return true;
    }
    bool prepareBaseLight(const LightState &before, LightState &output) {
        if (!_ready || before.mode > 3 || before.level > 4) return false;
        LightState after = before; byte selected = before.level;
        switch (before.mode) {
        case 0:
            if (before.level == 4) after.mode = 1;
            else if (before.transitionTicks >= 2) { ++after.level; after.transitionTicks = 0; if (!after.level) after.mode = 1; }
            break;
        case 1: output = after; return true;
        case 2:
            if (!before.level) after.mode = 3;
            else if (before.transitionTicks >= 2) {
                --after.level; after.transitionTicks = 0;
                if (!after.level) after.mode = 3;
            }
            break;
        case 3: selected = 0; break;
        }
        ++after.transitionTicks;
        memcpy(_cells, _masks[selected].data(), sizeof(_cells)); output = after; return true;
    }
    bool revealSource(int column, int row) {
        if (!_ready || column < -128 || column > 127 || row < -128 || row > 127) return false;
        byte next[187]; memcpy(next, _cells, sizeof(next));
        const byte originColumn = byte(204 + row * 33 + column);
        byte scanRow = byte(row + 3);
        for (uint i = 0; i < 37; ++i) {
            const byte scanColumn = i < 19 ? byte(originColumn - _offsets[i]) : byte(originColumn + _offsets[i]);
            if (scanRow >= 6 && scanRow < 17) {
                const byte relativeRow = scanRow - 6;
                if ((scanRow == 15 && scanColumn < 6) ||
                    (scanColumn >= _boundaries[relativeRow] && scanColumn - _boundaries[relativeRow] < 17)) {
                    const byte cell = byte(scanColumn - 204 - byte(relativeRow << 4));
                    if (cell >= sizeof(next)) return false;
                    next[cell] = 0;
                }
            }
            scanRow = byte(scanRow + _rowSteps[i]);
        }
        memcpy(_cells, next, sizeof(_cells)); return true;
    }
private:
    bool _ready;
    byte _cells[187], _offsets[37], _boundaries[11], _rowSteps[37];
    byte _additional[90];
    Common::Array<byte> _masks[5];
};
} }
#endif
