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
 * Room1679: port of the room handler FUN_00417760.
 * A closed gate (flag 0x690) blocks the exit to room 1600 (hotspot 0x640).
 * Knocking on the hatch (hotspot 0x6a5) makes the guard behind it look out
 * and talk; handing him item 10 opens the gate and the hero walks through;
 * any other item is used up without effect.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** Room 1679 (original handler FUN_00417760): the gate with the guard. */
class Room1679 : public Room {
public:
	Room1679();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;

private:
	Sprite _s4766b8; ///< object at 0x4766b8 (FUN_0040ff60()): the gate (while flag 0x690 is set)
	Sprite _s476968; ///< object at 0x476968 (FUN_0040ff60()): the hatch cover
	Actor _n476700; ///< object at 0x476700 (FUN_0040dc30()): the guard behind the hatch
};

/** Creates the room objects. */
Room1679::Room1679() : Room(1679) {
}

/** Static descriptor DAT_0043ff48. */
const RoomDescriptor *Room1679::getDescriptor() {
	return findRoomData(_id);
}

/** Room events: gate, hatch and guard. */
int Room1679::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: hatch cover, gate (if still closed) and the guard
	case kEvEnter:
		game()->placeObject(&_s476968, 0x7e, 0x75, 0, 0, 1, -1);
		if (game()->flagTest(0x690))
			game()->placeObject(&_s4766b8, 0x10c, 0x1b, 0, 1, 0, -1);
		game()->placeObject(&_n476700, 0x8c, 0x92, 0, 2, 0, -1);
		return 0;
	// Hotspot clicked
	case kEvHotspot:
		// Hotspot 0x640: exit to room 1600 - blocked while the gate is closed
		if (param == 0x640) {
			if (game()->flagTest(0x690)) {
				game()->sayIdle(0x649);
				return -1;
			}
			return 0;
		}
		// Hotspot 0x6a5: the hatch - knock, the guard looks out and says his unsaid lines
		if (param == 0x6a5 &&
				((game()->flagTest(0x691) && game()->flagTest(0x4f1)) ||
				(game()->flagTest(0x694) && !game()->flagTest(0x4f1)))) {
			game()->enqueue({
				seqCall(kPrimWalk, { xy(42, 92), 39 }),
				seqCall(kPrimDialogue, { 0, 0x6a3, 0x6a3, 0, 0, 0 }),
				seqCall(kPrimWait, { 40 }),
				seqSpawn(),
				seqCall(kPrimSound, { 0xef7 }),
				seqCall(kPrimMoveLinear, { &_s476968, 20, 0, 1, -4, 1 }),
				seqJoin(),
				seqCall(kPrimFace, { 39 }),
				seqWaitChildren(),
				seqIf(kPrimIfFlag, { 0x4f1 }),
				seqCall(kPrimDialogue, { &_n476700, -0x691, 0x693, 2, 2, 2 }),
				seqElse(),
				seqCall(kPrimDialogue, { &_n476700, -0x694, 0x69a, 2, 2, 2 }),
				seqEndIf(),
				seqCall(kPrimSound, { 0xef6 }),
				seqCall(kPrimMoveLinear, { &_s476968, 10, 0, 1, 8, 1 }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
			return 0;
		}
		return 0;
	// Held item over the hatch (after flag 0x6cd, before 0x69a): item 10 = action 0x6a7, others 0x6a6
	case kEvItemOnHotspot:
		if ((int16)param == 0x6a5 && game()->flagTest(0x6cd) && !game()->flagTest(0x69a))
			return 0x6a7 - (((uint)param >> 16) != 10 ? 1 : 0);
		return 0;
	// Action executed
	case kEvAction:
		// Action 0x6a6: any other item - used up, the guard talks, the hatch closes again
		if (param == 0x6a6) {
			game()->consumeHeldItem();
			game()->enqueue({
				seqCall(kPrimWalk, { xy(42, 92), 39 }),
				seqCall(kPrimDialogue, { 0, 0x6a4, 0x6a4, 0, 0, 0 }),
				seqCall(kPrimWait, { 40 }),
				seqSpawn(),
				seqCall(kPrimSound, { 0xef7 }),
				seqCall(kPrimMoveLinear, { &_s476968, 20, 0, 1, -4, 1 }),
				seqJoin(),
				seqCall(kPrimFace, { 39 }),
				seqWaitChildren(),
				seqCall(kPrimDialogue, { &_n476700, -0x69b, 0x6a1, 2, 2, 2 }),
				seqCall(kPrimSound, { 0xef6 }),
				seqCall(kPrimMoveLinear, { &_s476968, 10, 0, 1, 8, 1 }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
			return 0;
		}
		// Action 0x6a7: item 10 - the guard opens the gate, the hero goes to room 1600
		if (param == 0x6a7) {
			game()->consumeHeldItem();
			game()->enqueue({
				seqCall(kPrimWalk, { xy(42, 92), 39 }),
				seqCall(kPrimDialogue, { 0, 0x6a4, 0x6a4, 0, 0, 0 }),
				seqCall(kPrimWait, { 40 }),
				seqSpawn(),
				seqCall(kPrimSound, { 0xef7 }),
				seqCall(kPrimMoveLinear, { &_s476968, 20, 0, 1, -4, 1 }),
				seqJoin(),
				seqCall(kPrimFace, { 39 }),
				seqWaitChildren(),
				seqCall(kPrimDialogue, { &_n476700, -0x69b, 0x69c, 2, 2, 2 }),
				seqCall(kPrimSay, { &_n476700, 0x6a2, 2, 2, 2 }),
				seqSpawn(),
				seqCall(kPrimSound, { 0xef6 }),
				seqCall(kPrimMoveLinear, { &_s476968, 10, 0, 1, 8, 1 }),
				seqCall(kPrimAmbient, { 0xef5 }),
				seqCall(kPrimMoveLinear, { &_s4766b8, 100, 0, 1, -2, 1 }),
				seqJoin(),
				seqCall(kPrimClearFlag, { 0x690 }),
				seqCall(kPrimWalk, { xy(91, 92), 39 }),
				seqCall(kPrimWait, { 30 }),
				seqCall(kPrimWalk, { xy(91, 85), -1 }),
				seqCall(kPrimAmbientStop),
				seqCall(kPrimChangeRoom, { 1600 }),
				seqEnd()
			});
			return 0;
		}
		return 0;
	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom1679() {
	return new Room1679();
}

} // End of namespace Gooka
