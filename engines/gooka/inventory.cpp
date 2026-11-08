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
 * The inventory bar (original class at vtable 0x4379a8, methods
 * 0x417120..0x4175b0, global object DAT_00478898).
 */

#include "graphics/surface.h"

#include "gooka/gooka.h"
#include "gooka/image.h"
#include "gooka/inventory.h"

namespace Gooka {

// FUN_00417620: bar at (0, 400) above the scene (priority 1000), all slots empty.
Inventory::Inventory() : scrollOffset(0), heldItem(0), heldSlot(-1),
		_leftPressed(false), _rightPressed(false), _menuPressed(false) {
	x = 0;
	y = kPlayAreaHeight;
	priority = 1000;
	subPriority = 0;
	clear();
}

// FUN_00417290: note the initial scroll position of 30 px.
void Inventory::clear() {
	for (int i = 0; i < kNumSlots; i++)
		slots[i] = 0;
	sparkle.reset();
	scrollOffset = 0x1e;
}

// FUN_00417360: searches outwards from the slot drawn 6th from the left,
// alternating forwards and backwards around the ring.
int Inventory::findFreeSlot() const {
	const int start = scrollOffset / kSlotWidth + 0x38;
	int back = start;
	for (int i = 0; i < kNumSlots; i++, back--) {
		const int forward = (i + start) % kNumSlots;
		if (slots[forward] == 0)
			return forward;
		if (slots[back % kNumSlots] == 0)
			return back % kNumSlots;
	}
	return -1;
}

// FUN_004172c0. The original writes out of range for slot -1 (consumeHeld
// without a held item overwrote the scene pointer); the port ignores it.
void Inventory::setSlot(int slot, int item) {
	if (slot >= 0 && slot < kNumSlots)
		slots[slot] = item;
}

// FUN_004173c0
void Inventory::add(int item) {
	const int slot = findFreeSlot();
	if (slot == -1)
		return;
	setSlot(slot, item);
	scrollToSlot(slot);
	sparkleSlot(slot);
}

// FUN_00417440
void Inventory::remove(int item) {
	for (int i = 0; i < kNumSlots; i++) {
		if (slots[i] == item) {
			slots[i] = 0;
			return;
		}
	}
}

// FUN_004174a0: the ring is scrolled to the slot before the sparkle is placed.
void Inventory::replace(int oldItem, int newItem, bool withSparkle) {
	for (int i = 0; i < kNumSlots; i++) {
		if (slots[i] == oldItem) {
			scrollToSlot(i);
			slots[i] = newItem;
			if (withSparkle)
				sparkleSlot(i);
			return;
		}
	}
}

// FUN_00417470
bool Inventory::has(int item) const {
	for (int i = 0; i < kNumSlots; i++) {
		if (slots[i] == item)
			return true;
	}
	return false;
}

// FUN_0041d230
void Inventory::dropHeld() {
	if (heldSlot != -1) {
		setSlot(heldSlot, heldItem);
		heldSlot = -1;
	}
}

// FUN_0041d260
void Inventory::consumeHeld() {
	setSlot(heldSlot, 0);
	heldSlot = -1;
}

// FUN_00417530: the ring never has gaps, so every x maps to a slot; the
// offset is taken from the slot's (wrapped) screen position.
int Inventory::slotAt(int px, int py, int *offsetX) const {
	if (py < y || px < 0 || px >= kScreenWidth)
		return -1;
	const int slot = ((px + scrollOffset) / kSlotWidth) % kNumSlots;
	if (offsetX)
		*offsetX = px - ((slot + 51) * kSlotWidth - scrollOffset) % kRingWidth + kSlotWidth;
	return slot;
}

// FUN_00417590
int Inventory::itemAt(int px) const {
	const int slot = ((px + scrollOffset) / kSlotWidth) % kNumSlots;
	return (slot >= 0 && slot < kNumSlots) ? slots[slot] : 0;
}

// FUN_00417210
void Inventory::setButtons(bool left, bool right, bool menu) {
	_leftPressed = left;
	_rightPressed = right;
	_menuPressed = menu;
}

// FUN_00417260
void Inventory::scrollBy(int delta) {
	scrollOffset = (scrollOffset + delta + kRingWidth) % kRingWidth;
}

// FUN_004172e0: a slot off to the right ends up at x = 590, one off to the
// left at x = 10, whichever way round the ring is shorter.
void Inventory::scrollToSlot(int slot) {
	const int pos = slot * kSlotWidth;
	const int ahead = (pos - scrollOffset + kRingWidth) % kRingWidth;
	const int behind = (scrollOffset - pos + kRingWidth) % kRingWidth;
	int newOffset;
	if (ahead < behind) {
		if (ahead < 0x24f)
			return;
		newOffset = pos - 0x24e;
	} else {
		newOffset = pos - 10;
	}
	scrollOffset = (newOffset + kRingWidth) % kRingWidth;
}

// The FUN_00412080 call shared by FUN_004173c0 / FUN_004174a0: 16 px spread,
// 30 ticks, white, centred on the slot, 5 above the bar's priority.
void Inventory::sparkleSlot(int slot) {
	const int sx = ((slot + kNumSlots) * kSlotWidth - scrollOffset) % kRingWidth + kSlotWidth / 2 + x;
	sparkle.start(sx, y + 40, priority + 5, 0x10, 0x1e, 0xFFFFFF);
}

// Deviation: the original saved the slots as they were, so saving while an
// item was held (its slot is -1 then) lost the item.
void Inventory::sync(Common::Serializer &s) {
	s.syncAsSint32LE(scrollOffset);
	for (int i = 0; i < kNumSlots; i++) {
		int item = slots[i];
		if (s.isSaving() && i == heldSlot && item == -1)
			item = heldItem;
		s.syncAsSint32LE(item);
		if (s.isLoading())
			slots[i] = item;
	}
	if (s.isLoading()) {
		scrollOffset = CLIP<int>(scrollOffset, 0, kRingWidth - 1);
		heldItem = 0;
		heldSlot = -1;
	}
}

// FUN_00417120
Common::Point Inventory::getSize() const {
	return Common::Point(kScreenWidth, kScreenHeight - kPlayAreaHeight);
}

// FUN_00417140: 14 slots starting with the one the scroll offset falls in.
void Inventory::draw(Graphics::Surface &dst) {
	const uint16 *lut = g_engine->_lut;
	Image *bar = g_engine->getImage(kBarImage);
	Image *icons = g_engine->getImage(kIconSheet);
	if (bar)
		bar->draw(dst, 0, x, y, lut);
	if (!icons)
		return;

	const int first = scrollOffset / kSlotWidth;
	for (int i = first; i < first + kVisibleSlots; i++) {
		const int item = slots[i % kNumSlots];
		if (item > 0)
			icons->draw(dst, item + kIconFrameBase, x + i * kSlotWidth - scrollOffset, y, lut);
	}
	icons->draw(dst, _menuPressed ? 6 : 5, x + kMenuButtonX, y + kMenuButtonY, lut);
}

} // End of namespace Gooka
