/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_STORY_H
#define NOX_NATIVE_WORLD_STORY_H
#include "nox/native_world.h"
#include "nox/native_story_patches.h"
namespace Nox { namespace Native {
// A logical48x48 loaded map window. No CPU memory is represented here.
// Full actor turns, cache-edge scrolling and normal player saves are separate.
class WorldStory {
public:
    WorldStory() : _ready(false), _type(0), _map(0), _center(0), _left(0), _top(0) {}
    bool initialize(const World &world, const byte *spawns, uint count) {
        if (_ready || !world.ready() || !spawns || count != 256) return false;
        const MapResource &resource = world._resources[world._states[world._current].family];
        const MapDefinition *definition = resource.find(world.mapId()); if (!definition) return false;
        const uint column = world.x() / 16, row = world.y() / 16;
        // Source-supported interior only; wrapped local/world cache edges need
        // their own scrolling contract instead of invented padding.
        if (definition->compressed && (!column || column >= 15 || !row || row >= 15)) return false;
        WorldStory next;
        next._type = world.mapType(); next._map = world.mapId();
        next._center = definition->compressed ? byte(row * 16 + column) : 17;
        next._left = definition->compressed ? byte((column - 1) * 16) : 0;
        next._top = definition->compressed ? byte((row - 1) * 16) : 0;
        for (uint slot = 0; slot < 9; ++slot) {
            Common::Array<byte> cells;
            const uint sector = definition->compressed ? (row - 1 + slot / 3) * 16 + column - 1 + slot % 3 : slot;
            if (!resource.sector(world.mapId(),sector,cells) || cells.size() != 256) return false;
            for (uint y = 0; y < 16; ++y)
                memcpy(next._state.terrain + (slot / 3 * 16 + y) * 48 + slot % 3 * 16,cells.data() + y * 16,16);
        }
        const byte *tables = world.currentTables()->data();
        memcpy(next._state.objects,tables + 1024,256); memcpy(next._state.actors,tables + 1280,256);
        memcpy(next._state.spawns,spawns,256);
        next._ready = true; *this = next; return true;
    }
    // Source movement recenters at local X8/40 or Y7/39, after the
    // position commit and before the ordered actor pass. Shift mutable overlap;
    // only the newly exposed three sectors come from the immutable HDV.
    // Local-map leave confirmation remains an explicit caller boundary.
    bool advanceWindow(World &world,byte direction,const StoryMapPatches &rules,bool &needsExit) {
        needsExit=false;if (!matchesWorld(world) || direction>3) return false;
        const byte x=byte(world.x()-_left),y=byte(world.y()-_top);
        const bool shift=direction==0?y==7:direction==1?y==39:direction==2?x==40:x==8;
        if (!shift) return true;
        const MapResource &resource=world._resources[world._states[world._current].family];
        const MapDefinition *definition=resource.find(world.mapId());if (!definition) return false;
        if (!definition->compressed || world.mapType()>=3) { needsExit=true;return true; }
        WorldStory next=*this;World candidate(world);
        const int dx=direction==2?16:direction==3?-16:0,dy=direction==1?16:direction==0?-16:0;
        next._left=byte(_left+dx);next._top=byte(_top+dy);
        next._center=byte(_center+(direction==0?-16:direction==1?16:direction==2?1:-1));
        for (uint row=0;row<48;++row) for (uint column=0;column<48;++column) {
            const int oldX=int(column)+dx,oldY=int(row)+dy;
            if (oldX>=0 && oldX<48 && oldY>=0 && oldY<48)
                next._state.terrain[row*48+column]=_state.terrain[oldY*48+oldX];
        }
        for (uint i=0;i<3;++i) {
            const uint slot=direction==0?i:direction==1?6+i:direction==2?2+i*3:i*3;
            const byte sector=byte(_center+(direction==0?-33+int(i):direction==1?31+int(i):direction==2?-14+int(i)*16:-18+int(i)*16));
            Common::Array<byte> cells;if (!resource.sector(world.mapId(),sector,cells) || cells.size()!=256) return false;
            for (uint row=0;row<16;++row)
                memcpy(next._state.terrain+(slot/3*16+row)*48+slot%3*16,cells.data()+row*16,16);
        }
        // Keep the working spawn metadata and live object/NPC records; authored
        // quest patches run after the new center is committed, as in M:FD00.
        if (!next.apply(candidate,rules)) return false;
        world.adoptState(candidate);*this=next;return true;
    }
    bool apply(World &world, const StoryMapPatches &rules) {
        if (!_ready || !world.ready() || world.mapType() != _type || world.mapId() != _map) return false;
        StoryMapState next = _state;
        const byte *tables = world._states[world._current].tables.data();
        memcpy(next.objects,tables + 1024,256); memcpy(next.actors,tables + 1280,256);
        if (!rules.apply(_type,_map,_center,world.questFields(),256,next)) return false;
        byte *owned = world._states[world._current].tables.data();
        memcpy(owned + 1024,next.objects,256); memcpy(owned + 1280,next.actors,256); _state = next; return true;
    }
    bool matchesWorld(const World &world) const {
        return _ready && world.ready() && world.mapType()==_type && world.mapId()==_map;
    }
    bool actorBounds(const World &world,byte *bounds) const {
        if (!bounds || !matchesWorld(world)) return false;
        // Source lower1B5B derives these byte-relative admission bounds from
        // the party's loaded-window coordinates. Raw neighbor access remains
        // separately bounded by terrainAt; this is not fabricated edge padding.
        bounds[0]=byte(_left-world.x()+127);bounds[1]=byte(_left-world.x()+176);
        bounds[2]=byte(_top-world.y()+127);bounds[3]=byte(_top-world.y()+176);return true;
    }
    bool terrainAt(byte x, byte y, byte &tile) const {
        const uint column = byte(x - _left), row = byte(y - _top);
        if (!_ready || column >= 48 || row >= 48) return false;
        tile = _state.terrain[row * 48 + column]; return true;
    }
    bool project(const World &world, Common::Array<byte> &output) const {
        if (!world.ready() || world.mapType() != _type || world.mapId() != _map) return false;
        Common::Array<byte> next; next.resize(187);
        for (uint row = 0; row < 11; ++row) for (uint column = 0; column < 17; ++column)
            if (!terrainAt(byte(int(world.x()) + int(column) - 8),byte(int(world.y()) + int(row) - 5),next[row * 17 + column])) return false;
        output = next; return true;
    }
    bool captureRecords(const World &world) {
        if (!_ready || !world.ready() || world.mapType() != _type || world.mapId() != _map) return false;
        const byte *tables = world.currentTables()->data();
        memcpy(_state.objects,tables + 1024,256); memcpy(_state.actors,tables + 1280,256); return true;
    }
    const StoryMapState *state() const { return _ready ? &_state : nullptr; }
    bool save(Common::WriteStream &output) const {
        if (!_ready) return false;
        output.writeUint32BE(MKTAG('N','S','W','N')); output.writeUint16LE(1);
        output.writeByte(_type); output.writeByte(_map); output.writeByte(_center); output.writeByte(_left); output.writeByte(_top);
        output.write(_state.terrain,2304); output.write(_state.objects,256); output.write(_state.actors,256); output.write(_state.spawns,256); return !output.err();
    }
    bool load(Common::SeekableReadStream &input, const World &world) {
        if (!world.ready() || input.size() - input.pos() != 3083 || input.readUint32BE() != MKTAG('N','S','W','N') || input.readUint16LE() != 1) return false;
        WorldStory next; next._type = input.readByte(); next._map = input.readByte(); next._center = input.readByte(); next._left = input.readByte(); next._top = input.readByte();
        if (next._type != world.mapType() || next._map != world.mapId() || (next._left & 15) || (next._top & 15) ||
            input.read(next._state.terrain,2304) != 2304 || input.read(next._state.objects,256) != 256 ||
            input.read(next._state.actors,256) != 256 || input.read(next._state.spawns,256) != 256 || input.err()) return false;
        const byte *tables = world.currentTables()->data();
        if (memcmp(next._state.objects,tables + 1024,256) || memcmp(next._state.actors,tables + 1280,256)) return false;
        next._ready = true; *this = next; return true;
    }
private:
    bool _ready;
    byte _type,_map,_center,_left,_top;
    StoryMapState _state;
};
} }
#endif
