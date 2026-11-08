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
 * Bitmap font loading and glyph drawing (see font.h).
 */

#include "graphics/surface.h"

#include "gooka/font.h"

namespace Gooka {

GookaFont::GookaFont() {
	memset(_glyphs, 0, sizeof(_glyphs));
	memset(_widths, 1, sizeof(_widths));
}

// Loads the glyphs; width = rightmost used column + 1 px spacing, or an explicit
// width mask stored in the last row when its bit 7 is set.
bool GookaFont::load(Common::SeekableReadStream *stream) {
	if (stream->read(_glyphs, sizeof(_glyphs)) != sizeof(_glyphs))
		return false;

	for (int c = 0; c < 256; c++) {
		byte mask = 0;
		if (_glyphs[c][15] & 0x80) {
			mask = _glyphs[c][15];
			_glyphs[c][15] = 0;
		} else {
			for (int r = 0; r < 16; r++)
				mask |= _glyphs[c][r];
		}

		int bit = 0;
		while (bit < 8 && !(mask & (1 << bit)))
			bit++;
		_widths[c] = MAX(9 - bit, 1);
	}
	return true;
}

// Advance width of a CP1250 character.
int GookaFont::getCharWidth(uint32 chr) const {
	return _widths[chr & 0xFF];
}

// Plots the set bits of the glyph in the given color, clipped to the surface.
void GookaFont::drawChar(Graphics::Surface *dst, uint32 chr, int x, int y, uint32 color) const {
	const byte *glyph = _glyphs[chr & 0xFF];
	for (int r = 0; r < 16; r++) {
		const int dy = y + r;
		if (dy < 0 || dy >= dst->h)
			continue;
		const byte bits = glyph[r];
		for (int b = 0; b < 8; b++) {
			const int dx = x + b;
			if ((bits & (0x80 >> b)) && dx >= 0 && dx < dst->w)
				dst->setPixel(dx, dy, color);
		}
	}
}

} // End of namespace Gooka
