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
 * Room 2613: T-junction of the spaceship maze with the locked door to the
 * computer room (port of the room handler FUN_0041b1f0). The code lock
 * next to the door is shown in close-up in room 2615; once the code is
 * right (flag 0xa3a cleared there) the door slides open and the hero
 * walks through to room 2619.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Hotspot table 0x4416a8 of descriptor 0x4417f8, indexed by the slot of
 * the hotspot map: slot 4 is empty, slot 5 is the code lock 0xa37. The
 * generated descriptor stops at the first -1 and loses slot 5.
 */
static const int kRoom2613Hotspots[] = {
	0x40a44, 0x10a43, 0x30a42, -1, 0xa37, -1, -1, -1, -1, -1, -1, -1
};

/**
 * Room 2613 (original handler FUN_0041b1f0): the sliding door of the
 * computer room and the code lock hotspot.
 */
class Room2613 : public Room {
public:
	Room2613();
	/** Static descriptor 0x4417f8 with the complete hotspot table. */
	const RoomDescriptor *getDescriptor() override;
	/** Enter (door, back from the lock or the computer room) and the lock hotspot. */
	int handleEvent(int event, int param) override;

private:
	Sprite _s476a00;      ///< sliding door of the right archway (sheet 2614)
	RoomDescriptor _desc; ///< copy of the generated descriptor with kRoom2613Hotspots
};

Room2613::Room2613() : Room(2613) {
	_desc = *findRoomData(_id);
	_desc.hotspots = kRoom2613Hotspots;
}

const RoomDescriptor *Room2613::getDescriptor() {
	return &_desc;
}

int Room2613::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: door shut; back from the solved lock it opens and the hero goes to 2619; back from 2619 it closes behind him
	case kEvEnter:
		game()->placeObject(&_s476a00, 0x16a, 0x4d, 0, 0, 0, -1);
		if (param == 0xa37) {
			hero().show();
			if (!game()->flagTest(0xa3a)) {
				// Code accepted: the door opens (the flag is set again, the next visit needs the code again)
				game()->flagSet(0xa3a, true);
				hero().place(0x5b, 0x2c, 0x27);
				game()->enqueue({
					seqYield(),
					seqSpawn(),
					seqCall(kPrimWait, { 40 }),
					seqCall(kPrimWalk, { xy(0x62, 0x27), -1 }),
					seqJoin(),
					seqCall(kPrimAmbient, { 0xf08 }),
					seqCall(kPrimMoveLinear, { &_s476a00, 100, 0, 1, -1, 1 }),
					seqCall(kPrimSound, { 0xf09 }),
					seqCall(kPrimAmbientStop),
					seqWaitChildren(),
					seqCall(kPrimChangeRoom, { 0xa3b }),
					seqEnd()
				});
			} else {
				hero().place(0x5b, 0x2c, 0);
			}
		}
		if (param == 0xa3b) {
			hero().place(0x62, 0x27, 0);
			_s476a00.x = 0x16a;
			_s476a00.y = -0x17;
			_s476a00.priority = 0;
			_s476a00.subPriority = 0;
			game()->enqueue({
				seqSpawn(),
				seqCall(kPrimWait, { 20 }),
				seqCall(kPrimAmbient, { 0xf08 }),
				seqCall(kPrimMoveLinear, { &_s476a00, 50, 0, 1, 2, 1 }),
				seqCall(kPrimSound, { 0xf09 }),
				seqCall(kPrimAmbientStop),
				seqJoin(),
				seqCall(kPrimWalk, { xy(0x62, 0x2c), 0 }),
				seqWaitChildren(),
				seqEnd()
			});
			return 0;
		}
		return 0;

	// Hotspot clicked
	case kEvHotspot:
		// Hotspot 0xa37: code lock - walk up to it and look at it closely (room 2615)
		if (param == 0xa37) {
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x5b, 0x2c), 0xa8 }),
				seqCall(kPrimChangeRoom, { 0xa37 }),
				seqEnd()
			});
		}
		return 0;

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom2613() {
	return new Room2613();
}

} // End of namespace Gooka
