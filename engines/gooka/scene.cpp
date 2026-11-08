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
 * Sprite basics and the layered scene renderer (see sprite.h, scene.h).
 */

#include "common/algorithm.h"

#include "graphics/surface.h"

#include "gooka/gooka.h"
#include "gooka/image.h"
#include "gooka/scene.h"
#include "gooka/sprite.h"

namespace Gooka {

// ---------------------------------------------------------------------------
// Sprite
// ---------------------------------------------------------------------------

// Matches the original constructor FUN_0040ff60: frame 0, full scale, no sheet.
Sprite::Sprite() : x(0), y(0), priority(0), subPriority(0), frame(0), scale(kScaleFull),
		sheet(nullptr), user1(0), user2(0), _visible(false), _order(0) {
}

// A destroyed sprite must never stay in the scene list.
Sprite::~Sprite() {
	hide();
}

void Sprite::show() {
	if (!_visible && g_engine && g_engine->_scene)
		g_engine->_scene->add(this);
}

void Sprite::hide() {
	if (_visible && g_engine && g_engine->_scene)
		g_engine->_scene->remove(this);
}

// Visible frame size (frame record), shrunk like FUN_00409440: v = ((v * scale + 0x80) >> 8) + 1.
Common::Point Sprite::getSize() const {
	if (!sheet || frame < 0)
		return Common::Point(0, 0);
	Common::Point size = sheet->getFrameSize(frame);
	if (scale < kScaleFull) {
		size.x = ((size.x * scale + 0x80) >> 8) + 1;
		size.y = ((size.y * scale + 0x80) >> 8) + 1;
	}
	return size;
}

Common::Rect Sprite::getBounds() const {
	const Common::Point size = getSize();
	return Common::Rect(x, y, x + size.x, y + size.y);
}

// Default drawing: one frame of the sheet, scaled if needed (FUN_00408200).
void Sprite::draw(Graphics::Surface &dst) {
	if (!sheet || frame < 0)
		return;
	if (scale >= kScaleFull)
		sheet->draw(dst, frame, x, y, g_engine->_lut);
	else
		sheet->drawScaled(dst, frame, x, y, scale, g_engine->_lut);
}

// ---------------------------------------------------------------------------
// Scene
// ---------------------------------------------------------------------------

Scene::Scene() : _background(nullptr), _nextOrder(0) {
}

// Detaches the remaining sprites so their destructors do not touch the scene.
Scene::~Scene() {
	for (uint i = 0; i < _sprites.size(); i++)
		_sprites[i]->_visible = false;
}

int Scene::getLayerCount() const {
	return _background ? (int)_background->getFrameCount() : 0;
}

void Scene::add(Sprite *sprite) {
	sprite->_visible = true;
	sprite->_order = _nextOrder++;
	_sprites.push_back(sprite);
}

void Scene::remove(Sprite *sprite) {
	sprite->_visible = false;
	for (uint i = 0; i < _sprites.size(); i++) {
		if (_sprites[i] == sprite) {
			_sprites.remove_at(i);
			return;
		}
	}
}

// Priority first, then sub-priority, then the order in which the sprites were shown.
bool Scene::spriteLess(const Sprite *a, const Sprite *b) {
	if (a->priority != b->priority)
		return a->priority < b->priority;
	if (a->subPriority != b->subPriority)
		return a->subPriority < b->subPriority;
	return a->_order < b->_order;
}

// Full redraw every frame (the original only redrew dirty rectangles, with the same result).
void Scene::render(Graphics::Surface &dst) {
	dst.fillRect(Common::Rect(dst.w, dst.h), 0);

	Common::Array<Sprite *> sorted = _sprites;
	Common::sort(sorted.begin(), sorted.end(), spriteLess);

	const int layers = getLayerCount();
	uint next = 0;

	// Sprites behind the first layer (e.g. water seen through holes in frame 0)
	while (next < sorted.size() && sorted[next]->priority < 0)
		sorted[next++]->draw(dst);

	for (int layer = 0; layer < layers; layer++) {
		_background->draw(dst, layer, 0, 0, g_engine->_lut);
		while (next < sorted.size() && sorted[next]->priority == layer)
			sorted[next++]->draw(dst);
	}

	// Everything above the background: UI, texts, cursor
	while (next < sorted.size())
		sorted[next++]->draw(dst);
}

} // End of namespace Gooka
