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
 * Room2078: port of the room handler FUN_004212b0.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** Room 2078 (original handler FUN_004212b0); the objects come from the exit records. */
class Room2078 : public Room {
public:
	/** Room 2078 (no static objects). */
	Room2078();
	/** Static descriptor DAT_004423a8. */
	const RoomDescriptor *getDescriptor() override;
	/** The room body (what >= 2). */
	int handleEvent(int event, int param) override;
};

Room2078::Room2078() : Room(2078) {
}

const RoomDescriptor *Room2078::getDescriptor() {
	return findRoomData(_id);
}

int Room2078::handleEvent(int event, int param) {
	switch (event) {
	// Hotspot clicked
	case kEvHotspot:
		// Hotspot 0x820: find item 0x30 there (once), comment (line 2081)
		if (param == 0x820 && game()->flagTestAndClear(0x30)) {
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x33, 0x59), 0x76 }),
				seqCall(kPrimWait, { 0xa }),
				seqCall(kPrimDialogue, { 0, 0x821, 0x821, 0, 0, 0 }),
				seqCall(kPrimAddItem, { 0x30 }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
		}
		return 0;

	default:
		break;
	}
	return 0;
}

/** Creates the handler (registered in rooms/ported.cpp). */
Room *createRoom2078() {
	return new Room2078();
}

} // End of namespace Gooka
