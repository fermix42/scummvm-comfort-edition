/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_LIGHTING_H
#define NOX_NATIVE_LIGHTING_H
#include "nox/native_world.h"
#include "nox/native_world_story.h"
#include "nox/native_visibility.h"
namespace Nox { namespace Native {
// Owns the original map-family light ranges, exceptional map/coordinate-sector data and
// outer sampling geometry. Direct terrain reads replace the machine cache ABI.
class TerrainLighting {
public:
    TerrainLighting() : _ready(false), _type(255) {}
    bool load(HdvArchive &archive, byte type) {
        Common::Array<byte> game, program, ranges; MapAssetRange assets;
        if (!archive.read("NA/GAME", game) || !resolveMapAssets(game, type, assets) ||
            !archive.readRange(assets.resource, assets.offset + assets.size + 18, 4, ranges) ||
            !archive.read("NA/BS.AUX.RTN.BK2", program) || program.size() != 16787) return false;
        memcpy(_ranges, ranges.data(), sizeof(_ranges)); memcpy(_sectorMaps, game.data() + 0x965, sizeof(_sectorMaps));
        for (uint i = 0; i < 12; ++i) _samples[i] = READ_LE_UINT16(program.data() + 1 + 0x3d2 + i * 2);
        _type = type; _ready = true; return true;
    }
    bool apply(const World &world, byte sector, byte mode, VisibilityMask &mask, const WorldStory *window = nullptr) const {
        if (!_ready || !mask.ready() || !world.ready() || world.mapType() != _type || mode > 3) return false;
        if (mode == 1) return true;
        bool full = world.mapId() == 76 || world.mapId() == 77;
        for (uint i = 0; i < sizeof(_sectorMaps); i += 2)
            if (_sectorMaps[i] == sector && _sectorMaps[i + 1] == world.mapId()) full = true;
        if (full) { byte cells[187] = {}; return mask.importMask(cells, sizeof(cells)); }
        VisibilityMask next = mask; Common::Array<byte> terrain;
        if (window ? !window->project(world,terrain) : !world.projectTerrain(terrain)) return false;
        for (uint cell = 0; cell < 187; ++cell)
            if (source(terrain[cell], false) && !next.revealSource(cell % 17, cell / 17)) return false;
        for (uint line = 0; line < 12; ++line) {
            const int firstColumn = int(_samples[line] % 48) - 3;
            const int firstRow = int(_samples[line] / 48) - 3;
            for (uint sample = 0; sample < (line < 6 ? 17U : 11U); ++sample) {
                const int column = firstColumn + (line < 6 ? int(sample) : 0);
                const int row = firstRow + (line < 6 ? 0 : int(sample)); byte tile = 0;
                if (window ? !window->terrainAt(byte(int(world.x()) + column - 8),byte(int(world.y()) + row - 5),tile) :
                    !world.terrainAt(byte(int(world.x()) + column - 8),byte(int(world.y()) + row - 5),tile)) return false;
                if (source(tile, true) && !next.revealSource(column, row)) return false;
            }
        }
        mask = next; return true;
    }
private:
    bool source(byte tile, bool outer) const {
        if ((tile >= _ranges[0] && tile < _ranges[1]) || (tile >= _ranges[2] && tile < _ranges[3])) return true;
        if (_type == 7 && (tile == 223 || (!outer && (tile == 158 || tile == 159)))) return true;
        if (_type == 8 && tile == 222) return true;
        return !outer && _type == 2 && tile == 140;
    }
    bool _ready; byte _type, _ranges[4], _sectorMaps[100]; uint16 _samples[12];
};
// Commit the naturally resulting light state and mask only after all three
// original preparation phases succeed. No expected state is supplied here.
inline bool prepareSceneLighting(const World &world, const TerrainLighting &rules,
        byte sector, byte appearance, byte additionalTicks, LightState &state, VisibilityMask &mask, const WorldStory *window = nullptr) {
    VisibilityMask next = mask; LightState after;
    if (!next.prepareBaseLight(state, after) || !next.revealAdditional(after.mode, appearance, additionalTicks) ||
        !rules.apply(world, sector, after.mode, next, window)) return false;
    state = after; mask = next; return true;
}
} }
#endif
