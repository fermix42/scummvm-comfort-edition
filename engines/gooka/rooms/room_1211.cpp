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
 * Room 1211: street of Avaral with water, two locked doors and the door to
 * room 1383 that the lockpick opens (port of the room handler FUN_00414790).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 1211 (original handler FUN_00414790): three water shimmers with the
 * ambient sound, the locked doors 0x1b6/0x1b7 and the door 0x567 (exit to
 * 1383) that is locked until the lockpick (item 2) opened it.
 */
class Room1211 : public Room {
public:
	/** Constructs the room objects. */
	Room1211();
	/** Static descriptor 0x43f190. */
	const RoomDescriptor *getDescriptor() override;
	/** Enter/leave/tick, the doors and the lockpick actions. */
	int handleEvent(int event, int param) override;

private:
	/** Walks to a door, plays `wav` and comments with `line` (the shared tail at 0x414bd7). */
	void enqueueDoor(int pos, int dir, int wav, int line);

	ShimmerSprite _e475a58;  ///< 0x475a58: water (FUN_00412690(0x10, -1))
	ShimmerSprite _e475658;  ///< 0x475658: water (FUN_00412690(0x10, -1))
	ShimmerSprite _e475c48;  ///< 0x475c48: water (FUN_00412690(0x10, -1))
	int _h475a40 = kNoSound; ///< 0x475a40: ambient sound loop (WAV 0xd48)
};

Room1211::Room1211() : Room(1211), _e475a58(16, -1), _e475658(16, -1), _e475c48(16, -1) {
}

const RoomDescriptor *Room1211::getDescriptor() {
	return findRoomData(_id);
}

void Room1211::enqueueDoor(int pos, int dir, int wav, int line) {
	game()->enqueue({
		seqCall(kPrimWalk, { pos, dir }),
		seqCall(kPrimSoundWait, { wav }),
		seqCall(kPrimFace, { 0 }),
		seqCall(kPrimDialogue, { 0, line, line, 0, 0, 0 }),
		seqEnd()
	});
}

int Room1211::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: water shimmers and the ambient sound
	case kEvEnter:
		game()->placeShimmer(&_e475a58, 0x1a, 0xaa, -1, 0x46, 0x50, 0, 0x10, -1);
		game()->placeShimmer(&_e475658, 0x95, 0xac, -1, 0x46, 0x32, 0, 0x10, -1);
		game()->placeShimmer(&_e475c48, 0x104, 0xad, -1, 0x46, 0x14, 0, 0x10, -1);
		game()->playSound(&_h475a40, 0xd48, 0, true, -4000, 0);
		return 0;

	// Room left: stop the ambient sound
	case kEvLeave:
		game()->stopSound(&_h475a40);
		return 0;

	// Every tick: one of the three shimmers moves, in turn
	case kEvTick:
		if ((int)ticks() % 3 == 0)
			_e475a58.step();
		if ((int)ticks() % 3 == 1)
			_e475658.step();
		if ((int)ticks() % 3 == 2)
			_e475c48.step();
		return 0;

	// Hotspot clicked
	case kEvHotspot:
		switch (param) {
		// Hotspot 0x1b6: locked door - rattle it ("the door is locked")
		case 0x1b6:
			enqueueDoor(xy(0x77, 0x2f), 0xa5, 0x323, 0x4c2);
			return 0;

		// Hotspot 0x1b7: locked door - rattle it
		case 0x1b7:
			enqueueDoor(xy(0x7e, 0x42), 0x63, 0x323, 0x4c2);
			return 0;

		// Hotspot 0x567: door to 1383 - rattle it until the lockpick opened it (line 0x4c4 said), then use it as exit
		case 0x567:
			if (!game()->flagTest(0x4c4))
				return 0;
			enqueueDoor(xy(0x8d, 0x5d), 0x63, 0x323, 0x4c2);
			return -1;

		default:
			return 0;
		}

	// Held item over a hotspot: the lockpick (item 2) on the doors
	case kEvItemOnHotspot:
		if (param == 0x201b6)
			return 0x1bd;
		if (param == 0x201b7)
			return 0x1be;
		if (param == 0x20567)
			return game()->flagTest(0x4c4) * 0x1bf;
		return 0;

	// Action executed
	case kEvAction:
		switch (param) {
		// Lockpick on door 0x1b6: does not work
		case 0x1bd:
			enqueueDoor(xy(0x77, 0x2f), 0xa5, 0x324, 0x4c3);
			return 0;

		// Lockpick on door 0x1b7: does not work
		case 0x1be:
			enqueueDoor(xy(0x7e, 0x42), 0x63, 0x324, 0x4c3);
			return 0;

		// Lockpick on door 0x567: it opens ("this one could be unlocked!")
		case 0x1bf:
			game()->flagClear(0x4c4);
			enqueueDoor(xy(0x8d, 0x5d), 0x63, 0x324, 0x4c4);
			return 0;

		default:
			return 0;
		}

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom1211() {
	return new Room1211();
}

} // End of namespace Gooka
