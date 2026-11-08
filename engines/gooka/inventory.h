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
 * The inventory: 50 item slots shown in the wooden bar at the bottom of
 * the screen (original object DAT_00478898, bar drawing FUN_00417140).
 */

#ifndef GOOKA_INVENTORY_H
#define GOOKA_INVENTORY_H

#include "common/serializer.h"

#include "gooka/effects.h"
#include "gooka/sprite.h"

namespace Gooka {

/**
 * Item slots plus the bar sprite (image 409, item icons = frame itemId + 10
 * of sheet 407, 50-px slots, 14 visible, ring scrolling) and the item held
 * on the cursor. Item id 0 = empty slot, -1 = the slot of the held item
 * while the Ui carries it around.
 */
class Inventory : public Sprite {
public:
	static const int kNumSlots = 50;

	Inventory();

	int slots[kNumSlots]; ///< +0x30 item ids
	int scrollOffset;     ///< +0x104 ring scroll position in pixels
	SparkleSprite sparkle; ///< +0x108 effect when an item is added/replaced
	int heldItem;         ///< DAT_00478714 item on the cursor, 0 = none
	int heldSlot;         ///< DAT_00478718 slot it came from, -1 = none

	/** Empties all slots and resets the scroll position (FUN_00417290). */
	void clear();
	/** First empty slot, -1 if full (FUN_00417360). */
	int findFreeSlot() const;
	/** Stores an item in a slot (FUN_004172c0). */
	void setSlot(int slot, int item);
	/** Adds an item to the first free slot with a sparkle (FUN_004173c0). */
	void add(int item);
	/** Removes an item (FUN_00417440). */
	void remove(int item);
	/** Replaces an item in place, optionally with a sparkle (FUN_004174a0). */
	void replace(int oldItem, int newItem, bool withSparkle);
	/** True if the item is in a slot (FUN_00417470). */
	bool has(int item) const;

	/** Puts the held item back into its slot (FUN_0041d230). */
	void dropHeld();
	/** The held item is used up: its slot stays empty (FUN_0041d260). */
	void consumeHeld();

	/**
	 * Slot under a screen position in the bar, -1 if none (FUN_00417530).
	 * @param offsetX  optional: receives x relative to the slot's left edge
	 */
	int slotAt(int x, int y, int *offsetX = nullptr) const;

	/**
	 * Saves or loads slots and scroll position. A held item is saved back in
	 * its slot; after loading nothing is held.
	 */
	void sync(Common::Serializer &s);

	/** The bar is always 640x80 (FUN_00417120). */
	Common::Point getSize() const override;
	/** Bar, visible item icons and the menu button (FUN_00417140). */
	void draw(Graphics::Surface &dst) override;

private:
	friend class Ui;

	/** Layout of the bar. */
	enum {
		kSlotWidth = 50,
		kRingWidth = kNumSlots * kSlotWidth, ///< 2500 px
		kVisibleSlots = 14,
		kMenuButtonX = 620,                  ///< 0x26c
		kMenuButtonY = 3,
		kIconFrameBase = 10,                 ///< icon = frame item + 10 of sheet 407
		kBarImage = 409,
		kIconSheet = 407
	};

	/** Remembers the pressed state of the scroll edges and the menu button (FUN_00417210). */
	void setButtons(bool left, bool right, bool menu);
	/** Scrolls the ring by `delta` pixels (FUN_00417260). */
	void scrollBy(int delta);
	/** Scrolls just enough to make a slot visible (FUN_004172e0). */
	void scrollToSlot(int slot);
	/** Item in the slot under screen x, 0 if none (FUN_00417590). */
	int itemAt(int x) const;
	/** Sparkle burst over a slot (the FUN_00412080 call of FUN_004173c0 / FUN_004174a0). */
	void sparkleSlot(int slot);

	bool _leftPressed;  ///< +0xf8 left scroll edge held
	bool _rightPressed; ///< +0xfc right scroll edge held
	bool _menuPressed;  ///< +0x100 menu button held: drawn with frame 6 instead of 5
};

} // End of namespace Gooka

#endif // GOOKA_INVENTORY_H
