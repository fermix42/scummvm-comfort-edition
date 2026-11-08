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
 * Room 1215: street of Avaral with four locked doors, leading to the
 * palace gate (1223) (port of the room handler FUN_00415130).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 1215 (original handler FUN_00415130): the locked doors
 * 0x1b4..0x1b7 that the lockpick (item 2) cannot open.
 */
class Room1215 : public Room {
public:
	/** Constructs the room (no static objects). */
	Room1215();
	/** Static descriptor 0x43f4f0. */
	const RoomDescriptor *getDescriptor() override;
	/** The doors and the lockpick actions. */
	int handleEvent(int event, int param) override;

private:
	/** Walks to a door, plays `wav` and comments with `line` (the shared tail at 0x415404). */
	void enqueueDoor(int pos, int dir, int wav, int line);
};

Room1215::Room1215() : Room(1215) {
}

const RoomDescriptor *Room1215::getDescriptor() {
	return findRoomData(_id);
}

void Room1215::enqueueDoor(int pos, int dir, int wav, int line) {
	game()->enqueue({
		seqCall(kPrimWalk, { pos, dir }),
		seqCall(kPrimSoundWait, { wav }),
		seqCall(kPrimFace, { 0 }),
		seqCall(kPrimDialogue, { 0, line, line, 0, 0, 0 }),
		seqEnd()
	});
}

int Room1215::handleEvent(int event, int param) {
	switch (event) {
	// Hotspot clicked
	case kEvHotspot:
		switch (param) {
		// Hotspot 0x1b4: locked door - rattle it ("the door is locked")
		case 0x1b4:
			enqueueDoor(xy(0x0c, 0x53), 0x5b, 0x323, 0x4c2);
			return 0;

		// Hotspot 0x1b5: locked door - rattle it
		case 0x1b5:
			enqueueDoor(xy(0x2f, 0x42), 0x5b, 0x323, 0x4c2);
			return 0;

		// Hotspot 0x1b6: locked door - rattle it
		case 0x1b6:
			enqueueDoor(xy(0x60, 0x45), 0x63, 0x323, 0x4c2);
			return 0;

		// Hotspot 0x1b7: locked door - rattle it
		case 0x1b7:
			enqueueDoor(xy(0x9f, 0x5b), 0xa5, 0x323, 0x4c2);
			return 0;

		default:
			return 0;
		}

	// Held item over a hotspot: the lockpick (item 2) on the doors
	case kEvItemOnHotspot:
		switch (param) {
		case 0x201b4:
			return 0x1bb;
		case 0x201b5:
			return 0x1bc;
		case 0x201b6:
			return 0x1bd;
		case 0x201b7:
			return 0x1be;
		default:
			return 0;
		}

	// Action executed
	case kEvAction:
		switch (param) {
		// Lockpick on door 0x1b4: does not work
		case 0x1bb:
			enqueueDoor(xy(0x0c, 0x53), 0x5b, 0x324, 0x4c3);
			return 0;

		// Lockpick on door 0x1b5: does not work
		case 0x1bc:
			enqueueDoor(xy(0x2f, 0x42), 0x5b, 0x324, 0x4c3);
			return 0;

		// Lockpick on door 0x1b6: does not work
		case 0x1bd:
			enqueueDoor(xy(0x60, 0x45), 0x63, 0x324, 0x4c3);
			return 0;

		// Lockpick on door 0x1b7: does not work
		case 0x1be:
			enqueueDoor(xy(0x9f, 0x5b), 0xa5, 0x324, 0x4c3);
			return 0;

		default:
			return 0;
		}

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom1215() {
	return new Room1215();
}

} // End of namespace Gooka
