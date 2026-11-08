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
 * Room1600: port of the room handler FUN_00417da0. The original handles
 * no event besides the id and the descriptor: everything falls through to
 * the global handler.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** Room 1600 (original handler FUN_00417da0): static room, no logic of its own. */
class Room1600 : public Room {
public:
	Room1600();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;
};

/** Creates the room; it has no static objects. */
Room1600::Room1600() : Room(1600) {
}

/** Static descriptor DAT_00440190. */
const RoomDescriptor *Room1600::getDescriptor() {
	return findRoomData(_id);
}

/** Every event returns 0 (the original even has an explicit case 7 returning 0). */
int Room1600::handleEvent(int event, int param) {
	switch (event) {
	// Hotspot clicked: nothing room-specific, exits and comments come from the core
	case kEvHotspot:
		return 0;
	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom1600() {
	return new Room1600();
}

} // End of namespace Gooka
