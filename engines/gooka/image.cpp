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
 * RLE image decoding and drawing (see image.h for the format).
 */

#include "common/debug.h"
#include "common/textconsole.h"

#include "graphics/surface.h"

#include "gooka/detection.h"
#include "gooka/image.h"

namespace Gooka {

/** Row terminator in the RLE stream. */
static const byte kEndOfRow = 0xFE;
/** Frame terminator in the RLE stream. */
static const byte kEndOfFrame = 0xFF;

Image::Image() : _data(nullptr), _size(0), _end16(0), _headerLen(0), _width(0), _height(0) {
}

Image::~Image() {
	free(_data);
}

// Reads the resource and records where every frame of the RGB555 section starts.
bool Image::load(Common::SeekableReadStream *stream) {
	free(_data);
	_data = nullptr;
	_frames.clear();

	_size = stream->size();
	if (_size < 28)
		return false;
	_data = (byte *)malloc(_size);
	if (!_data || stream->read(_data, _size) != _size)
		return false;

	_headerLen = READ_LE_UINT32(_data);
	const uint32 frameCount = READ_LE_UINT32(_data + 4);
	const uint32 prefixWords = READ_LE_UINT32(_data + 8);
	_width = READ_LE_UINT32(_data + 12);
	_height = READ_LE_UINT32(_data + 16);
	if (_headerLen + 4 > _size)
		return false;

	uint32 pos = _headerLen + 4;
	_end16 = MIN<uint32>(pos + READ_LE_UINT32(_data + _headerLen), _size);

	for (uint32 f = 0; f < frameCount; f++) {
		Frame frame;
		frame.dx = frame.dy = -1;
		if (prefixWords) {
			if (pos + 4 * prefixWords > _end16)
				return false;
			frame.dx = _data[pos];
			frame.dy = _data[pos + 2];
			pos += 4 * prefixWords;
		}
		frame.offset = pos;
		frame.width = 0;
		frame.rows = 0;

		// Skip the rows up to the frame terminator, measuring them on the way
		int row = 0, rowWidth = 0;
		while (pos < _end16) {
			const byte b = _data[pos++];
			if (b == kEndOfFrame)
				break;
			if (b == kEndOfRow) {
				row++;
				if (rowWidth) {
					frame.rows = row;
					frame.width = MAX<int>(frame.width, rowWidth);
				}
				rowWidth = 0;
				continue;
			}
			if (pos >= _end16)
				return false;
			rowWidth += b + _data[pos];
			pos += 1 + _data[pos] * 2;
		}
		if (pos > _end16)
			return false;
		_frames.push_back(frame);
	}

	debugC(3, kDebugGraphics, "Image: %dx%d, %d frames, header %d", _width, _height, frameCount, _headerLen);
	return true;
}

Common::Point Image::getFrameSize(uint frame) const {
	if (frame >= _frames.size())
		return Common::Point(0, 0);
	return Common::Point(_frames[frame].width, _frames[frame].rows);
}

// Returns the prefix hot spot of a frame.
Common::Point Image::getHotspot(uint frame) const {
	if (frame >= _frames.size())
		return Common::Point(-1, -1);
	return Common::Point(_frames[frame].dx, _frames[frame].dy);
}

// Decodes the RLE rows straight onto the surface; skipped pixels stay untouched.
void Image::draw(Graphics::Surface &dst, uint frame, int x, int y, const uint16 *lut, bool mirror) const {
	if (frame >= _frames.size())
		return;
	assert(dst.format.bytesPerPixel == 2);

	const byte *src = _data + _frames[frame].offset;
	const byte *end = _data + _end16;
	int row = 0;
	int col = 0;

	while (src < end) {
		const byte b = *src++;
		if (b == kEndOfFrame)
			break;
		if (b == kEndOfRow) {
			row++;
			col = 0;
			continue;
		}
		col += b;
		const int count = *src++;
		if (src + count * 2 > end)
			break;

		const int dy = y + row;
		if (dy >= 0 && dy < dst.h) {
			uint16 *line = (uint16 *)dst.getBasePtr(0, dy);
			for (int i = 0; i < count; i++) {
				const int sx = col + i;
				const int dx = mirror ? x + _width - 1 - sx : x + sx;
				if (dx >= 0 && dx < dst.w) {
					const uint16 c = READ_LE_UINT16(src + i * 2) & 0x7FFF;
					line[dx] = lut ? lut[c] : c;
				}
			}
		}
		src += count * 2;
		col += count;
	}
}

// Source rows and pixels advance an 8-bit accumulator by `scale`; the destination
// row/column only moves on carry, so later source pixels overwrite earlier ones.
void Image::drawScaled(Graphics::Surface &dst, uint frame, int x, int y, int scale, const uint16 *lut) const {
	if (scale >= 0x100) {
		draw(dst, frame, x, y, lut);
		return;
	}
	if (frame >= _frames.size() || scale <= 0)
		return;
	assert(dst.format.bytesPerPixel == 2);

	const byte *src = _data + _frames[frame].offset;
	const byte *end = _data + _end16;
	int dstRow = 0;
	int accY = 0;
	int dstCol = 0;
	int accX = 0x80;

	while (src < end) {
		const byte b = *src++;
		if (b == kEndOfFrame)
			break;
		if (b == kEndOfRow) {
			accY += scale;
			if (accY >= 0x100) {
				accY &= 0xFF;
				dstRow++;
			}
			dstCol = 0;
			accX = 0x80;
			continue;
		}

		// Transparent run
		accX += b * scale;
		dstCol += accX >> 8;
		accX &= 0xFF;

		const int count = *src++;
		if (src + count * 2 > end)
			break;

		const int dy = y + dstRow;
		uint16 *line = (dy >= 0 && dy < dst.h) ? (uint16 *)dst.getBasePtr(0, dy) : nullptr;
		for (int i = 0; i < count; i++) {
			const int dx = x + dstCol;
			if (line && dx >= 0 && dx < dst.w) {
				const uint16 c = READ_LE_UINT16(src + i * 2) & 0x7FFF;
				line[dx] = lut ? lut[c] : c;
			}
			accX += scale;
			dstCol += accX >> 8;
			accX &= 0xFF;
		}
		src += count * 2;
	}
}

// Walks the RLE stream to the requested row and checks whether (x, y) lies in a pixel run.
bool Image::isOpaque(uint frame, int x, int y) const {
	if (frame >= _frames.size() || x < 0 || y < 0 || x >= _width || y >= _height)
		return false;

	const byte *src = _data + _frames[frame].offset;
	const byte *end = _data + _end16;
	int row = 0;
	int col = 0;

	while (src < end) {
		const byte b = *src++;
		if (b == kEndOfFrame)
			break;
		if (b == kEndOfRow) {
			if (row == y)
				return false;
			row++;
			col = 0;
			continue;
		}
		col += b;
		const int count = *src++;
		if (row == y && x >= col && x < col + count)
			return true;
		src += count * 2;
		col += count;
	}
	return false;
}

} // End of namespace Gooka
