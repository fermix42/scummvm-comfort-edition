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
 * Room2444: port of the room handler FUN_00426a40.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 2444 (original handler FUN_00426a40): inside the captain's tomb.
 * The mummy (hotspot 0x98e) holds the lost shem (item 0x43).
 */
class Room2444 : public Room {
public:
	Room2444();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;
};

/** The room has no static objects. */
Room2444::Room2444() : Room(2444) {
}

/** Static descriptor 0x4439a8. */
const RoomDescriptor *Room2444::getDescriptor() {
	return findRoomData(_id);
}

/** Room events (FUN_00426a40, `what` >= 2). */
int Room2444::handleEvent(int event, int param) {
	// Hotspot 0x98e: the captain's mummy - comment and take the shem (item 0x43), once (line 0x98d)
	if (event == kEvHotspot && param == 0x98e && game()->flagTest(0x98d)) {
		game()->enqueue({
			seqCall(kPrimDialogue, { 0, 3582, 3582, 0, 0, 0 }),
			seqCall(kPrimWalk, { xy(97, 99), 34 }),
			seqCall(kPrimDialogue, { 0, 2445, 2445, 0, 0, 0 }),
			seqCall(kPrimFace, { 90 }),
			seqCall(kPrimWait, { 12 }),
			seqCall(kPrimAddItem, { 67 }),
			seqCall(kPrimFace, { 0 }),
			seqEnd()
		});
		return 1;
	}
	return 0;
}

/** Factory used by the room registry. */
Room *createRoom2444() {
	return new Room2444();
}

} // End of namespace Gooka
