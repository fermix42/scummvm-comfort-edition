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
 * Room3200: port of the room handler FUN_00429a70.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 3201 (original handler FUN_00429a70): the encyclopedia notebook,
 * the close-up opened by looking at item 0xf (fallback: changeRoom(0xc81)).
 * The handler's room id is 0xc81 = 3201; its descriptor (0x444c10) uses
 * background 3200. The pages (strings 3202 + 2 * page, two columns) are
 * drawn by a CounterSprite; the open page is kept in var 0xc80 and the
 * room to return to in var 0xc81.
 */
class Room3200 : public Room {
public:
	Room3200();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;

private:
	CounterSprite _c47c7c8; ///< object at 0x47c7c8 (FUN_0042a830(0xc82, 10)): the pages 0..10
};

/** Constructs the room statics; the id is the one the original returns for `what == 0`. */
Room3200::Room3200() : Room(0xc81), _c47c7c8(3202, 10) {
}

/** Static descriptor 0x444c10 (background 3200). */
const RoomDescriptor *Room3200::getDescriptor() {
	return findRoomData(3200);
}

/** Room events (FUN_00429a70, `what` >= 2). */
int Room3200::handleEvent(int event, int param) {
	// DAT_0047c7f8: the page before this event
	const int page = _c47c7c8.frame;

	switch (event) {
	// Enter: hide the hero, show the page kept in var 0xc80, remember where we came from
	case kEvEnter:
		hero().hide();
		game()->setRectHotspot(&_c47c7c8, 0, 0, 0, 0, -1);
		_c47c7c8.frame = game()->var(0xc80);
		// (the original also marks the sprite dirty, +0x20 = -1; the scene redraws anyway)
		if (param != 0xc81 && param != -1) {
			game()->var(0xc81) = param;
			return 0;
		}
		break;

	// Leave: keep the page, show the hero again
	case kEvLeave:
		game()->var(0xc80) = page;
		hero().show();
		return 0;

	// Hotspot clicked; any click keeps the open page in var 0xc80
	case kEvHotspot:
		if (param == 0xc97) {
			// Hotspot 0xc97: close the book, back to the room it was opened in
			game()->changeRoom(game()->var(0xc81));
		} else if (param == 0xc98) {
			// Hotspot 0xc98: previous page
			_c47c7c8.decrement();
		} else if (param == 0xc99) {
			// Hotspot 0xc99: next page
			_c47c7c8.increment();
		}
		game()->var(0xc80) = _c47c7c8.frame;
		return 0;

	// Opening the book from itself and looking at items do nothing here
	case kEvBeforeItemRoom:
	case kEvLookItem:
		return 1;

	default:
		break;
	}
	return 0;
}

/** Factory used by the room registry. */
Room *createRoom3200() {
	return new Room3200();
}

} // End of namespace Gooka
