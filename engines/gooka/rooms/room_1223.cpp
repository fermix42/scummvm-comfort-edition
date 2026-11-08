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
 * Room 1223: the palace gate with the guard; the throne room (1239) is
 * behind it (port of the room handler FUN_00415d60).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 1223 (original handler FUN_00415d60): water shimmer and ambient sound, the
 * guard 0x4ca who lets Gooka in only after he showed his father's papers
 * (item 0xe, action 0x4cb), and the gate 0x4d7 to the throne room, which
 * is only entered when there is something to tell the king.
 */
class Room1223 : public Room {
public:
	/** Constructs the room objects. */
	Room1223();
	/** Static descriptor 0x43fa50. */
	const RoomDescriptor *getDescriptor() override;
	/** Enter/leave/tick, the guard and the gate. */
	int handleEvent(int event, int param) override;

private:
	int _h4751ac = kNoSound; ///< 0x4751ac: ambient sound loop (WAV 0xd48)
	ShimmerSprite _e474fb8;  ///< 0x474fb8: water (FUN_00417080())
	Actor _n474a90;          ///< 0x474a90: the guard
};

Room1223::Room1223() : Room(1223) {
}

const RoomDescriptor *Room1223::getDescriptor() {
	return findRoomData(_id);
}

int Room1223::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: ambient sound, water; the guard (hotspot 0x4ca) stands at the gate while flag 0x846 is set
	case kEvEnter:
		game()->playSound(&_h4751ac, 0xd48, 0, true, -4000, 0);
		game()->placeShimmer(&_e474fb8, 0x24e, 0xa4, -1, 0x50, 0x28, 0, 0x10, -1);
		if (game()->flagTest(0x846))
			game()->placeObjectIfFlag(0x4ca, &_n474a90, 0x230, 0x91, 1, 5, 0);
		return 0;

	// Room left: stop the ambient sound
	case kEvLeave:
		game()->stopSound(&_h4751ac);
		return 0;

	// Every 3rd tick: water shimmer
	case kEvTick:
		if ((int)ticks() % 3 == 0)
			_e474fb8.step();
		return 0;

	// Hotspot clicked: the guard (0x4ca, only before his first talk, line 0x4cc) or the gate (0x4d7)
	case kEvHotspot:
		if (param == 0x4ca) {
			if (!game()->flagTest(0x4cc))
				return 0;
		} else if (param != 0x4d7) {
			return 0;
		}
		if (game()->flagTest(0x4cb)) {
			// Papers not shown yet: the first talk with the guard, afterwards he refuses entry
			if (!game()->flagTest(0x4cc)) {
				game()->sayIdle(0x4d5);
				return -1;
			}
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x82, 0x54), -1 }),
				seqSpawn(),
				seqCall(kPrimAnimate, { &_n474a90, 7, 5 }),
				seqCall(kPrimSay, { &_n474a90, 0x4cc, 7, 7, 2 }),
				seqJoin(),
				seqCall(kPrimFace, { 0x24 }),
				seqWaitChildren(),
				seqCall(kPrimDialogue, { &_n474a90, 0x4cd, 0x4d2, 7, 7, 2 }),
				seqCall(kPrimWalk, { xy(0x64, 0x54), 0 }),
				seqCall(kPrimAnimate, { &_n474a90, 5, 5 }),
				seqEnd()
			});
			return -1;
		}
		// Papers shown: the gate is open, but Gooka only goes in when there is news for the king
		if (!game()->flagTest(0x4da) &&
				(!game()->flagTest(0x4f2) || game()->flagTest(0xd56)) &&
				(!game()->flagTest(0x4fe) || game()->flagTest(0x6ad)) &&
				(!game()->flagTest(0x516) || (!game()->hasItem(0x42) && !game()->hasItem(0x46)))) {
			game()->sayIdle(0x4c8);
			return -1;
		}
		return 0;

	// Held item over a hotspot: the papers (item 0xe) on the guard after the first talk
	case kEvItemOnHotspot:
		if (param == 0xe04ca)
			return game()->flagTest(0x4cc) ? 0 : game()->flagTestId(0x4cb);
		return 0;

	// Action 0x4cb: show the papers, the guard lets Gooka into the throne room
	case kEvAction:
		if (param == 0x4cb) {
			game()->flagClear(0x4cb);
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x7e, 0x54), 0x24 }),
				seqCall(kPrimDialogue, { &_n474a90, 0x4d3, 0x4d4, 5, 4, 2 }),
				seqCall(kPrimWalk, { xy(0x9f, 0x54), -1 }),
				seqCall(kPrimChangeRoom, { 0x4d7 }),
				seqEnd()
			});
		}
		return 0;

	// Hotspot lookup: before the papers are shown the gate area 0x4d6 counts as the guard,
	// afterwards the gate area and the guard both are the gate 0x4d7 (cursor 3)
	case kEvHotspotOverride:
		if (param == 0x4d6)
			return game()->flagTest(0x4cb) ? 0x4ca : 0x304d7;
		if (param == 0x4ca && !game()->flagTest(0x4cb))
			return 0x304d7;
		return 0;

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom1223() {
	return new Room1223();
}

} // End of namespace Gooka
