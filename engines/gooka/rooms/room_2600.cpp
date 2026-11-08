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
 * Room 2600: the cave with the crashed spaceship (port of the room handler
 * FUN_004198e0). Pure data: the exits to 2424 and through the hatch into
 * the ship (2601) come from the static descriptor.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** Room 2600 (original handler FUN_004198e0): returns only its id and descriptor. */
class Room2600 : public Room {
public:
	Room2600();
	/** Static descriptor 0x440a90. */
	const RoomDescriptor *getDescriptor() override;
	/** The original handles no event: everything goes to the global fallback. */
	int handleEvent(int event, int param) override;
};

Room2600::Room2600() : Room(2600) {
}

const RoomDescriptor *Room2600::getDescriptor() {
	return findRoomData(_id);
}

int Room2600::handleEvent(int event, int param) {
	return 0;
}

/** Factory used by the room registry. */
Room *createRoom2600() {
	return new Room2600();
}

} // End of namespace Gooka
