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
 * Room 1383: the room with the stygra (port of the room handler
 * FUN_00417060).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 1383 (original handler FUN_00417060). The room is pure data: the
 * exit to 1211 and the stygra (item 63) are handled by the generic exit and
 * pick-up logic of the descriptor. The original returns the room id and
 * descriptor for `what` 0/1 and 0 for every other event.
 */
class Room1383 : public Room {
public:
	/** Room id 1383 (the room has no objects of its own). */
	Room1383();
	/** Static descriptor 0x43fde0. */
	const RoomDescriptor *getDescriptor() override;
	/** No event is handled; everything goes to the global fallback. */
	int handleEvent(int event, int param) override;
};

Room1383::Room1383() : Room(1383) {
}

const RoomDescriptor *Room1383::getDescriptor() {
	return findRoomData(_id);
}

int Room1383::handleEvent(int event, int param) {
	return 0;
}

/** Factory used by the room registry. */
Room *createRoom1383() {
	return new Room1383();
}

} // End of namespace Gooka
