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
 * Room 1209: street between the harbour (1200) and room 1217 (port of the
 * room handler FUN_004146e0).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** Room 1209 (original handler FUN_004146e0): only places one object of the room sheet. */
class Room1209 : public Room {
public:
	/** Constructs the room objects. */
	Room1209();
	/** Static descriptor 0x43efc0. */
	const RoomDescriptor *getDescriptor() override;
	/** Room entry. */
	int handleEvent(int event, int param) override;

private:
	Sprite _s475368; ///< 0x475368: decoration of the room sheet (frame 4)
};

Room1209::Room1209() : Room(1209) {
}

const RoomDescriptor *Room1209::getDescriptor() {
	return findRoomData(_id);
}

int Room1209::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: place the object (frame 4 of the room sheet)
	case kEvEnter:
		game()->placeObject(&_s475368, 400, 180, 0, 4, 0, -1);
		return 0;

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom1209() {
	return new Room1209();
}

} // End of namespace Gooka
