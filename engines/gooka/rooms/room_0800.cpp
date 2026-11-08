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
 * Room 800: the hall of Gooka's house with the fountain and the doors to
 * all other rooms of the house (port of the room handler FUN_00426b60).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 800 (original handler FUN_00426b60): animated fountain water,
 * the locked door of the sister's room (859), filling the bowl at the
 * fountain and the way out to the porch (927).
 */
class Room0800 : public Room {
public:
	/** Room id 800; the objects are set up when the room is entered. */
	Room0800();
	/** Static descriptor 0x443c98. */
	const RoomDescriptor *getDescriptor() override;
	/** Enter/leave/tick, item actions and the hotspots 0x35b and 0x39f. */
	int handleEvent(int event, int param) override;

private:
	ShimmerSprite _sprFountain;  ///< 0x47bcd8: fountain water (FUN_00417080())
	int _hSndFountain = kNoSound; ///< 0x47bf68: fountain sound loop
};

Room0800::Room0800() : Room(800) {
}

const RoomDescriptor *Room0800::getDescriptor() {
	return findRoomData(_id);
}

int Room0800::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: fountain water and sound; on the morning of the murder go straight to the study
	case kEvEnter:
		game()->placeShimmer(&_sprFountain, 267, 324, 3, 83, 32, 0, 3, 2);
		game()->playSound(&_hSndFountain, 807, 0, true, 0, 0);
		if (game()->flagTest(0x335))
			game()->actOnHotspot(0x331, 0);
		return 0;

	// Room left: stop the fountain sound
	case kEvLeave:
		game()->stopSound(&_hSndFountain);
		return 0;

	// Every 4th tick: ripple the water and cycle its frame 0x17..0x19
	case kEvTick:
		if ((ticks() & 3) == 0) {
			_sprFountain.step();
			// The original also marks the sprite dirty (+0x20 = -1); the scene here redraws everything
			_sprFountain.sheetFrame = (int)ticks() / 4 % 3 + 0x17;
		}
		return 0;

	// Held item over a hotspot: action ids
	case kEvItemOnHotspot:
		// Item 0x1b (key) on the sister's door: unlock it while it is locked
		if (param == 0x1b035b)
			return game()->flagTest(0x324) * 0x324;
		// Item 0x20 (bowl) on the fountain: fill the bowl
		if (param == 0x200327)
			return 0x326;
		return 0;

	// Action executed
	case kEvAction:
		if (param == 0x324) {
			// Unlock the sister's door
			game()->enqueue({
				seqCall(kPrimWalk, { xy(132, 76), 99 }),
				seqCall(kPrimSoundWait, { 804 }),
				seqCall(kPrimFace, { 0 }),
				seqCall(kPrimDialogue, { 0, 0x325, 0x325, 0, 0, 0 }),
				seqCall(kPrimClearFlag, { 0x324 }),
				seqEnd()
			});
		} else if (param == 0x326) {
			// Fill the bowl at the fountain: bowl 0x20 becomes the bowl of water 0x26
			game()->enqueue({
				seqCall(kPrimWalk, { xy(87, 85), 41 }),
				seqCall(kPrimSetFrame, { &hero(), 0x9d }),
				seqCall(kPrimSoundWait, { 806 }),
				seqCall(kPrimDropHeld),
				seqCall(kPrimReplaceItem, { 0x20, 0x26 }),
				seqCall(kPrimSetFrame, { &hero(), 0x29 }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
		}
		// The original falls through into the hotspot case, which ignores these ids
		return 0;

	// Hotspot clicked
	case kEvHotspot:
		switch (param) {
		// Hotspot 859: the sister's door - rattle it while it is locked, else use it as an exit
		case 859:
			if (game()->flagTest(0x324)) {
				game()->enqueue({
					seqCall(kPrimWalk, { xy(132, 76), 100 }),
					seqCall(kPrimSoundWait, { 803 }),
					seqCall(kPrimFace, { 0 }),
					seqCall(kPrimDialogue, { 0, 802, 802, 0, 0, 0 }),
					seqEnd()
				});
				return -1;
			}
			return 0;

		// Hotspot 927: the way out to the porch, at the bottom edge on the hero's side (x 34 or 113)
		case 927: {
			const int x = hero().getGridPos().x >= 80 ? 113 : 34;
			game()->walkToExit(x, 0x5d, -1, x, 99, 927, 1);
			return -1;
		}

		default:
			return 0;
		}

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom0800() {
	return new Room0800();
}

} // End of namespace Gooka
