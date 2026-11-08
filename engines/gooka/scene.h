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
 * The scene: layered background plus the list of visible sprites
 * (original scene object DAT_00448428, renderer FUN_00408ed0).
 */

#ifndef GOOKA_SCENE_H
#define GOOKA_SCENE_H

#include "common/array.h"

namespace Graphics {
struct Surface;
}

namespace Gooka {

class Image;
class Sprite;

/**
 * Draws the background layers and the sprites in the original order:
 * sprites with priority < 0, then for every background frame L: frame L
 * followed by the sprites with priority == L, finally sprites with
 * priority >= number of frames (text, UI, cursor). Sprites are sorted by
 * (priority, subPriority), ties keep the order in which they were shown.
 */
class Scene {
public:
	Scene();
	~Scene();

	/** Sets the background image (all frames are layers); nullptr = none. */
	void setBackground(Image *background) { _background = background; }
	/** Current background image. */
	Image *getBackground() const { return _background; }
	/** Number of background layers (frames of the background image). */
	int getLayerCount() const;

	/** Adds a sprite (called by Sprite::show()). */
	void add(Sprite *sprite);
	/** Removes a sprite (called by Sprite::hide() and ~Sprite()). */
	void remove(Sprite *sprite);
	/** Visible sprites in insertion order. */
	const Common::Array<Sprite *> &getSprites() const { return _sprites; }

	/** Redraws the whole screen into `dst` (640x480, screen format). */
	void render(Graphics::Surface &dst);

private:
	/** Sort order of the renderer (comparator FUN_00408ea0 + stable insertion order). */
	static bool spriteLess(const Sprite *a, const Sprite *b);

	Image *_background;
	Common::Array<Sprite *> _sprites;
	uint32 _nextOrder; ///< counter for Sprite::_order
};

} // End of namespace Gooka

#endif // GOOKA_SCENE_H
