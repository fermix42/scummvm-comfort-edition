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
 * Room2424: port of the room handler FUN_00425b30.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 2424 (original handler FUN_00425b30): coming from room 2415 the
 * hero drops in from the top of the screen.
 */
class Room2424 : public Room {
public:
	Room2424();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;
};

/** The room has no static objects. */
Room2424::Room2424() : Room(2424) {
}

/** Static descriptor 0x443548. */
const RoomDescriptor *Room2424::getDescriptor() {
	return findRoomData(_id);
}

/** Room events (FUN_00425b30, `what` >= 2). */
int Room2424::handleEvent(int event, int param) {
	// Enter from 2415: the hero falls from the top and lands on grid (80, 93)
	if (event == kEvEnter && param == 0x96f) {
		hero().place(0x50, 0x5d, 0);
		game()->enqueue({
			seqCall(kPrimShowAt, { &hero(), xy(262, 0), 0 }),
			seqCall(kPrimMoveAccel, { &hero(), 100, 0, 3, 39, 39, 1 }),
			seqCall(kPrimSound, { 3861 }),
			seqCall(kPrimPlaceActor, { &hero(), xy(80, 93), 0 | 119 << 16 }),
			seqCall(kPrimFace, { 0 }),
			seqEnd()
		});
	}
	return 0;
}

/** Factory used by the room registry. */
Room *createRoom2424() {
	return new Room2424();
}

} // End of namespace Gooka
