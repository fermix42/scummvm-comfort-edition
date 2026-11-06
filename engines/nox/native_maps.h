/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_MAPS_H
#define NOX_NATIVE_MAPS_H

#include "common/array.h"
#include "nox/native_lz.h"
#include "common/str.h"
#include "common/endian.h"

namespace Nox {
namespace Native {

// The shared native packet decoder preserves the accepted sector format.
inline bool decodeMapSector(const byte *data, uint size, Common::Array<byte> &output) {
    return decodeReversePacket(data, size, 256, output);
}

struct MapDefinition {
	byte id;
	uint32 tiles, following;
	uint size;
	bool compressed;
	Common::Array<byte> metadata;
	MapDefinition() : id(0), tiles(0), following(0), size(0), compressed(false) {}
    bool dialogueContext(byte &output) const {
        if (metadata.size()<23 || metadata[21]<'0' || metadata[21]>'9') return false;
        const byte last=metadata[22];byte low;
        if (last>='0' && last<='9') low=last-'0';
        else if (last>='A' && last<='F') low=last-'A'+10;
        else return false;
        output=byte(((metadata[21]&15)<<4)|low);return true;
    }
};

class MapResource {
public:
	bool open(const Common::Array<byte> &data, bool surface, bool underworld) {
		if (data.size() < 3 || (surface ? (data[0] > 1 || data[1] != 1) : (data[0] != 0xea && data[0] != 0xaa)))
			return fail("Unsupported native map resource header");
		Common::Array<MapDefinition> maps;
		bool seen[256] = {};
		uint cursor = surface ? 2 : 1;
		for (;;) {
			if (cursor >= data.size() || data.size() - cursor < 2) return fail("Truncated native map directory");
			if (data[cursor] == 255) {
				if (data[cursor + 1]) return fail("Invalid native map terminator");
				cursor += 2; break;
			}
			const uint length = data[cursor] ? data[cursor] : 256;
			if (length < 23 || length > data.size() - cursor) return fail("Invalid native map metadata extent");
			const byte *header = data.data() + cursor;
			MapDefinition map;
			map.id = header[1]; map.tiles = read24(header + 2);
			map.size = READ_LE_UINT16(header + 5); map.following = read24(header + 7);
			map.compressed = surface || (underworld && map.id != 76 && map.id != 77);
			if (seen[map.id] || map.tiles > data.size() || map.size > data.size() - map.tiles ||
				map.following > data.size() || data.size() - map.following < 1792)
				return fail("Invalid native map data/state extent or duplicate ID");
			seen[map.id] = true;
			if (map.compressed) {
				if (map.tiles < 514) return fail("Missing native compressed sector directory");
				uint previous = 0;
				for (uint i = 0; i <= 256; ++i) {
					const uint address = sectorAddress(data, map, i);
					if ((i == 0 && address != 0x800) || (i && address <= previous) || address < 0x800 || address - 0x800 > map.size)
						return fail("Invalid native sector directory");
					previous = address;
				}
				if (previous - 0x800 != map.size) return fail("Native compressed map extent mismatch");
			} else if (map.size != 2304 || map.following != map.tiles + map.size) {
				return fail("Invalid native local map extent");
			}
			map.metadata.resize(length); memcpy(map.metadata.data(), header, length);
			maps.push_back(map); cursor += length;
		}
		if (maps.empty()) return fail("Empty native map directory");
		for (uint i = 0; i < maps.size(); ++i)
			if ((maps[i].compressed ? maps[i].tiles - 514 : maps[i].tiles) < cursor)
				return fail("Native map data overlaps directory");
		_data = data; _maps = maps; _error.clear(); return true;
	}
	const Common::Array<MapDefinition> &maps() const { return _maps; }
	const Common::String &error() const { return _error; }
	const MapDefinition *find(byte id) const {
		for (uint i = 0; i < _maps.size(); ++i) if (_maps[i].id == id) return &_maps[i];
		return nullptr;
	}
	bool sector(byte id, uint index, Common::Array<byte> &output) const {
		const MapDefinition *map = find(id);
		if (!map || index >= (map->compressed ? 256U : 9U)) return false;
		if (map->compressed) {
			const uint begin = sectorAddress(_data, *map, index) - 0x800;
			const uint end = sectorAddress(_data, *map, index + 1) - 0x800;
			return decodeMapSector(_data.data() + map->tiles + begin, end - begin, output);
		}
		Common::Array<byte> bytes; bytes.resize(256);
		memcpy(bytes.data(), _data.data() + map->tiles + index * 256, 256);
		output = bytes; return true;
	}
	bool tile(byte id, byte x, byte y, byte &output) const {
		const MapDefinition *map = find(id);
		if (!map || (!map->compressed && (x >= 48 || y >= 48))) return false;
		Common::Array<byte> cells;
		const uint index = (y / 16) * (map->compressed ? 16 : 3) + x / 16;
		if (!sector(id, index, cells)) return false;
		output = cells[(y % 16) * 16 + x % 16]; return true;
	}
	bool stateTables(byte id, Common::Array<byte> &output) const {
		const MapDefinition *map = find(id); if (!map) return false;
		Common::Array<byte> bytes; bytes.resize(1792);
		memcpy(bytes.data(), _data.data() + map->following, bytes.size());
		output = bytes; return true;
	}
    // Authored notice strings use relocated pointers in the original map data
    // format. Resolve them as resource offsets; no loaded-memory ABI is needed.
    bool notice(byte id,byte index,Common::Array<byte> &output) const {
        const MapDefinition *map=find(id);
        if (!map || map->metadata.size()<20) return false;
        const uint32 start=read24(map->metadata.data()+15);const uint length=READ_LE_UINT16(map->metadata.data()+18);
        if (length<2 || start>_data.size() || length>_data.size()-start) return false;
        const byte *data=_data.data()+start;const uint base=0x11a0,first=READ_LE_UINT16(data);
        if (first<base+2 || first-base>length || ((first-base)&1)) return false;
        const uint count=(first-base)/2;if (index>=count) return false;
        uint prior=first-base;
        for (uint i=0;i<count;++i) {
            const uint address=READ_LE_UINT16(data+2*i);
            if (address<base || address-base<prior || address-base>=length) return false;
            prior=address-base;
        }
        const uint begin=READ_LE_UINT16(data+2*index)-base;
        const uint limit=uint(index)+1<count?READ_LE_UINT16(data+2*(uint(index)+1))-base:length;
        uint end=begin;while (end<limit && data[end]) ++end;
        if (end==limit) return false;
        Common::Array<byte> next;next.resize(end-begin);if (!next.empty()) memcpy(next.data(),data+begin,next.size());output=next;return true;
    }
private:
	static uint32 read24(const byte *data) { return uint32(data[0]) | uint32(data[1]) << 8 | uint32(data[2]) << 16; }
	static uint sectorAddress(const Common::Array<byte> &data, const MapDefinition &map, uint index) {
		const uint offset = map.tiles - 514;
		return uint(data[offset + index]) | uint(data[offset + 257 + index]) << 8;
	}
	bool fail(const char *message) { _error = message; return false; }
	Common::Array<byte> _data;
	Common::Array<MapDefinition> _maps;
	Common::String _error;
};

} // namespace Native
} // namespace Nox
#endif
