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
 * Room2083: port of the room handler FUN_00421390.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** Room 2083 (original handler FUN_00421390). */
class Room2083 : public Room {
public:
	/** Room 2083; the member is the original static. */
	Room2083();
	/** Static descriptor DAT_00442548. */
	const RoomDescriptor *getDescriptor() override;
	/** The room body (what >= 2). */
	int handleEvent(int event, int param) override;

private:
	Sprite _s47aba0; ///< object of the passage 0x82d (frame 1 while line 2091 is unsaid, else 5)
};

Room2083::Room2083() : Room(2083) {
}

const RoomDescriptor *Room2083::getDescriptor() {
	return findRoomData(_id);
}

int Room2083::handleEvent(int event, int param) {
	switch (event) {
	// Enter: the passage object (FUN_0040fde0(0x84, 0x86) and FUN_0040fec0(0x831) called first are empty)
	case kEvEnter:
		game()->placeObject(&_s47aba0, 0x1d5, 0x43, 0, game()->flagTest(0x82b) ? 1 : 5, 0, -1);
		return 0;

	// Hotspot clicked
	case kEvHotspot:
		// Hotspot 7: item 7 - comment only (line 2084); it is taken with action 0x825
		if (param == 7) {
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x35, 0x5c), 0x60 }),
				seqCall(kPrimWait, { 0xa }),
				seqCall(kPrimFace, { 0 }),
				seqCall(kPrimDialogue, { 0, 0x824, 0x824, 0, 0, 0 }),
				seqEnd()
			});
			return -1;
		}
		// Hotspot 0x82d (room 2093): closed while line 2091 is unsaid (comment, line 2086), else jump down into the maze
		if (param == 0x82d) {
			if (game()->flagTest(0x82b)) {
				game()->enqueue({
					seqCall(kPrimWalk, { xy(0x81, 0x36), 0xa6 }),
					seqCall(kPrimWait, { 0xa }),
					seqCall(kPrimFace, { 0 }),
					seqCall(kPrimDialogue, { 0, 0x826, 0x826, 0, 0, 0 }),
					seqEnd()
				});
				return -1;
			}
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x81, 0x30), -1 }),
				seqCall(kPrimSound, { 0x831 }),
				seqCall(kPrimSetFrame, { &hero(), 0x95 }),
				seqCall(kPrimShowPriority, { &hero(), 0 }),
				seqCall(kPrimWait, { 2 }),
				seqCall(kPrimAnimate, { &hero(), 0x9c, 2 }),
				seqCall(kPrimHide, { &hero() }),
				seqCall(kPrimWait, { 1 }),
				seqCall(kPrimChangeRoom, { 0x82d }),
				seqCall(kPrimShow, { &hero() }),
				seqEnd()
			});
			return -1;
		}
		return 0;

	// Held item over a hotspot
	case kEvItemOnHotspot:
		// Item 0x11 on item 7: action 0x825 while item 7 lies there
		if (param == 0x110007)
			return game()->flagTest(7) ? 0x825 : 0;
		// Item 0x2c on the passage 0x82d: action 0x829 (once)
		if (param == 0x2c082d)
			return game()->flagTestId(0x829);
		// Item 0x30 on the passage 0x82d: action 0x827 while line 2091 is unsaid
		if (param == 0x30082d)
			return game()->flagTestId(0x827) * (game()->flagTest(0x82b) ? 1 : 0);
		return 0;

	// Actions
	case kEvAction:
		// Action 0x825: take item 7 with item 0x11
		if (param == 0x825) {
			if (game()->flagTestAndClear(7)) {
				game()->enqueue({
					seqCall(kPrimWalk, { xy(0x35, 0x5c), 0x60 }),
					seqCall(kPrimSetFrame, { &hero(), 0x86 }),
					seqCall(kPrimSound, { 7 }),
					seqCall(kPrimAnimate, { &hero(), 0x84, 4 }),
					seqCall(kPrimHideExit, { 7 }),
					seqCall(kPrimAddItem, { 7 }),
					seqCall(kPrimClearFlag, { 7 }),
					seqCall(kPrimSetFrame, { &hero(), 0x60 }),
					seqCall(kPrimFace, { 0 }),
					seqEnd()
				});
				return 0;
			}
			return 0;
		}
		// Action 0x827: item 0x30 opens the passage (line 2091) unless flag 0x829 is still set (line 2088)
		if (param == 0x827) {
			if (!game()->flagTest(0x829)) {
				game()->enqueue({
					seqCall(kPrimWalk, { xy(0x7f, 0x36), 0xa6 }),
					seqCall(kPrimSoundWait, { 0x324 }),
					seqCall(kPrimSound, { 0xedf }),
					seqCall(kPrimAnimate, { &_s47aba0, 5, 3 }),
					seqCall(kPrimFace, { 0 }),
					seqCall(kPrimDialogue, { 0, 0x82b, 0x82b, 0, 0, 0 }),
					seqEnd()
				});
				return 0;
			}
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x7f, 0x36), 0xa6 }),
				seqCall(kPrimWait, { 0xa }),
				seqCall(kPrimFace, { 0 }),
				seqCall(kPrimDialogue, { 0, 0x828, 0x828, 0, 0, 0 }),
				seqEnd()
			});
			return 0;
		}
		// Action 0x829: item 0x2c at the passage clears flag 0x829 (line 2090)
		if (param == 0x829) {
			game()->flagClear(0x829);
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x7f, 0x36), 0x27 }),
				seqCall(kPrimDialogue, { 0, 0x82a, 0x82a, 0, 0, 0 }),
				seqCall(kPrimFace, { 0xa6 }),
				seqCall(kPrimWait, { 0xa }),
				seqCall(kPrimWalk, { xy(0x89, 0x36), 0xa6 }),
				seqCall(kPrimWait, { 0xa }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
			return 0;
		}
		return 0;

	default:
		break;
	}
	return 0;
}

/** Creates the handler (registered in rooms/ported.cpp). */
Room *createRoom2083() {
	return new Room2083();
}

} // End of namespace Gooka
