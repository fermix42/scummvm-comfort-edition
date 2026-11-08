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
 * Room1606: port of the room handler FUN_004189f0.
 * An NPC stands in the room while flag 0x846 is set. Clicking him starts
 * one conversation per visit (flag 0x68c); which one depends on the story
 * flags 0x655 and 0x685..0x68a (rooms 1602, 1604 and 1606 share this
 * scheme with their own NPC and lines).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** Room 1606 (original handler FUN_004189f0): room with a talking NPC. */
class Room1606 : public Room {
public:
	Room1606();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;

private:
	/** Enqueues the conversation that matches the story flags (event 7 of the original). */
	void talk();

	Actor _n475f30; ///< object at 0x475f30 (FUN_0040dc30()): the NPC, hotspot 0x68b
};

/** Creates the room and its NPC actor. */
Room1606::Room1606() : Room(1606) {
}

/** Static descriptor DAT_004404d8. */
const RoomDescriptor *Room1606::getDescriptor() {
	return findRoomData(_id);
}

/** Room events: place the NPC on entry, talk to him on click. */
int Room1606::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: the NPC is there while flag 0x846 is set; allow one talk per visit
	case kEvEnter:
		if (game()->flagTest(0x846))
			game()->placeObjectIfFlag(0x68b, &_n475f30, 0x88, 0x9b, 0, 0, 0);
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
void Room1606::talk() {
	if (game()->flagTest(0x655)) {
		game()->enqueue({
			seqCall(kPrimWalk, { xy(76, 87), 34 }),
			seqCall(kPrimDialogue, { 0, 0x660, 0x660, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n475f30, 0x67d, 0, 0, 2 }),
			seqCall(kPrimSetFrame, { &_n475f30, 0 }),
			seqCall(kPrimFace, { 0 }),
			seqEnd()
		});
	} else if (game()->flagTest(0x685)) {
		game()->enqueue({
			seqCall(kPrimWalk, { xy(76, 87), 34 }),
			seqCall(kPrimDialogue, { 0, 0x661, 0x661, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n475f30, 0x67e, 0, 0, 2 }),
			seqCall(kPrimDialogue, { 0, 0x662, 0x662, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n475f30, 0x681, 0, 0, 2 }),
			seqCall(kPrimDialogue, { 0, 0x66d, 0x66d, 0, 0, 0 }),
			seqCall(kPrimFace, { 0 }),
			seqEnd()
		});
	} else if (game()->flagTest(0x686)) {
		game()->enqueue({
			seqCall(kPrimWalk, { xy(76, 87), 34 }),
			seqCall(kPrimDialogue, { 0, 0x663, 0x663, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n475f30, 0x680, 1, 1, 2 }),
			seqCall(kPrimDialogue, { 0, 0x667, 0x667, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n475f30, 0x681, 0, 0, 2 }),
			seqCall(kPrimDialogue, { 0, 0x668, 0x668, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n475f30, 0x682, 0, 0, 2 }),
			seqCall(kPrimDialogue, { 0, 0x66d, 0x66d, 0, 0, 0 }),
			seqCall(kPrimFace, { 0 }),
			seqEnd()
		});
	} else if (game()->flagTest(0x687)) {
		game()->enqueue({
			seqCall(kPrimWalk, { xy(76, 87), 34 }),
			seqCall(kPrimDialogue, { 0, 0x666, 0x666, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n475f30, 0x67f, 0, 0, 2 }),
			seqCall(kPrimDialogue, { 0, 0x669, 0x669, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n475f30, 0x687, 0, 0, 2 }),
			seqCall(kPrimDialogue, { 0, 0x66d, 0x66d, 0, 0, 0 }),
			seqCall(kPrimFace, { 0 }),
			seqEnd()
		});
	} else if (game()->flagTest(0x688)) {
		game()->enqueue({
			seqCall(kPrimWalk, { xy(76, 87), 34 }),
			seqCall(kPrimDialogue, { 0, 0x666, 0x666, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n475f30, 0x680, 0, 0, 2 }),
			seqCall(kPrimDialogue, { 0, 0x66a, 0x66a, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n475f30, 0x682, 0, 0, 2 }),
			seqCall(kPrimDialogue, { 0, 0x66d, 0x66d, 0, 0, 0 }),
			seqCall(kPrimFace, { 0 }),
			seqEnd()
		});
	} else if (game()->flagTest(0x689)) {
		game()->enqueue({
			seqCall(kPrimWalk, { xy(76, 87), 34 }),
			seqCall(kPrimDialogue, { 0, 0x666, 0x666, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n475f30, 0x67f, 0, 0, 2 }),
			seqCall(kPrimDialogue, { 0, 0x66b, 0x66b, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n475f30, 0x682, 0, 0, 2 }),
			seqCall(kPrimDialogue, { 0, 0x66d, 0x66d, 0, 0, 0 }),
			seqCall(kPrimFace, { 0 }),
			seqEnd()
		});
	} else if (game()->flagTest(0x68a)) {
		game()->enqueue({
			seqCall(kPrimWalk, { xy(76, 87), 34 }),
			seqCall(kPrimDialogue, { 0, 0x666, 0x666, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n475f30, 0x67f, 0, 0, 2 }),
			seqCall(kPrimDialogue, { 0, 0x66c, 0x66c, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n475f30, 0x682, 0, 0, 2 }),
			seqCall(kPrimDialogue, { 0, 0x66e, 0x66e, 0, 0, 0 }),
			seqCall(kPrimFace, { 0 }),
			seqEnd()
		});
	}
}

/** Factory used by the room registry. */
Room *createRoom1606() {
	return new Room1606();
}

} // End of namespace Gooka
