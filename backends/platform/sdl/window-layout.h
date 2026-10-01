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

#ifndef BACKENDS_PLATFORM_SDL_WINDOW_LAYOUT_H
#define BACKENDS_PLATFORM_SDL_WINDOW_LAYOUT_H

#include "common/util.h"

namespace WindowLayout {

enum Mode { kNormal, kLeftTwoThirds, kCustom };

// All values are in SDL desktop coordinates, not drawable pixels. Keep these
// as ints: virtual desktops can extend beyond Common::Rect's int16 range.
struct Rect {
	int x, y, w, h;
};

inline Rect outerRect(const Rect &work, int mode, int width, int height, int x, int y) {
	Rect result = work;
	if (mode == kLeftTwoThirds) {
		result.w = MAX(1, work.w / 3 * 2 + work.w % 3 * 2 / 3);
	} else if (mode == kCustom) {
		// Zero size means use all available space on that axis. Clamp the
		// position after the size, keeping the entire window on the display.
		result.w = width > 0 ? CLIP(width, MIN(320, work.w), work.w) : work.w;
		result.h = height > 0 ? CLIP(height, MIN(200, work.h), work.h) : work.h;
		result.x += CLIP(x, 0, work.w - result.w);
		result.y += CLIP(y, 0, work.h - result.h);
	}
	return result;
}

inline Rect clientRect(const Rect &outer, int top, int left, int bottom, int right) {
	Rect result = {outer.x + left, outer.y + top,
	              MAX(1, outer.w - left - right), MAX(1, outer.h - top - bottom)};
	return result;
}

} // namespace WindowLayout

#endif
