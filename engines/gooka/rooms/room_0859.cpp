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
 * Room 859: the sister Baroda's room with her chest (port of the room
 * handler FUN_00428a80).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 859 (original handler FUN_00428a80): the chest (0x35c) is locked
 * (flag 0x35f) until it is pried open; inside are items 0x27/0x28 and,
 * under the double bottom (flag 0x362 until found), the pouch 0x16.
 */
class Room0859 : public Room {
public:
	/** Room id 859; the objects are set up when the room is entered. */
	Room0859();
	/** Static descriptor 0x444858. */
	const RoomDescriptor *getDescriptor() override;
	/** Enter, the chest hotspot and the two chest actions. */
	int handleEvent(int event, int param) override;

private:
	Sprite _sprChestLid; ///< 0x47c3c0: chest lid (frame 1 locked, 6 open)
	Sprite _s47c498; ///< 0x47c498: overlay (frame 7) shown while the hero reaches into the chest
};

Room0859::Room0859() : Room(859) {
}

const RoomDescriptor *Room0859::getDescriptor() {
	return findRoomData(_id);
}

int Room0859::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: chest (closed while locked) and the hidden reaching overlay
	case kEvEnter:
		game()->placeObject(&_sprChestLid, 523, 187, 0, game()->flagTest(0x35f) ? 1 : 6, 0, -1);
		game()->placeObjectHidden(&_s47c498, 523, 187, 1, 7, 0);
		return 0;

	// Held item over a hotspot: items 0x25 / 0x29 on the chest pry it open, then find the double bottom
	case kEvItemOnHotspot:
		if (param == 0x25035c || param == 0x29035c) {
			if (game()->flagTest(0x35f))
				return game()->flagTestId(0x35f);
			return game()->flagTestId(0x362) * game()->hasItem(0x27);
		}
		return 0;

	// Action executed
	case kEvAction:
		if (param == 0x35f) {
			// Pry the lock open
			game()->flagClear(0x35f);
			game()->enqueue({
				seqCall(kPrimWalk, { xy(128, 95), 144 }),
				seqCall(kPrimShowRelative, { &hero(), &hero(), 0x28, 0, 0, 0 }),
				seqIf(kPrimIfItem, { 0x29 }),
				seqCall(kPrimSetFrame, { &hero(), 0xa0 }),
				seqElse(),
				seqCall(kPrimSetFrame, { &hero(), 0xd2 }),
				seqEndIf(),
				seqCall(kPrimSoundWait, { 3852 }),
				seqCall(kPrimWait, { 5 }),
				seqCall(kPrimShowRelative, { &hero(), &hero(), -0x28, 0, 0, 0 }),
				seqCall(kPrimSetFrame, { &hero(), 0x90 }),
				seqCall(kPrimSetFrame, { &_sprChestLid, 2 }),
				seqCall(kPrimWalk, { xy(134, 99), 0x90 }),
				seqSpawn(),
				seqCall(kPrimSound, { 860 }),
				seqCall(kPrimAnimate, { &_sprChestLid, 6, 4 }),
				seqJoin(),
				seqCall(kPrimFace, { 0 }),
				seqWaitChildren(),
				seqCall(kPrimDialogue, { 0, 864, 864, 0, 0, 0 }),
				seqEnd()
			});
		} else if (param == 0x362) {
			// Pull at the bottom: the chest has a double bottom
			game()->flagClear(0x362);
			game()->enqueue({
				seqCall(kPrimWalk, { xy(134, 99), 38 }),
				seqCall(kPrimShow, { &_s47c498 }),
				seqCall(kPrimSetFrame, { &hero(), 0x90 }),
				seqCall(kPrimSetFrame, { &hero(), 0x90 }),
				seqCall(kPrimSoundWait, { 3853 }),
				seqCall(kPrimSetFrame, { &hero(), 0x26 }),
				seqCall(kPrimHide, { &_s47c498 }),
				seqCall(kPrimFace, { 0 }),
				seqCall(kPrimDialogue, { 0, 867, 867, 0, 0, 0 }),
				seqEnd()
			});
		}
		// The original falls through into the hotspot case, which ignores these ids
		return 0;

	// Hotspot clicked
	case kEvHotspot:
		// Hotspot 0x35c: the chest - locked, first search (items 0x27, 0x28), pouch under the bottom, or empty
		if (param == 0x35c) {
			if (game()->flagTest(0x35f)) {
				// Locked: "it needs a key"
				game()->enqueue({
					seqCall(kPrimWalk, { xy(134, 99), 144 }),
					seqCall(kPrimWait, { 20 }),
					seqCall(kPrimFace, { 0 }),
					seqCall(kPrimDialogue, { 0, 862, 862, 0, 0, 0 }),
					seqEnd()
				});
				return 1;
			}
			if (!game()->hasItem(0x27)) {
				// First search: take items 0x27 and 0x28
				game()->enqueue({
					seqCall(kPrimWalk, { xy(134, 99), 38 }),
					seqCall(kPrimShow, { &_s47c498 }),
					seqCall(kPrimSetFrame, { &hero(), 0x90 }),
					seqCall(kPrimWait, { 20 }),
					seqCall(kPrimSetFrame, { &hero(), 0x26 }),
					seqCall(kPrimHide, { &_s47c498 }),
					seqCall(kPrimFace, { 0 }),
					seqCall(kPrimDialogue, { 0, 865, 865, 0, 0, 0 }),
					seqSpawn(),
					seqCall(kPrimAddItem, { 0x27 }),
					seqCall(kPrimWait, { 50 }),
					seqCall(kPrimAddItem, { 0x28 }),
					seqJoin(),
					seqWaitChildren(),
					seqEnd()
				});
				return 1;
			}
			if (!game()->flagTest(0x362) && !game()->hasItem(0x16)) {
				// Double bottom found: take the pouch 0x16
				game()->enqueue({
					seqCall(kPrimWalk, { xy(134, 99), 38 }),
					seqCall(kPrimShow, { &_s47c498 }),
					seqCall(kPrimSetFrame, { &hero(), 0x90 }),
					seqCall(kPrimFace, { 0x90 }),
					seqCall(kPrimWait, { 20 }),
					seqCall(kPrimAddItem, { 0x16 }),
					seqCall(kPrimSetFrame, { &hero(), 0x26 }),
					seqCall(kPrimHide, { &_s47c498 }),
					seqCall(kPrimFace, { 0 }),
					seqCall(kPrimDialogue, { 0, 861, 861, 0, 0, 0 }),
					seqEnd()
				});
				return 1;
			}
			// Nothing (more) to find
			game()->enqueue({
				seqCall(kPrimWalk, { xy(134, 99), 38 }),
				seqCall(kPrimShow, { &_s47c498 }),
				seqCall(kPrimSetFrame, { &hero(), 0x90 }),
				seqCall(kPrimFace, { 0x90 }),
				seqCall(kPrimWait, { 20 }),
				seqCall(kPrimSetFrame, { &hero(), 0x26 }),
				seqCall(kPrimHide, { &_s47c498 }),
				seqCall(kPrimFace, { 0 }),
				seqCall(kPrimDialogue, { 0, 871, 871, 0, 0, 0 }),
				seqEnd()
			});
			return 1;
		}
		return 0;

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom0859() {
	return new Room0859();
}

} // End of namespace Gooka
