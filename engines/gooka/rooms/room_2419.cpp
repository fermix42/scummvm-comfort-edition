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
 * Room2419: port of the room handler FUN_004254c0.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 2419 (original handler FUN_004254c0): Gurham's room. Entering
 * without having used the controller (flag 0x96c still set) gets the hero
 * shot (line 0x975) and the game ends behind the closing blinds; with the
 * controller everybody is frozen and the hero can take the fourth shem
 * (item 0x44) from Gurham (hotspot 0x974).
 */
class Room2419 : public Room {
public:
	Room2419();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;

private:
	Sprite _s47b7f8; ///< object at 0x47b7f8 (FUN_0040ff60()): animated when the hero is shot
	Sprite _s47b418; ///< object at 0x47b418 (FUN_0040ff60())
	Sprite _s47b3d0; ///< object at 0x47b3d0 (FUN_0040ff60()): shown for 3 ticks as the shot hits
	Actor _n47b628;  ///< object at 0x47b628 (FUN_0040dc30()): Gurham
	/**
	 * Game-over blinds (FUN_00412d30 / FUN_00412d80). The original uses one
	 * global object (0x473d30, shared with FUN_0041eca0 and room FUN_00423300);
	 * every user resets it on entry, so a per-room instance behaves the same.
	 */
	EndTextScroller _endBlinds;
};

/** Constructs the room statics. */
Room2419::Room2419() : Room(2419) {
}

/** Static descriptor 0x443200. */
const RoomDescriptor *Room2419::getDescriptor() {
	return findRoomData(_id);
}

/** Room events (FUN_004254c0, `what` >= 2). */
int Room2419::handleEvent(int event, int param) {
	switch (event) {
	// Enter: Gurham's room; from 2410 either the hero gets shot or the controller froze everybody
	case kEvEnter:
		game()->placeObject(&_s47b7f8, 0x1c2, 0xb4, 0, 0, 0, -1);
		game()->placeObject(&_s47b418, 0x96, 0x78, 0, 4, 0, -1);
		game()->placeObject(&_n47b628, 0x129, 0x9c, 0, 5, 0, -1);
		game()->placeObjectHidden(&_s47b3d0, 0x199, 0xec, 0, 3, 0);
		_endBlinds.reset();
		if (param == 0x96a) {
			if (game()->flagTest(0x96c)) {
				// Controller not used: Gurham orders the kill (0x975), the hero is shot
				game()->enqueue({
					seqCall(kPrimWait, { 40 }),
					seqCall(kPrimSay, { &_n47b628, 2421, 5, 5, 2 }),
					seqCall(kPrimAnimate, { &_s47b7f8, 2, 3 }),
					seqCall(kPrimWait, { 10 }),
					seqCall(kPrimSound, { 3823 }),
					seqCall(kPrimShow, { &_s47b3d0 }),
					seqCall(kPrimWait, { 3 }),
					seqCall(kPrimSetFrame, { &hero(), 209 }),
					seqCall(kPrimHide, { &_s47b3d0 }),
					seqCall(kPrimWait, { 70 }),
					seqEnd()
				});
				return 0;
			}
			if (game()->flagTest(0x976)) {
				// Controller used: "the controller froze them all" (0x976)
				game()->enqueue({
					seqCall(kPrimWalk, { xy(80, 95), 39 }),
					seqCall(kPrimDialogue, { 0, 2422, 2422, 0, 0, 0 }),
					seqEnd()
				});
				return 0;
			}
		}
		break;

	// Tick: once the kill line (0x975) was said, close the blinds one step per tick (game over)
	case kEvTick:
		if (!game()->flagTest(0x975) && !game()->sequenceBusy()) {
			_endBlinds.addLine();
			return 0;
		}
		break;

	// Hotspot 0x974: Gurham - take the fourth shem (item 0x44) once
	case kEvHotspot:
		if (param == 0x974 && game()->flagTestAndClear(0x977)) {
			game()->enqueue({
				seqCall(kPrimWalk, { xy(80, 75), 166 }),
				seqCall(kPrimWait, { 20 }),
				seqCall(kPrimAddItem, { 68 }),
				seqCall(kPrimFace, { 0 }),
				seqCall(kPrimDialogue, { 0, 2423, 2423, 0, 0, 0 }),
				seqEnd()
			});
		}
		break;

	// Floor click: ignored while the game-over blinds close
	case kEvFloorClick:
		if (!game()->flagTest(0x975) && !game()->sequenceBusy())
			return -1;
		break;

	default:
		break;
	}
	return 0;
}

/** Factory used by the room registry. */
Room *createRoom2419() {
	return new Room2419();
}

} // End of namespace Gooka
