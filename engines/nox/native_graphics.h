/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_GRAPHICS_H
#define NOX_NATIVE_GRAPHICS_H

#include "nox/resources.h"

namespace Nox {
namespace Native {

struct MapAssetRange { Common::String resource; uint32 offset; uint size; };
inline bool resolveMapAssets(const Common::Array<byte> &game, byte type, MapAssetRange &output) {
	static const char *families[] = { "SURF", nullptr, "UM", "RUIN", "UM", "TOWN2", "TOWN", "CSTL", "KEEP" };
	if (game.size() != 13824 || type >= ARRAYSIZE(families) || !families[type]) return false;
	const byte *entry = game.data() + 0xc80 + type * 5;
	MapAssetRange next; next.resource = Common::String("NA/DATA.MAP.") + families[type];
	next.offset = uint32(entry[0]) | uint32(entry[1]) << 8 | uint32(entry[2]) << 16;
	next.size = READ_LE_UINT16(entry + 3);
	if (next.size != 20480) return false;
	output = next; return true;
}

class TileAtlas {
public:
	bool load(HdvArchive &archive, byte type) {
		Common::Array<byte> game, atlas;
		MapAssetRange range;
		if (!archive.read("NA/GAME", game) || !resolveMapAssets(game, type, range)) return false;
		// The original GAME data directory has nine five-byte resource extents.
		// Resolve the user's bytes directly; bootstrap execution is unnecessary.
		if (!archive.readRange(range.resource, range.offset, range.size, atlas)) return false;
		uint16 offsets[256]; byte frames[256];
		for (uint tile = 0; tile < 256; ++tile) {
			// Legacy asset addresses are an on-disk encoding, converted once to
			// relative offsets. They never become execution or CPU-bank state.
			const uint encoded = uint(game[0x30e + tile]) | uint(game[0x40e + tile]) << 8;
			if (encoded < 0x7000 || encoded >= 0xc000) return false;
			offsets[tile] = encoded - 0x7000; frames[tile] = encoded >= 0x8000 ? 4 : 1;
			if (uint(offsets[tile]) + uint(frames[tile]) * 32 > atlas.size()) return false;
		}
		_atlas = atlas; memcpy(_offsets, offsets, sizeof(offsets)); memcpy(_frames, frames, sizeof(frames)); return true;
	}
	const Common::Array<byte> &data() const { return _atlas; }
	bool animated(byte id) const { return _atlas.size()==20480 && _frames[id]==4; }
	const byte *tile(byte id, byte frame) const {
		if (_atlas.size() != 20480 || frame > 3) return nullptr;
		return _atlas.data() + _offsets[id] + (_frames[id] == 4 ? frame * 32 : 0);
	}
private:
	Common::Array<byte> _atlas;
	uint16 _offsets[256]; byte _frames[256];
};

class GlyphFont {
public:
	bool load(HdvArchive &archive, byte variant) {
		if (variant < 1 || variant > 2) return false;
		Common::Array<byte> data;
		if (!archive.read(variant == 1 ? "NA/FONT1" : "NA/FONT2", data) || data.size() != 768) return false;
		_data = data; return true;
	}
	const byte *glyph(byte character) const {
		const uint value = character & 127;
		return _data.size() == 768 && value >= 32 ? _data.data() + (value - 32) * 8 : nullptr;
	}
	const Common::Array<byte> &data() const { return _data; }
private:
	Common::Array<byte> _data;
};

// Native ownership of the original HGR image format. No CPU, bank switches,
// registers, instruction addresses or Runtime object participate in drawing.
// Other display modes and full scene/visibility/UI composition remain separate.
class HgrFrame {
public:
	HgrFrame() { memset(_bytes, 0, sizeof(_bytes)); }
	bool importImage(const byte *data, uint size) {
		if (!data || size != sizeof(_bytes)) return false;
		memcpy(_bytes, data, sizeof(_bytes)); return true;
	}
	const byte *data() const { return _bytes; }
	bool putByte(uint column,uint row,byte value) {
		if (column>=40 || row>=192) return false;
		_bytes[rowOffset(row)+column]=value;return true;
	}
	static uint rowOffset(uint row) { return (row & 7) * 1024 + ((row >> 3) & 7) * 128 + (row >> 6) * 40; }
	bool clearCell(uint index) {
		if (index >= 187) return false;
		for (uint row = 0; row < 16; ++row) {
			const uint offset = rowOffset(8 + (index / 17) * 16 + row) + (index % 17) * 2;
			_bytes[offset] = _bytes[offset + 1] = 0;
		}
		return true;
	}
	bool drawCell(const TileAtlas &atlas, byte id, byte frame, uint index, uint height = 16) {
		const byte *bitmap = atlas.tile(id, frame);
		if (!bitmap || index >= 187 || !height || height > 16) return false;
		for (uint row = 0; row < height; ++row) {
			const uint offset = rowOffset(8 + (index / 17) * 16 + row) + (index % 17) * 2;
			_bytes[offset] = bitmap[row * 2]; _bytes[offset + 1] = bitmap[row * 2 + 1];
		}
		return true;
	}
    bool clearTextSpan(uint column, uint textRow, uint width) {
        if (column >= 40 || textRow >= 24 || !width || column + width > 40) return false;
        for (uint row = 0; row < 8; ++row) memset(_bytes + rowOffset(textRow * 8 + row) + column,0,width);
        return true;
    }
    bool scrollTextRows(uint column,uint firstRow,uint endRow,uint width) {
        if (!width || column+width>40 || firstRow>=endRow || endRow>24) return false;
        for (uint y=firstRow*8;y<(endRow-1)*8;++y)
            memcpy(_bytes+rowOffset(y)+column,_bytes+rowOffset(y+8)+column,width);
        return clearTextSpan(column,endRow-1,width);
    }
    bool drawGlyph(const GlyphFont &font, byte character, uint column, uint textRow, byte inverse) {
        const byte *bitmap = font.glyph(character);
        if (!bitmap || column >= 40 || textRow >= 24) return false;
        for (uint row = 0; row < 8; ++row) _bytes[rowOffset(textRow * 8 + row) + column] = bitmap[row] ^ inverse;
        return true;
    }
	void renderRGB(Common::Array<byte> &output) const {
		output.resize(560 * 384 * 3);
		for (uint row = 0; row < 192; ++row) {
			byte bits[280], phase[280]; const uint offset = rowOffset(row);
			for (uint column = 0; column < 40; ++column)
				for (uint bit = 0; bit < 7; ++bit) { const byte value = _bytes[offset + column]; bits[column * 7 + bit] = (value >> bit) & 1; phase[column * 7 + bit] = value >> 7; }
			byte *line = output.data() + row * 2 * 560 * 3;
			for (uint x = 0; x < 280; ++x) {
				uint32 color = 0;
				if (bits[x]) {
					if ((x && bits[x - 1]) || (x < 279 && bits[x + 1])) color = 0xffffff;
					else color = phase[x] ? (x & 1 ? 0xff6a3c : 0x14cffd) : (x & 1 ? 0x14f53c : 0xff44fd);
				}
				for (uint copy = 0; copy < 2; ++copy) {
					byte *pixel = line + (x * 2 + copy) * 3; pixel[0] = color >> 16; pixel[1] = color >> 8; pixel[2] = color;
				}
			}
			memcpy(line + 560 * 3, line, 560 * 3);
		}
	}
private:
	byte _bytes[8192];
};

} // namespace Native
} // namespace Nox
#endif
