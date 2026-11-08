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
 * Room 1213: street of Avaral with water and three locked doors (port of
 * the room handler FUN_00414ca0).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 1213 (original handler FUN_00414ca0): three water shimmers with the
 * ambient sound and the locked doors 0x1b4..0x1b6 that the lockpick (item 2)
 * cannot open.
 */
class Room1213 : public Room {
public:
	/** Constructs the room objects. */
	Room1213();
	/** Static descriptor 0x43f318. */
	const RoomDescriptor *getDescriptor() override;
	/** Enter/leave/tick, the doors and the lockpick actions. */
	int handleEvent(int event, int param) override;

private:
	/** Walks to a door, plays `wav` and comments with `line` (the shared tail at 0x41506e). */
	void enqueueDoor(int pos, int dir, int wav, int line);

	ShimmerSprite _e4746a8;  ///< 0x4746a8: water (FUN_00417080())
	ShimmerSprite _e475468;  ///< 0x475468: water (FUN_00417080())
	ShimmerSprite _e475850;  ///< 0x475850: water (FUN_00417080())
	int _h474f20 = kNoSound; ///< 0x474f20: ambient sound loop (WAV 0xd48)
};

Room1213::Room1213() : Room(1213) {
}

const RoomDescriptor *Room1213::getDescriptor() {
	return findRoomData(_id);
}

void Room1213::enqueueDoor(int pos, int dir, int wav, int line) {
	game()->enqueue({
		seqCall(kPrimWalk, { pos, dir }),
		seqCall(kPrimSoundWait, { wav }),
		seqCall(kPrimFace, { 0 }),
		seqCall(kPrimDialogue, { 0, line, line, 0, 0, 0 }),
		seqEnd()
	});
}

int Room1213::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: water shimmers and the ambient sound
	case kEvEnter:
		game()->placeShimmer(&_e4746a8, 0x21a, 0xaa, -1, 0x46, 0x50, 0, 0x10, -1);
		game()->placeShimmer(&_e475468, 0x1a6, 0xac, -1, 0x46, 0x32, 0, 0x10, -1);
		game()->placeShimmer(&_e475850, 0x147, 0xad, -1, 0x46, 0x14, 0, 0x10, -1);
		game()->playSound(&_h474f20, 0xd48, 0, true, -4000, 0);
		return 0;

	// Room left: stop the ambient sound
	case kEvLeave:
		game()->stopSound(&_h474f20);
		return 0;

	// Every 3rd tick: all three shimmers move
	case kEvTick:
		if ((int)ticks() % 3 == 0) {
			_e4746a8.step();
			_e475468.step();
			_e475850.step();
		}
		return 0;

	// Hotspot clicked
	case kEvHotspot:
		switch (param) {
		// Hotspot 0x1b4: locked door - rattle it ("the door is locked")
		case 0x1b4:
			enqueueDoor(xy(0x11, 0x57), 0x5c, 0x323, 0x4c2);
			return 0;

		// Hotspot 0x1b5: locked door - rattle it
		case 0x1b5:
			enqueueDoor(xy(0x21, 0x3e), 0x5b, 0x323, 0x4c2);
			return 0;

		// Hotspot 0x1b6: locked door - rattle it
		case 0x1b6:
			enqueueDoor(xy(0x2d, 0x2f), 0xa5, 0x323, 0x4c2);
			return 0;

		default:
			return 0;
		}

	// Held item over a hotspot: the lockpick (item 2) on the doors
	case kEvItemOnHotspot:
		if (param == 0x201b4)
			return 0x1bb;
		if (param == 0x201b5)
			return 0x1bc;
		if (param == 0x201b6)
			return 0x1bd;
		return 0;

	// Action executed
	case kEvAction:
		switch (param) {
		// Lockpick on door 0x1b4: does not work
		case 0x1bb:
			enqueueDoor(xy(0x11, 0x57), 0x5c, 0x324, 0x4c3);
			return 0;

		// Lockpick on door 0x1b5: does not work
		case 0x1bc:
			enqueueDoor(xy(0x21, 0x3e), 0x5b, 0x324, 0x4c3);
			return 0;

		// Lockpick on door 0x1b6: does not work
		case 0x1bd:
			enqueueDoor(xy(0x2d, 0x2f), 0xa5, 0x324, 0x4c3);
			return 0;

		default:
			return 0;
		}

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom1213() {
	return new Room1213();
}

} // End of namespace Gooka
