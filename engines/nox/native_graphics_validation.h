/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_GRAPHICS_VALIDATION_H
#define NOX_NATIVE_GRAPHICS_VALIDATION_H
#include "nox/native_graphics.h"
namespace Nox {
inline Common::String nativeGraphicsDigest(const byte *data, uint size) {
	Common::MemoryReadStream stream(data, size); return Common::computeStreamMD5AsString(stream);
}
inline bool readNativeGraphicsDigest(Common::SeekableReadStream &input, Common::String &output) {
	if (input.size() - input.pos() < 32) return false;
	Common::String next; for (uint i = 0; i < 32; ++i) next += char(input.readByte()); output = next; return true;
}
inline bool validateNativeGraphics(HdvArchive &archive, Common::SeekableReadStream &input,
		Common::String &report, Common::String &error) {
	if (input.readUint32BE() != MKTAG('N','G','F','X') || input.readUint32LE() != 1 || input.readUint32LE() != 8) return false;
	uint qualified = 0;
	for (uint i = 0; i < 8; ++i) {
		const byte kind = input.readByte(), historical = input.readByte(); Common::String expected;
		if (historical > 1 || !readNativeGraphicsDigest(input, expected)) return false;
		Native::TileAtlas atlas;
		if (!atlas.load(archive, kind) || nativeGraphicsDigest(atlas.data().data(), atlas.data().size()) != expected) {
			error = Common::String::format("Native original tile atlas differs for style%u", uint(kind)); return false;
		}
		qualified += historical;
	}
	for (byte variant = 1; variant <= 2; ++variant) {
		Native::GlyphFont font; Common::String expected;
		if (!readNativeGraphicsDigest(input, expected) || !font.load(archive, variant) || nativeGraphicsDigest(font.data().data(), font.data().size()) != expected || font.glyph(31) || !font.glyph(32) || !font.glyph(127)) {
			error = "Native original glyph font differs"; return false;
		}
	}
	byte original[8192]; Common::String expectedRGB, expectedPlane;
	if (input.read(original, sizeof(original)) != sizeof(original) || !readNativeGraphicsDigest(input, expectedRGB) || !readNativeGraphicsDigest(input, expectedPlane)) return false;
	const uint count = input.readUint16LE();
	if (count != 187 || input.size() - input.pos() != int64(count) * 4) return false;
	struct Cell { uint index; byte tile, frame; };
	Common::Array<Cell> cells;
	Native::HgrFrame frame; if (!frame.importImage(original, sizeof(original))) return false;
	for (uint i = 0; i < count; ++i) {
		Cell cell; cell.index = input.readUint16LE(); cell.tile = input.readByte(); cell.frame = input.readByte(); cells.push_back(cell);
		if (!frame.clearCell(cell.index)) return false;
	}
	Common::Array<byte> rgb; frame.renderRGB(rgb);
	if (nativeGraphicsDigest(rgb.data(), rgb.size()) == expectedRGB) { error = "Controlled clear did not change visible pixels before native blits"; return false; }
	Native::TileAtlas atlas; if (!atlas.load(archive, 8)) return false;
	for (uint i = 0; i < cells.size(); ++i)
		if (!frame.drawCell(atlas, cells[i].tile, cells[i].frame, cells[i].index)) return false;
	if (memcmp(frame.data(), original, sizeof(original)) || nativeGraphicsDigest(frame.data(), sizeof(original)) != expectedPlane) {
		error = "Native cell blits differ from actual historical display plane"; return false;
	}
	frame.renderRGB(rgb);
	if (nativeGraphicsDigest(rgb.data(), rgb.size()) != expectedRGB) { error = "Native HGR rendering differs from actual historical CE screenshot pixels"; return false; }
	if (frame.drawCell(atlas, 0, 0, 187) || frame.clearCell(187) || memcmp(frame.data(), original, sizeof(original))) { error = "Native out-of-range cell changed display data"; return false; }
	if (input.err() || input.pos() != input.size() || qualified != 7) return false;
	report += Common::String::format("native-graphics\tatlas_styles=8\thistorical_styles=%u\tfonts=2\tcells=%u\tfull_plane=1\tactual_screenshot_pixels=215040\tvisible_clear=1\tatomic_cell_refusal=1\townership=controlled-render-units\n", qualified, count);
	return true;
}
}
#endif
