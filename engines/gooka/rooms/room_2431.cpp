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
 * Room2431: port of the room handler FUN_00426330.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 2431 (original handler FUN_00426330): in front of the captain's
 * tomb. Its guardian mechanism (actor 0x98c) talks to the hero and sets
 * him a puzzle (room 2448); once it is solved (flag 0x991 cleared there)
 * the guardian and the door move away.
 */
class Room2431 : public Room {
public:
	Room2431();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;

private:
	Actor _n47b2d0;  ///< object at 0x47b2d0 (FUN_0040dc30()): the guardian mechanism
	Sprite _s47b4b0; ///< object at 0x47b4b0 (FUN_0040ff60()): the door
};

/** Constructs the room statics. */
Room2431::Room2431() : Room(2431) {
}

/** Static descriptor 0x443808. */
const RoomDescriptor *Room2431::getDescriptor() {
	return findRoomData(_id);
}

/** Room events (FUN_00426330, `what` >= 2). */
int Room2431::handleEvent(int event, int param) {
	switch (event) {
	// Enter: guardian and door while the puzzle is unsolved; back from 2448 both move up and away
	case kEvEnter:
		if (game()->flagTest(0x991)) {
			game()->placeObjectIfFlag(0x98c, &_n47b2d0, 0xe8, 0x3f, 0, 0, 0);
			if (game()->flagTest(0x980))
				game()->placeObjectHidden(&_s47b4b0, 0x126, 0xa8, 0, 3, 1);
			else
				game()->placeObject(&_s47b4b0, 0x126, 0xa8, 0, 3, 1, -1);
		}
		if (param == 0x990) {
			hero().place(0x4b, 0x50, 0x27);
			game()->placeObjectIfFlag(0x98c, &_n47b2d0, 0xe8, 0x3f, 0, 0, 0);
			game()->placeObject(&_s47b4b0, 0x126, 0xa8, 0, 3, 1, -1);
			game()->enqueue({
				seqYield(),
				seqSpawn(),
				seqCall(kPrimMoveLinear, { &_n47b2d0, 256, 0, 1, -1, 1 }),
				seqJoin(),
				seqCall(kPrimAmbient, { 3848 }),
				seqCall(kPrimMoveLinear, { &_s47b4b0, 256, 0, 1, -1, 1 }),
				seqCall(kPrimSound, { 3849 }),
				seqCall(kPrimAmbientStop),
				seqEnd()
			});
			return 0;
		}
		break;

	// Hotspot 0x98c: the guardian - dialogue, then the hero is let to the puzzle (room 2448)
	case kEvHotspot:
		if (param == 0x98c && game()->flagTest(0x991)) {
			game()->enqueue({
				seqCall(kPrimShow, { &_s47b4b0 }),
				seqCall(kPrimWalk, { xy(75, 80), -1 }),
				seqSpawn(),
				seqCall(kPrimWait, { 50 }),
				seqCall(kPrimWalk, { xy(90, 95), 39 }),
				seqJoin(),
				seqCall(kPrimSay, { &_n47b2d0, 2432, 1, 1, 2 }),
				seqWaitChildren(),
				seqCall(kPrimDialogue, { &_n47b2d0, 2433, 2442, 1, 1, 2 }),
				seqCall(kPrimSetFrame, { &_s47b4b0, 4 }),
				seqCall(kPrimSound, { 3862 }),
				seqCall(kPrimMoveLinear, { &_s47b4b0, 16, 0, 1, -1, 1 }),
				seqCall(kPrimWalk, { xy(90, 80), 170 }),
				seqCall(kPrimChangeRoom, { 2448 }),
				seqEnd()
			});
			return -1;
		}
		break;

	default:
		break;
	}
	return 0;
}

/** Factory used by the room registry. */
Room *createRoom2431() {
	return new Room2431();
}

} // End of namespace Gooka
