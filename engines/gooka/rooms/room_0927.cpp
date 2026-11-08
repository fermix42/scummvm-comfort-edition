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
 * Room 927: the porch in front of the house with the judge's two guards
 * (port of the room handler FUN_00429180).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 927 (original handler FUN_00429180): while flag 0x34f is set (the
 * sister has not been taken away yet) two guards block the way out
 * (hotspot 0xcab = room 3243).
 */
class Room0927 : public Room {
public:
	/** Room id 927; the objects are set up when the room is entered. */
	Room0927();
	/** Static descriptor 0x4449a0. */
	const RoomDescriptor *getDescriptor() override;
	/** Enter and the way out. */
	int handleEvent(int event, int param) override;

private:
	Actor _actLeftGuard; ///< 0x47c068: left guard
	Actor _actRightGuard; ///< 0x47bc20: right guard
};

Room0927::Room0927() : Room(927) {
}

const RoomDescriptor *Room0927::getDescriptor() {
	return findRoomData(_id);
}

int Room0927::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: the guards stand at the gate while the house is closed
	case kEvEnter:
		if (game()->flagTest(0x34f)) {
			game()->placeObject(&_actLeftGuard, 160, 100, 1, 0, 0, -1);
			game()->placeObject(&_actRightGuard, 323, 100, 1, 4, 1, -1);
		}
		return 0;

	// Hotspot clicked
	case kEvHotspot:
		// Hotspot 0xcab: the way out - the guards block it; later the hero doubts once where to go
		if (param == 0xcab) {
			if (game()->flagTest(0x34f)) {
				game()->enqueue({
					seqCall(kPrimWalk, { xy(83, 95), -1 }),
					seqSpawn(),
					seqCall(kPrimWait, { 3 }),
					seqCall(kPrimAnimate, { &_actLeftGuard, 3, 2 }),
					seqJoin(),
					seqSpawn(),
					seqCall(kPrimAnimate, { &_actRightGuard, 7, 2 }),
					seqCall(kPrimSound, { 930 }),
					seqJoin(),
					seqCall(kPrimWalk, { xy(83, 92), 39 }),
					seqCall(kPrimSay, { &hero(), 929, 0, 0x68, 4 }),
					seqCall(kPrimWalk, { xy(83, 95), 0 }),
					seqSpawn(),
					seqCall(kPrimAnimate, { &_actLeftGuard, 0, 5 }),
					seqJoin(),
					seqCall(kPrimAnimate, { &_actRightGuard, 4, 7 }),
					seqCall(kPrimSay, { &_actLeftGuard, 928, 0, 8, 1 }),
					seqEnd()
				});
				return 1;
			}
			if (game()->flagTest(0x399) && game()->flagTest(0x3a3)) {
				game()->sayIdle(0x3a3);
				return 1;
			}
		}
		return 0;

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom0927() {
	return new Room0927();
}

} // End of namespace Gooka
