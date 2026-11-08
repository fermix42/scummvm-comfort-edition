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
 * Room 2601: the entrance corridor of the crashed spaceship with the lift
 * (port of the room handler FUN_00419930). The lift button (hotspot 0xa32)
 * opens the sliding door and takes the hero into the corridor maze (2640).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 2601 (original handler FUN_00419930): the lift door sprite, the
 * arrival by lift from room 2640 and the ride down.
 */
class Room2601 : public Room {
public:
	Room2601();
	/** Static descriptor 0x440ba8. */
	const RoomDescriptor *getDescriptor() override;
	/** Enter (door, arrival by lift) and the lift button. */
	int handleEvent(int event, int param) override;

private:
	Sprite _s476a48; ///< sliding lift door (sheet 2602)
};

Room2601::Room2601() : Room(2601) {
}

const RoomDescriptor *Room2601::getDescriptor() {
	return findRoomData(_id);
}

int Room2601::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: lift door shut; arriving by lift from 2640 the hero steps out and the door closes
	case kEvEnter:
		if (param != 0xa50) {
			game()->placeObject(&_s476a48, 0xd5, 0x24, 0, 0, 0, -1);
			return 0;
		}
		game()->placeObject(&_s476a48, 0xd5, -0x108, 0, 0, 0, -1);
		hero().place(0x48, 0x3d, 0);
		game()->enqueue({
			seqSpawn(),
			seqCall(kPrimWait, { 40 }),
			seqCall(kPrimAmbient, { 0xf08 }),
			seqCall(kPrimMoveLinear, { &_s476a48, 100, 0, 1, 3, 1 }),
			seqJoin(),
			seqCall(kPrimWalk, { xy(0x48, 0x4c), 0 }),
			seqWaitChildren(),
			seqCall(kPrimSound, { 0xf09 }),
			seqCall(kPrimAmbientStop),
			seqEnd()
		});
		return 0;

	// Hotspot clicked
	case kEvHotspot:
		// Hotspot 0xa32: lift button - the lift stands at floor 420, the door opens, the hero rides into the maze (2640)
		if (param != 0xa32)
			return 0;
		game()->var(0xa32) = 0x1a4;
		game()->enqueue({
			seqCall(kPrimWalk, { xy(0x3c, 0x4a), 0xaa }),
			seqCall(kPrimSoundWait, { 0xee3 }),
			seqSpawn(),
			seqCall(kPrimAmbient, { 0xf08 }),
			seqCall(kPrimMoveLinear, { &_s476a48, 120, 0, 1, -2, 1 }),
			seqJoin(),
			seqCall(kPrimWait, { 30 }),
			seqCall(kPrimWalk, { xy(0x48, 0x3d), -1 }),
			seqCall(kPrimSound, { 0xf09 }),
			seqCall(kPrimAmbientStop),
			seqCall(kPrimChangeRoom, { 0xa50 }),
			seqEnd()
		});
		return 0;

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom2601() {
	return new Room2601();
}

} // End of namespace Gooka
