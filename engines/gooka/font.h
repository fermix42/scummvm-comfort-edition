/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 * The game's bitmap fonts (type 32753) as a Graphics::Font.
 */

#ifndef GOOKA_FONT_H
#define GOOKA_FONT_H

#include "common/stream.h"

#include "graphics/font.h"

namespace Gooka {

/**
 * 8x16 monochrome bitmap font indexed by CP1250 code (256 glyphs x 16 bytes,
 * bit 7 = leftmost pixel). Proportional widths are derived from the bitmaps
 * exactly like the original loader (FUN_004096f0) does.
 */
class GookaFont : public Graphics::Font {
public:
	GookaFont();

	/** Reads the 4096-byte glyph table and computes the glyph widths. */
	bool load(Common::SeekableReadStream *stream);

	int getFontHeight() const override { return 16; }
	int getMaxCharWidth() const override { return 9; }
	int getCharWidth(uint32 chr) const override;
	void drawChar(Graphics::Surface *dst, uint32 chr, int x, int y, uint32 color) const override;

private:
	byte _glyphs[256][16]; ///< one byte per glyph row
	byte _widths[256];     ///< advance width of each glyph in pixels
};

} // End of namespace Gooka

#endif // GOOKA_FONT_H
