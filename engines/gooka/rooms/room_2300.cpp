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
 * Room2300: port of the room handler FUN_00422a80.
 * Coming from room 2304 after line 0x903 was said there (flag 0x903 clear,
 * the hero gave up waiting) plays a scene once and sets the flag again: a
 * creature walks in from the left, a flickering sprite animates, the hero
 * comments (lines 0x8fd, 0x8fe) and the creature walks away again.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** Room 2300 (original handler FUN_00422a80): the creature scene. */
class Room2300 : public Room {
public:
	Room2300();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;

private:
	Sprite _s47aff8; ///< object at 0x47aff8 (FUN_0040ff60()): the creature (walk frames 11..14 right, 15..18 left)
	Sprite _s47b288; ///< object at 0x47b288 (FUN_0040ff60()): the flickering sprite (frames 2..10)
};

/** Creates the room objects. */
Room2300::Room2300() : Room(2300) {
}

/** Static descriptor DAT_00442810. */
const RoomDescriptor *Room2300::getDescriptor() {
	return findRoomData(_id);
}

/** Room events: the entry scene and the creature's walk cycle. */
int Room2300::handleEvent(int event, int param) {
	switch (event) {
	// Room entered from 2304 with flag 0x903 clear (line 0x903 said): play the scene
	case kEvEnter:
		if (!game()->flagTest(0x903) && param == 0x900) {
			game()->flagSet(0x903, true);
			game()->placeObject(&_s47aff8, -0x50, 0xae, 0, 0, 0, -1);
			game()->placeObjectHidden(&_s47b288, 0x4f, 0x6a, 5, 2, 0);
			game()->enqueue({
				seqSpawn(),
				seqCall(kPrimMoveLinear, { &_s47aff8, 136, 1, 1, 1, 11 }),
				seqCall(kPrimWait, { 70 }),
				seqCall(kPrimSound, { 0xf04 }),
				seqCall(kPrimAnimate, { &_s47b288, 10, 3 }),
				seqCall(kPrimAnimate, { &_s47b288, 7, 3 }),
				seqCall(kPrimAnimate, { &_s47b288, 10, 3 }),
				seqCall(kPrimAnimate, { &_s47b288, 5, 3 }),
				seqCall(kPrimAnimate, { &_s47b288, 8, 3 }),
				seqCall(kPrimAnimate, { &_s47b288, 10, 3 }),
				seqCall(kPrimAnimate, { &_s47b288, 4, 3 }),
				seqCall(kPrimAnimate, { &_s47b288, 10, 3 }),
				seqCall(kPrimAnimate, { &_s47b288, 5, 3 }),
				seqCall(kPrimAnimate, { &_s47b288, 10, 3 }),
				seqCall(kPrimAnimate, { &_s47b288, 5, 3 }),
				seqCall(kPrimAnimate, { &_s47b288, 8, 3 }),
				seqCall(kPrimAnimate, { &_s47b288, 10, 3 }),
				seqCall(kPrimAnimate, { &_s47b288, 10, 3 }),
				seqCall(kPrimAnimate, { &_s47b288, 7, 3 }),
				seqCall(kPrimAnimate, { &_s47b288, 10, 3 }),
				seqCall(kPrimAnimate, { &_s47b288, 5, 3 }),
				seqCall(kPrimAnimate, { &_s47b288, 8, 3 }),
				seqCall(kPrimAnimate, { &_s47b288, 4, 3 }),
				seqCall(kPrimAnimate, { &_s47b288, 2, 4 }),
				seqCall(kPrimHide, { &_s47b288 }),
				seqCall(kPrimWait, { 50 }),
				seqJoin(),
				seqCall(kPrimWalk, { xy(62, 61), 0 }),
				seqCall(kPrimDialogue, { 0, 0x8fd, 0x8fd, 0, 0, 0 }),
				seqCall(kPrimWait, { 50 }),
				seqCall(kPrimDialogue, { 0, 0x8fe, 0x8fe, 0, 0, 0 }),
				seqWaitChildren(),
				seqCall(kPrimSetFrame, { &_s47aff8, 15 }),
				seqCall(kPrimMoveLinear, { &_s47aff8, 130, -1, 1, -1, 2 }),
				seqCall(kPrimHide, { &_s47aff8 }),
				seqEnd()
			});
		}
		return 0;
	// Tick: walk cycle of the creature, one frame every 4 ticks (11..14 or 15..18 by direction)
	case kEvTick: {
		const int base = _s47aff8.frame > 0xe ? 0xf : 0xb;
		_s47aff8.setFrame(base + ((int)ticks() / 4) % 4);
		return 0;
	}
	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom2300() {
	return new Room2300();
}

} // End of namespace Gooka
