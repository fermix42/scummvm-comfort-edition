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
 * Room 2619: the computer room of the spaceship (port of the room handler
 * FUN_0041b4e0). Using the complete remote control (item 0x48, "remote
 * control IV") on the computer (hotspot 0xa3c) switches it on and the
 * hero is teleported away (room 3246).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** Room 2619 (original handler FUN_0041b4e0): computer screen and the teleport. */
class Room2619 : public Room {
public:
	Room2619();
	/** Static descriptor 0x441910. */
	const RoomDescriptor *getDescriptor() override;
	/** Enter (screen), remote control on the computer (events 0x10/0x12). */
	int handleEvent(int event, int param) override;

private:
	Sprite _s4769b8; ///< computer screen switched off (sheet 2625, frame 0)
	Sprite _s476da0; ///< computer screen switched on (frame 1), hidden until used
};

Room2619::Room2619() : Room(2619) {
}

const RoomDescriptor *Room2619::getDescriptor() {
	return findRoomData(_id);
}

int Room2619::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: screen off, the lit screen hidden
	case kEvEnter:
		game()->placeObject(&_s4769b8, 0xe1, 0x8c, 0, 0, 0, -1);
		game()->placeObjectHidden(&_s476da0, 0xe1, 0x8c, 0, 1, 0);
		return 0;

	// Item on a hotspot: remote control IV (0x48) on the computer (0xa3c) = "use the remote control" (action 0xa3d)
	case kEvItemOnHotspot:
		if (param == (0x48 << 16 | 0xa3c))
			return 0xa3d;
		return 0;

	// Action executed: 0xa3d - the computer starts up (clicks, beep), the hero dematerialises and is teleported (3246)
	case kEvAction:
		if (param == 0xa3d) {
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x4b, 0x53), 0xa7 }),
				seqCall(kPrimHide, { &_s4769b8 }),
				seqCall(kPrimShow, { &_s476da0 }),
				seqCall(kPrimSoundWait, { 0xee3 }),
				seqCall(kPrimWait, { 1 }),
				seqCall(kPrimSoundWait, { 0xee3 }),
				seqCall(kPrimWait, { 1 }),
				seqCall(kPrimSoundWait, { 0xee3 }),
				seqCall(kPrimWait, { 1 }),
				seqCall(kPrimSoundWait, { 0xee3 }),
				seqCall(kPrimWait, { 1 }),
				seqCall(kPrimSoundWait, { 0xee3 }),
				seqCall(kPrimWait, { 3 }),
				seqCall(kPrimSoundWait, { 0xee4 }),
				seqCall(kPrimWait, { 20 }),
				seqCall(kPrimSound, { 0x831 }),
				seqCall(kPrimSetFrame, { &hero(), 0x95 }),
				seqCall(kPrimWait, { 2 }),
				seqCall(kPrimAnimate, { &hero(), 0x9c, 2 }),
				seqCall(kPrimHide, { &hero() }),
				seqCall(kPrimWait, { 1 }),
				seqCall(kPrimChangeRoom, { 0xcae }),
				seqEnd()
			});
		}
		return 0;

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom2619() {
	return new Room2619();
}

} // End of namespace Gooka
