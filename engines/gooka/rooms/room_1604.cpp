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
 * Room1604: port of the room handler FUN_004183b0.
 * An NPC stands in the room while flag 0x846 is set. Clicking him starts
 * one conversation per visit (flag 0x68c); which one depends on the story
 * flags 0x655 and 0x685..0x68a (rooms 1602, 1604 and 1606 share this
 * scheme with their own NPC and lines).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** Room 1604 (original handler FUN_004183b0): room with a talking NPC. */
class Room1604 : public Room {
public:
	Room1604();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;

private:
	/** Enqueues the conversation that matches the story flags (event 7 of the original). */
	void talk();

	Actor _n4768b8; ///< object at 0x4768b8 (FUN_0040dc30()): the NPC, hotspot 0x68b
};

/** Creates the room and its NPC actor. */
Room1604::Room1604() : Room(1604) {
}

/** Static descriptor DAT_004403c0. */
const RoomDescriptor *Room1604::getDescriptor() {
	return findRoomData(_id);
}

/** Room events: place the NPC on entry, talk to him on click. */
int Room1604::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: the NPC is there while flag 0x846 is set; allow one talk per visit
	case kEvEnter:
		if (game()->flagTest(0x846))
			game()->placeObjectIfFlag(0x68b, &_n4768b8, 0x1ce, 0xaf, 0, 0, 0);
		game()->flagSet(0x68c, true);
		return 0;
	// Hotspot clicked
	case kEvHotspot:
		// Hotspot 0x68b: the NPC - one conversation per visit (the original returns 0 here too)
		if (param == 0x68b && game()->flagTest(0x68c)) {
			game()->flagClear(0x68c);
			talk();
		}
		return 0;
	default:
		return 0;
	}
}

/** Picks the conversation: flag 0x655 first, then the first set flag of 0x685..0x68a. */
void Room1604::talk() {
	if (game()->flagTest(0x655)) {
		game()->enqueue({
			seqCall(kPrimWalk, { xy(80, 95), 36 }),
			seqCall(kPrimDialogue, { 0, 0x660, 0x660, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n4768b8, 0x676, 1, 1, 2 }),
			seqCall(kPrimSetFrame, { &_n4768b8, 0 }),
			seqCall(kPrimFace, { 0 }),
			seqEnd()
		});
	} else if (game()->flagTest(0x685)) {
		game()->enqueue({
			seqCall(kPrimWalk, { xy(80, 95), 36 }),
			seqCall(kPrimDialogue, { 0, 0x661, 0x661, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n4768b8, 0x677, 1, 1, 2 }),
			seqCall(kPrimDialogue, { 0, 0x662, 0x662, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n4768b8, 0x684, 1, 1, 2 }),
			seqCall(kPrimDialogue, { 0, 0x667, 0x667, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n4768b8, 0x685, 1, 1, 2 }),
			seqCall(kPrimDialogue, { 0, 0x66d, 0x66d, 0, 0, 0 }),
			seqCall(kPrimSetFrame, { &_n4768b8, 0 }),
			seqCall(kPrimFace, { 0 }),
			seqEnd()
		});
	} else if (game()->flagTest(0x686)) {
		game()->enqueue({
			seqCall(kPrimWalk, { xy(80, 95), 36 }),
			seqCall(kPrimDialogue, { 0, 0x663, 0x663, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n4768b8, 0x679, 1, 1, 2 }),
			seqCall(kPrimDialogue, { 0, 0x667, 0x667, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n4768b8, 0x67a, 1, 1, 2 }),
			seqCall(kPrimDialogue, { 0, 0x668, 0x668, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n4768b8, 0x67b, 1, 1, 2 }),
			seqCall(kPrimDialogue, { 0, 0x66d, 0x66d, 0, 0, 0 }),
			seqCall(kPrimFace, { 0 }),
			seqEnd()
		});
	} else if (game()->flagTest(0x687)) {
		game()->enqueue({
			seqCall(kPrimWalk, { xy(80, 95), 36 }),
			seqCall(kPrimDialogue, { 0, 0x666, 0x666, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n4768b8, 0x678, 1, 1, 2 }),
			seqCall(kPrimDialogue, { 0, 0x669, 0x669, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n4768b8, 0x67b, 1, 1, 2 }),
			seqCall(kPrimDialogue, { 0, 0x66d, 0x66d, 0, 0, 0 }),
			seqCall(kPrimFace, { 0 }),
			seqEnd()
		});
	} else if (game()->flagTest(0x688)) {
		game()->enqueue({
			seqCall(kPrimWalk, { xy(80, 95), 36 }),
			seqCall(kPrimDialogue, { 0, 0x666, 0x666, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n4768b8, 0x679, 1, 1, 2 }),
			seqCall(kPrimDialogue, { 0, 0x66a, 0x66a, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n4768b8, 0x688, 1, 1, 2 }),
			seqCall(kPrimDialogue, { 0, 0x66d, 0x66d, 0, 0, 0 }),
			seqCall(kPrimFace, { 0 }),
			seqEnd()
		});
	} else if (game()->flagTest(0x689)) {
		game()->enqueue({
			seqCall(kPrimWalk, { xy(80, 95), 36 }),
			seqCall(kPrimDialogue, { 0, 0x666, 0x666, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n4768b8, 0x678, 1, 1, 2 }),
			seqCall(kPrimDialogue, { 0, 0x66b, 0x66b, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n4768b8, 0x67b, 1, 1, 2 }),
			seqCall(kPrimDialogue, { 0, 0x66d, 0x66d, 0, 0, 0 }),
			seqCall(kPrimFace, { 0 }),
			seqEnd()
		});
	} else if (game()->flagTest(0x68a)) {
		game()->enqueue({
			seqCall(kPrimWalk, { xy(80, 95), 36 }),
			seqCall(kPrimDialogue, { 0, 0x666, 0x666, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n4768b8, 0x678, 1, 1, 2 }),
			seqCall(kPrimDialogue, { 0, 0x66c, 0x66c, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n4768b8, 0x68a, 1, 1, 2 }),
			seqCall(kPrimDialogue, { 0, 0x66e, 0x66e, 0, 0, 0 }),
			seqCall(kPrimFace, { 0 }),
			seqEnd()
		});
	}
}

/** Factory used by the room registry. */
Room *createRoom1604() {
	return new Room1604();
}

} // End of namespace Gooka
