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
 * Room1619: port of the room handler FUN_004195c0.
 * Shimmering water, an ambient loop and, while flag 0x846 is set, an NPC
 * (hotspot 0x654) who greets the hero once (flag 0x655) and later hands
 * over item 0x39 (flag 0x65f) unless flag 0x68a is set.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** Room 1619 (original handler FUN_004195c0): the NPC with item 0x39. */
class Room1619 : public Room {
public:
	Room1619();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;

private:
	ShimmerSprite _e4762d0; ///< object at 0x4762d0 (FUN_00412690(0x10, 0xffffffff))
	int _h4768b0 = kNoSound; ///< object at 0x4768b0 (FUN_0040a6f0()): ambient loop 0xd48
	Sprite _s476660; ///< object at 0x476660 (FUN_0040ff60()): object next to the NPC
	Actor _n475fe0; ///< object at 0x475fe0 (FUN_0040dc30()): the NPC
};

/** Creates the room objects; the shimmer gets amplitude 16 and direction -1. */
Room1619::Room1619() : Room(1619), _e4762d0(16, -1) {
}

/** Static descriptor DAT_00440918. */
const RoomDescriptor *Room1619::getDescriptor() {
	return findRoomData(_id);
}

/** Room events: shimmer, ambient loop and the NPC. */
int Room1619::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: shimmer, ambient loop; NPC and his object while flag 0x846 is set
	case kEvEnter:
		game()->placeShimmer(&_e4762d0, 0x17c, 0x72, -1, 0x50, 0x28, 0, 0x10, -1);
		game()->playSound(&_h4768b0, 0xd48, 0, true, -4000, 0);
		if (game()->flagTest(0x846)) {
			game()->placeObjectIfFlag(0x654, &_s476660, 0x46, 0xbe, 0, 1, 2);
			game()->placeObjectIfFlag(0x654, &_n475fe0, 0x46, 0x96, 0, 2, 1);
			return 0;
		}
		game()->placeObject(&_s476660, 0x46, 0xbe, 0, 1, 2, -1);
		return 0;
	// Room left: ambient loop off
	case kEvLeave:
		game()->stopSound(&_h4768b0);
		return 0;
	// Tick: shimmer step every 3rd tick
	case kEvTick:
		if (ticks() % 3 == 0)
			_e4762d0.step();
		return 0;
	// Hotspot clicked
	case kEvHotspot:
		// Hotspot 0x654: the NPC - first the greeting, later item 0x39 (once each)
		if (param == 0x654) {
			if (game()->flagTestAndClear(0x655)) {
				// FUN_0040f760(0x36, 0x5f, 0x22, npc, 2, 2, 2, 0x655, 0x65b): walk, dialogue, face front
				game()->enqueue({
					seqCall(kPrimWalk, { xy(54, 95), 34 }),
					seqCall(kPrimDialogue, { &_n475fe0, 0x655, 0x65b, 2, 2, 2 }),
					seqCall(kPrimFace, { 0 }),
					seqEnd()
				});
				return 0;
			}
			if (!game()->flagTest(0x68a) && game()->flagTestAndClear(0x65f)) {
				game()->enqueue({
					seqCall(kPrimWalk, { xy(54, 95), 34 }),
					seqCall(kPrimDialogue, { &_n475fe0, 0x65c, 0x65e, 2, 2, 2 }),
					seqCall(kPrimAddItem, { 0x39 }),
					seqCall(kPrimFace, { 0 }),
					seqEnd()
				});
			}
		}
		return 0;
	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom1619() {
	return new Room1619();
}

} // End of namespace Gooka
