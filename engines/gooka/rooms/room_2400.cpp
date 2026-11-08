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
 * Room2400: port of the room handler FUN_00424170.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 2400 (original handler FUN_00424170): the sincha (flag/action 0x963
 * "kill the sincha") guards the upper part of the room. It snaps at the
 * hero when he walks there; feeding it item 0x35 kills it.
 */
class Room2400 : public Room {
public:
	Room2400();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;

private:
	Sprite _s47b7b0; ///< object at 0x47b7b0: the sincha
	Sprite _s47b550; ///< object at 0x47b550: overlay of the snapping sincha
	Sprite _s47b598; ///< object at 0x47b598: the thrown item 0x35
};

/** Constructs the room statics. */
Room2400::Room2400() : Room(2400) {
}

/** Static descriptor 0x442dc8. */
const RoomDescriptor *Room2400::getDescriptor() {
	return findRoomData(_id);
}

/** Room events (FUN_00424170, `what` >= 2). */
int Room2400::handleEvent(int event, int param) {
	switch (event) {
	// Enter: living sincha (frame 0) while flag 0x963 is set, else dead (frame 10)
	case kEvEnter:
		game()->placeObject(&_s47b7b0, 0xaa, 0x6e, 0, game()->flagTest(0x963) ? 0 : 10, 2, -1);
		game()->placeObjectHidden(&_s47b550, 0xaa, 0x6e, 0, 0, 0);
		game()->placeObjectHidden(&_s47b598, 300, 0xdc, 0, 0xb, 3);
		return 0;

	// Floor click in the upper part while the sincha lives: it snaps at the hero
	case kEvFloorClick:
		if ((uint32)param >> 16 < 0x56 && game()->flagTest(0x963)) {
			game()->enqueue({
				seqCall(kPrimWalk, { xy(74, 86), -1 }),
				seqCall(kPrimShow, { &_s47b550 }),
				seqCall(kPrimSetFrame, { &_s47b7b0, 1 }),
				seqCall(kPrimSetFrame, { &_s47b550, 3 }),
				seqCall(kPrimSound, { 3875 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimSetFrame, { &_s47b7b0, 2 }),
				seqCall(kPrimSetFrame, { &_s47b550, 4 }),
				seqCall(kPrimWait, { 8 }),
				seqSpawn(),
				seqCall(kPrimWalk, { xy(74, 96), 0 }),
				seqJoin(),
				seqCall(kPrimWait, { 30 }),
				seqCall(kPrimSound, { 3876 }),
				seqCall(kPrimSetFrame, { &_s47b7b0, 1 }),
				seqCall(kPrimSetFrame, { &_s47b550, 3 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimHide, { &_s47b550 }),
				seqCall(kPrimSetFrame, { &_s47b7b0, 0 }),
				seqWaitChildren(),
				seqCall(kPrimDialogue, { 0, 2401, 2401, 0, 0, 0 }),
				seqEnd()
			});
			return -1;
		}
		break;

	// Held item over a hotspot: item 0x35 on 0x960 = action 0x963
	case kEvItemOnHotspot:
		if (param == 0x350960)
			return 0x963;
		break;

	// Action 0x963: throw item 0x35 to the sincha, it eats it and dies
	case kEvAction:
		if (param == 0x963) {
			game()->flagClear(0x963);
			game()->consumeHeldItem();
			game()->enqueue({
				seqCall(kPrimWalk, { xy(74, 86), -1 }),
				seqSpawn(),
				seqCall(kPrimFace, { 166 }),
				seqCall(kPrimShow, { &_s47b598 }),
				seqSpawn(),
				seqCall(kPrimWait, { 5 }),
				seqCall(kPrimShowPriority, { &_s47b598, 0 | 1 << 16 }),
				seqCall(kPrimFace, { 39 }),
				seqCall(kPrimDialogue, { 0, 2405, 2405, 0, 0, 0 }),
				seqJoin(),
				seqCall(kPrimMoveVelocity, { &_s47b598, 30, 0, 0, -15, 1, 1 }),
				seqCall(kPrimSound, { 3872 }),
				seqCall(kPrimHide, { &_s47b598 }),
				seqWaitChildren(),
				seqJoin(),
				seqCall(kPrimSound, { 3875 }),
				seqCall(kPrimShow, { &_s47b550 }),
				seqCall(kPrimSetFrame, { &_s47b7b0, 1 }),
				seqCall(kPrimSetFrame, { &_s47b550, 3 }),
				seqCall(kPrimWait, { 2 }),
				seqCall(kPrimSetFrame, { &_s47b7b0, 2 }),
				seqCall(kPrimSetFrame, { &_s47b550, 4 }),
				seqWaitChildren(),
				seqCall(kPrimSound, { 3877 }),
				seqCall(kPrimSetFrame, { &_s47b7b0, 1 }),
				seqCall(kPrimSetFrame, { &_s47b550, 3 }),
				seqCall(kPrimWait, { 2 }),
				seqCall(kPrimHide, { &_s47b550 }),
				seqCall(kPrimSetFrame, { &_s47b7b0, 0 }),
				seqCall(kPrimWait, { 20 }),
				seqCall(kPrimSetFrame, { &_s47b7b0, 5 }),
				seqCall(kPrimWait, { 10 }),
				seqCall(kPrimAnimate, { &_s47b7b0, 10, 8 }),
				seqEnd()
			});
			return 0;
		}
		break;

	default:
		break;
	}
	return 0;
}

/** Factory used by the room registry. */
Room *createRoom2400() {
	return new Room2400();
}

} // End of namespace Gooka
