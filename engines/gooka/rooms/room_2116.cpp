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
 * Room2116: port of the room handler FUN_004224d0 (the exit of the maze:
 * a cut-scene with the NPC _n47acc8 that ends in room 3243).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** Room 2116 (original handler FUN_004224d0). */
class Room2116 : public Room {
public:
	/** Room 2116; the members are the original statics. */
	Room2116();
	/** Static descriptor DAT_00442698. */
	const RoomDescriptor *getDescriptor() override;
	/** The room body (what >= 2). */
	int handleEvent(int event, int param) override;

private:
	ShimmerSprite _e479ed0; ///< shimmer over sheet frame 1 (FUN_00412690(0x10, 0xffffffff))
	ShimmerSprite _e47a620; ///< shimmer over sheet frame 0 (FUN_00412690(0x10, 0xffffffff))
	int _h47aa94 = kNoSound; ///< ambient sound 0xd48
	Actor _n47acc8; ///< the NPC, frame 8 of the room sheet, arrives from the left
	Sprite _s479e78; ///< animated object, frames 9..12
};

Room2116::Room2116() : Room(2116), _e479ed0(16, -1), _e47a620(16, -1) {
}

const RoomDescriptor *Room2116::getDescriptor() {
	return findRoomData(_id);
}

int Room2116::handleEvent(int event, int param) {
	switch (event) {
	// Enter: shimmers, NPC, object, ambient sound; the hero climbs out of the maze and the
	// NPC arrives and talks (lines 2117-2156, item 0x32 becomes 0x33), then off to room 3243
	case kEvEnter:
		game()->placeShimmer(&_e479ed0, 0xf4, 0xae, -1, 0xe6, 0x24, 1, 0x10, -1);
		game()->placeShimmer(&_e47a620, 0x13, 0xdc, -1, 0x104, 0xbe, 0, 0x10, -1);
		game()->placeObject(&_n47acc8, -0x10a, 100, 0, 8, 0, -1);
		game()->placeObject(&_s479e78, 0x14, 0x155, 0, 8, 1, -1);
		game()->playSound(&_h47aa94, 0xd48, 0, true, -2000, 0);
		hero().place(0x7f, 0x5d, 0x9c);
		game()->enqueue({
			seqCall(kPrimWait, { 1 }),
			seqSpawn(),
			seqCall(kPrimSound, { 0x830 }),
			seqCall(kPrimAnimate, { &hero(), 0x95, 2 }),
			seqCall(kPrimSetFrame, { &hero(), 0x27 }),
			seqCall(kPrimWalk, { xy(0x5f, 0x5e), 0x77 }),
			seqJoin(),
			seqCall(kPrimSound, { 0xf1e }),
			seqCall(kPrimMoveLinear, { &_n47acc8, 0x12, 8, 1, 0, 1 }),
			seqCall(kPrimMoveLinear, { &_n47acc8, 8, 6, 1, 0, 1 }),
			seqCall(kPrimMoveLinear, { &_n47acc8, 8, 4, 1, 0, 1 }),
			seqCall(kPrimMoveLinear, { &_n47acc8, 8, 3, 1, 0, 1 }),
			seqCall(kPrimMoveLinear, { &_n47acc8, 8, 2, 1, 0, 1 }),
			seqCall(kPrimMoveLinear, { &_n47acc8, 6, 1, 1, 0, 1 }),
			seqCall(kPrimAnimate, { &_n47acc8, 7, 4 }),
			seqCall(kPrimShowRelative, { &_n47acc8, &_n47acc8, -0x24, 0, 0, 0 }),
			seqCall(kPrimAnimate, { &_n47acc8, 5, 4 }),
			seqWaitChildren(),
			seqCall(kPrimDialogue, { 0, 0x845, 0x845, 0, 0, 0 }),
			seqCall(kPrimSay, { &_n47acc8, 0x846, 2, 2, 2 }),
			seqCall(kPrimFace, { 0x22 }),
			seqCall(kPrimDialogue, { &_n47acc8, 0x847, 0x860, 2, 2, 2 }),
			seqIf(kPrimIfNotFlag, { 0x32 }),
			seqCall(kPrimDialogue, { 0, 0x861, 0x861, 0, 0, 0 }),
			seqCall(kPrimSetFrame, { &hero(), 0xa1 }),
			seqCall(kPrimWait, { 3 }),
			seqCall(kPrimSetFrame, { &hero(), 0xa2 }),
			seqCall(kPrimSay, { &_n47acc8, 0x862, 2, 2, 2 }),
			seqCall(kPrimSetFrame, { &hero(), 0xa1 }),
			seqCall(kPrimWait, { 3 }),
			seqCall(kPrimSetFrame, { &hero(), 0x22 }),
			seqCall(kPrimDialogue, { &_n47acc8, 0x863, 0x866, 2, 2, 2 }),
			seqCall(kPrimReplaceItemQuiet, { 0x32, 0x33 }),
			seqEndIf(),
			seqCall(kPrimDialogue, { &_n47acc8, 0x867, 0x86c, 2, 2, 2 }),
			seqCall(kPrimAddItem, { 0x34 }),
			seqCall(kPrimSetFlag, { -0xcb1 }),
			seqCall(kPrimChangeRoom, { 0xcab }),
			seqEnd()
		});
		return 0;

	// Leave: stop the ambient sound
	case kEvLeave:
		game()->stopSound(&_h47aa94);
		return 0;

	// Tick: the shimmers take turns; the object animates with the second one
	case kEvTick: {
		const int t = (int)ticks();
		if (t % 3 == 0)
			_e479ed0.step();
		if (t % 3 == 1) {
			_e47a620.step();
			_s479e78.setFrame(t / 3 % 4 + 9);
		}
		return 0;
	}

	default:
		break;
	}
	return 0;
}

/** Creates the handler (registered in rooms/ported.cpp). */
Room *createRoom2116() {
	return new Room2116();
}

} // End of namespace Gooka
