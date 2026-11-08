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
 * Sprites: everything drawn on the screen except the background layers
 * (room objects, actors, text, cursor, inventory bar).
 */

#ifndef GOOKA_SPRITE_H
#define GOOKA_SPRITE_H

#include "common/rect.h"

namespace Graphics {
struct Surface;
}

namespace Gooka {

class Image;

/** Scale value meaning 100 % (the blitter only scales down). */
enum {
	kScaleFull = 0x100
};

/**
 * Plain sprite (original class at vtable 0x437930, 0x44 bytes).
 *
 * The fields are public on purpose: the original room code reads and writes
 * them directly, so the ported rooms do the same. Field comments give the
 * original offsets to make porting from the decompiled code mechanical.
 */
class Sprite {
public:
	Sprite();
	virtual ~Sprite();

	int x;           ///< +0x04 left edge in screen pixels
	int y;           ///< +0x08 top edge in screen pixels
	int priority;    ///< +0x0c background layer after which it is drawn (-1 = behind layer 0, >= layers = on top)
	int subPriority; ///< +0x10 tie-break between equal priorities
	int frame;       ///< +0x30 frame of the sheet, -1 = draw nothing
	int scale;       ///< +0x34 kScaleFull = 100 %, smaller values shrink
	Image *sheet;    ///< +0x38 image the frames come from (not owned)
	int user1;       ///< +0x3c free for room code
	int user2;       ///< +0x40 free for room code

	/** Sets the frame (FUN_00409530). */
	void setFrame(int f) { frame = f; }
	/** Sets the scale (FUN_00409550). */
	void setScale(int s) { scale = s; }

	/** Adds the sprite to the scene (FUN_004089f0); no-op if already shown. */
	void show();
	/** Removes the sprite from the scene (FUN_004089d0). */
	void hide();
	/** True while the sprite is part of the scene. */
	bool isVisible() const { return _visible; }

	/** Size of the current frame after scaling (FUN_00409440 rounding). */
	virtual Common::Point getSize() const;
	/** Screen rectangle covered by the sprite. */
	Common::Rect getBounds() const;

	/** Draws the sprite; the default draws `frame` of `sheet` at (x, y) with `scale`. */
	virtual void draw(Graphics::Surface &dst);

private:
	friend class Scene;
	bool _visible;
	uint32 _order; ///< insertion order in the scene (stable sorting)
};

} // End of namespace Gooka

#endif // GOOKA_SPRITE_H
