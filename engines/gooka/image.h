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
 * Decoder and blitter for the RLE picture/animation resources (type 32751).
 */

#ifndef GOOKA_IMAGE_H
#define GOOKA_IMAGE_H

#include "common/array.h"
#include "common/rect.h"
#include "common/stream.h"

namespace Graphics {
struct Surface;
}

namespace Gooka {

/**
 * One picture or animation (N frames of the same size).
 *
 * Layout: u32 headerLen (24 = picture, 27 = sprite), u32 frames,
 * u32 prefixWords, u32 width, u32 height, u32 transparent, [3 unknown bytes],
 * u32 size16, then the RGB555 section and an 8-bit section (ignored here).
 * Each frame: [dx, pad, dy, pad] if prefixWords == 1, then rows of
 * (skip, count, pixels[count])* terminated by 0xFE; 0xFF ends the frame.
 * Trailing empty rows may be omitted.
 *
 * Frames are kept RLE-encoded and decoded while drawing.
 */
class Image {
public:
	Image();
	~Image();

	/** Reads and indexes the whole resource; returns false on malformed data. */
	bool load(Common::SeekableReadStream *stream);

	/** Number of frames in the animation (1 for plain pictures). */
	uint getFrameCount() const { return _frames.size(); }
	/** Frame width in pixels (all frames share it). */
	int getWidth() const { return _width; }
	/** Frame height in pixels (all frames share it). */
	int getHeight() const { return _height; }
	/** True for transparent sprites (header length 27), false for pictures. */
	bool isSprite() const { return _headerLen == 27; }

	/**
	 * Visible size of a frame like the original frame record (FUN_00407f00):
	 * x = widest row (skips + pixels), y = rows up to the last non-empty one.
	 */
	Common::Point getFrameSize(uint frame) const;

	/** Per-frame hot spot from the frame prefix, (-1, -1) if the image has none. */
	Common::Point getHotspot(uint frame) const;

	/**
	 * Draws a frame with its top-left corner at (x, y), clipped to the surface.
	 * @param lut     converts RGB555 to the 16-bit surface format (nullptr = copy as is)
	 * @param mirror  flip the frame horizontally
	 */
	void draw(Graphics::Surface &dst, uint frame, int x, int y, const uint16 *lut, bool mirror = false) const;

	/**
	 * Draws a frame shrunk to `scale`/256 (nearest neighbour, like the original
	 * blitter FUN_0042d879); scale >= 256 draws unscaled.
	 */
	void drawScaled(Graphics::Surface &dst, uint frame, int x, int y, int scale, const uint16 *lut) const;

	/** True if the frame has an opaque pixel at (x, y), relative to its top-left corner. */
	bool isOpaque(uint frame, int x, int y) const;

private:
	/** Location of one frame inside _data. */
	struct Frame {
		uint32 offset; ///< start of the row data (after the prefix)
		int16 dx, dy;  ///< hot spot from the prefix, -1 if none
		int16 width;   ///< widest row in pixels (frame record +0)
		int16 rows;    ///< last non-empty row + 1 (frame record +4)
	};

	byte *_data;      ///< whole resource
	uint32 _size;     ///< size of _data in bytes
	uint32 _end16;    ///< end of the RGB555 section inside _data
	uint32 _headerLen;
	int _width, _height;
	Common::Array<Frame> _frames;
};

} // End of namespace Gooka

#endif // GOOKA_IMAGE_H
