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
 * Room2312: port of the room handler FUN_00423300.
 * Item 0x35 (object/hotspot 0x35) sits up high while flag 0x909 is set:
 * item 4 knocks it down, then item 0x21 or 0x40 picks it up. Arriving from
 * room 2304 while flag 0x90a is set and line 0x902 has been said (its flag
 * is clear) is deadly unless the hero carries item 0x46 or 0x42: the
 * game-over blinds close (event 0x17 once per tick). Using item 0x46 on the
 * exit to 2304 clears flag 0x90a.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** Room 2312 (original handler FUN_00423300). */
class Room2312 : public Room {
public:
	Room2312();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;

private:
	Sprite _s47b1a0; ///< object at 0x47b1a0 (FUN_0040ff60()): item 0x35 (frame 0 up high, 2 on the floor)
	/**
	 * Game-over blinds. The original uses one global (DAT_00473d30) for the
	 * three rooms with blinds; it is reset on every entry, so a member per
	 * room behaves the same.
	 */
	EndTextScroller _blinds;
};

/** Creates the room objects. */
Room2312::Room2312() : Room(2312) {
}

/** Static descriptor DAT_00442ad8. */
const RoomDescriptor *Room2312::getDescriptor() {
	return findRoomData(_id);
}

/** Room events: item 0x35, the deadly arrival and the blinds. */
int Room2312::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: item 0x35 up high or on the floor; arrival from 2304 may end the game
	case kEvEnter:
		if (game()->flagTest(0x909))
			game()->placeObjectIfFlag(0x35, &_s47b1a0, 0xe8, 0x14, 0, 0, 0);
		else
			game()->placeObjectIfFlag(0x35, &_s47b1a0, 0x10e, 0xfa, 0, 2, 0);
		if (param == 0x900 && game()->flagTest(0x90a) && !game()->flagTest(0x902)) {
			if (!game()->hasItem(0x46) && !game()->hasItem(0x42)) {
				// No protection: comment, wait, then close the blinds forever (game over)
				game()->enqueue({
					seqCall(kPrimWalk, { xy(140, 83), 0 }),
					seqCall(kPrimDialogue, { 0, 0x90d, 0x90d, 0, 0, 0 }),
					seqCall(kPrimWait, { 100 }),
					seqWhile(kPrimNone),
					seqCall(kPrimRaiseEvent, { 0x17, 0 }),
					seqYield(),
					seqEndWhile(),
					seqEnd()
				});
				_blinds.reset();
				return 0;
			}
			game()->enqueue({
				seqCall(kPrimWalk, { xy(140, 83), 0 }),
				seqCall(kPrimDialogue, { 0, 0x90d, 0x90d, 0, 0, 0 }),
				seqEnd()
			});
		}
		_blinds.reset();
		return 0;
	// Hotspot clicked
	case kEvHotspot:
		// Hotspot 0x35: item 0x35 - comment depending on whether it is still up high
		if (param == 0x35) {
			if (game()->flagTest(0x909)) {
				game()->sayIdle(0x7d1);
				return -1;
			}
			game()->sayIdle(0x90f);
			return -1;
		}
		// Hotspot 0x900: exit to room 2304 - refused while flag 0x90a is set and line 0x902 was said
		if (param == 0x900 && game()->flagTest(0x90a) && !game()->flagTest(0x902)) {
			game()->enqueue({
				seqCall(kPrimWalk, { xy(150, 83), 36 }),
				seqCall(kPrimWait, { 10 }),
				seqCall(kPrimWalk, { xy(140, 83), 0 }),
				seqCall(kPrimDialogue, { 0, 0x90e, 0x90e, 0, 0, 0 }),
				seqEnd()
			});
			return -1;
		}
		return 0;
	// Held item over a hotspot
	case kEvItemOnHotspot:
		// Item 0x21 or 0x40 on item 0x35 once it is down: action 0x910 (pick it up)
		if (param == 0x210035 || param == 0x400035)
			return game()->flagTest(0x909) ? 0 : 0x910;
		// Item 4 on item 0x35 while it is up high: action 0x909 (knock it down)
		if (param == 0x40035)
			return game()->flagTestId(0x909);
		// Item 0x46 on the exit 0x900: action 0x90a once line 0x902 was said
		if (param == 0x460900)
			return game()->flagTest(0x902) ? 0 : game()->flagTestId(0x90a);
		return 0;
	// Action executed
	case kEvAction:
		// Action 0x909: knock item 0x35 down with item 4
		if (param == 0x909) {
			game()->flagClear(0x909);
			game()->enqueue({
				seqCall(kPrimWalk, { xy(54, 68), 36 }),
				seqCall(kPrimWait, { 2 }),
				seqCall(kPrimSetFrame, { &hero(), 120 }),
				seqCall(kPrimWait, { 3 }),
				seqCall(kPrimAnimate, { &hero(), 122, 5 }),
				seqCall(kPrimSound, { 0xf1f }),
				seqCall(kPrimSetFrame, { &_s47b1a0, 1 }),
				seqSpawn(),
				seqCall(kPrimMoveLinear, { &_s47b1a0, 20, 2, 1, 0, 1 }),
				seqJoin(),
				seqSpawn(),
				seqCall(kPrimMoveAccel, { &_s47b1a0, 200, 0, 1, 1, 1, 1 }),
				seqCall(kPrimSound, { 0xf20 }),
				seqCall(kPrimSetFrame, { &_s47b1a0, 2 }),
				seqCall(kPrimShowAt, { &_s47b1a0, xy(270, 250), 0 }),
				seqJoin(),
				seqCall(kPrimWait, { 5 }),
				seqCall(kPrimSetFrame, { &hero(), 36 }),
				seqWaitChildren(),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
			return 0;
		}
		// Action 0x90a: item 0x46 at the exit - clears flag 0x90a
		if (param == 0x90a) {
			game()->flagClear(0x90a);
			game()->enqueue({
				seqCall(kPrimDialogue, { 0, 0x90b, 0x90b, 0, 0, 0 }),
				seqCall(kPrimWalk, { xy(150, 83), 36 }),
				seqCall(kPrimSoundWait, { 0xf1c }),
				seqCall(kPrimDialogue, { 0, 0x90c, 0x90c, 0, 0, 0 }),
				seqEnd()
			});
			return 0;
		}
		// Action 0x910: pick item 0x35 up from the floor
		if (param == 0x910) {
			game()->enqueue({
				seqCall(kPrimWalk, { xy(61, 68), 146 }),
				seqCall(kPrimHideExit, { 0x35 }),
				seqCall(kPrimAddItem, { 0x35 }),
				seqCall(kPrimClearFlag, { 0x35 }),
				seqCall(kPrimHide, { &_s47b1a0 }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
			return 0;
		}
		return 0;
	// Raised every tick by the game-over sequence: close the blinds one step
	case kEvRoomPrivate1:
		_blinds.addLine();
		return 0;
	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom2312() {
	return new Room2312();
}

} // End of namespace Gooka
