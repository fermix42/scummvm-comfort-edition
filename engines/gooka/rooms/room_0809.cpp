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
 * Room 809: the room with the father's safe (port of the room handler
 * FUN_00427a70).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 809 (original handler FUN_00427a70): the safe (0x32a) can be opened
 * with the key or its back smashed with the mace; behind it is the
 * Marin-Danim plate (0x32b), decoded with the controller, that leads to
 * room 3229. Flag -811 = back of the safe destroyed, -815 = plate decoded.
 */
class Room0809 : public Room {
public:
	/** Room id 809; the objects are set up when the room is entered. */
	Room0809();
	/** Static descriptor 0x444350. */
	const RoomDescriptor *getDescriptor() override;
	/** Enter, the safe and plate hotspots, item actions and hotspot overrides. */
	int handleEvent(int event, int param) override;

private:
	Sprite _sprSafe; ///< 0x47c2c8: safe door (frame 3 while locked, 6 open)
	Sprite _s47c280; ///< 0x47c280: destroyed back of the safe (shown while flag -811 is set)
	Sprite _s47c408; ///< 0x47c408: mace blow animation
};

Room0809::Room0809() : Room(809) {
}

const RoomDescriptor *Room0809::getDescriptor() {
	return findRoomData(_id);
}

int Room0809::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: safe door, smashed back, hidden mace animation; coming back from the plate, face front
	case kEvEnter:
		game()->placeObject(&_sprSafe, 10, 0x5c, 0, game()->flagTest(0x32e) ? 3 : 6, 5, -1);
		game()->placeObjectIfFlag(-811, &_s47c280, 0xe, 0x67, 0, 7, 0);
		game()->placeObjectHidden(&_s47c408, 0xc, 0x66, 0, 8, 1);
		if (param == 0xc9d) {
			game()->enqueue({
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
		}
		return 0;

	// Hotspot clicked
	case kEvHotspot:
		// Hotspot 0x32a: the safe - comment on its hard metal (flag 0x32c is never cleared)
		if (param == 0x32a) {
			if (game()->flagTest(0x32c))
				game()->sayIdle(0xdf1);
		} else if (param == 0x32b) {
			// Hotspot 0x32b: the decoded plate - step through to room 3229
			game()->enqueue({
				seqCall(kPrimWalk, { xy(17, 72), 168 }),
				seqCall(kPrimChangeRoom, { 0xc9d }),
				seqEnd()
			});
			return 1;
		}
		return 0;

	// Held item over a hotspot: action ids
	case kEvItemOnHotspot:
		// Item 3 (mace) on the safe: destroy its back while items 0xe and 0xf are gone
		if (param == 0x3032a) {
			if (!game()->flagTest(0xf) && !game()->flagTest(0xe))
				return 0x32c;
		} else if (param == 0x10032a) {
			// Item 0x10 (key) on the safe: open it while it is closed
			return game()->flagTestId(0x32e);
		} else if (param == 0x45032b) {
			// Item 0x45 (controller) on the plate: decode it once
			if (!game()->flagTest(-815))
				return 0x32f;
		}
		return 0;

	// Action executed
	case kEvAction:
		if (param == 0x32c) {
			// Smash the back of the safe with the mace
			if (!game()->flagTest(0xf) && !game()->flagTest(0xe)) {
				game()->flagSet(-811, true);
				game()->enqueue({
					seqCall(kPrimDialogue, { 0, 816, 816, 0, 0, 0 }),
					seqCall(kPrimWalk, { xy(14, 70), 167 }),
					seqCall(kPrimWait, { 20 }),
					seqCall(kPrimShow, { &_s47c280 }),
					seqCall(kPrimShow, { &_s47c408 }),
					seqCall(kPrimSound, { 3835 }),
					seqCall(kPrimAnimate, { &_s47c408, 0xb, 3 }),
					seqCall(kPrimAnimate, { &_s47c408, 9, 4 }),
					seqCall(kPrimAnimate, { &_s47c408, 0xa, 3 }),
					seqCall(kPrimAnimate, { &_s47c408, 8, 4 }),
					seqCall(kPrimAnimate, { &_s47c408, 0xb, 3 }),
					seqCall(kPrimAnimate, { &_s47c408, 9, 4 }),
					seqCall(kPrimAnimate, { &_s47c408, 0xa, 3 }),
					seqCall(kPrimAnimate, { &_s47c408, 9, 4 }),
					seqCall(kPrimAnimate, { &_s47c408, 0xe, 4 }),
					seqCall(kPrimHide, { &_s47c408 }),
					seqCall(kPrimWait, { 20 }),
					seqCall(kPrimFace, { 0 }),
					seqEnd()
				});
				return 0;
			}
		} else if (param == 0x32e) {
			// Unlock the safe with the key
			game()->enqueue({
				seqCall(kPrimWalk, { xy(24, 72), 168 }),
				seqCall(kPrimSoundWait, { 804 }),
				seqCall(kPrimSound, { 3858 }),
				seqCall(kPrimAnimate, { &_sprSafe, 6, 4 }),
				seqCall(kPrimFace, { 0 }),
				seqCall(kPrimClearFlag, { 0x32e }),
				seqEnd()
			});
			return 0;
		} else if (param == 0x32f) {
			// Decode the plate with the controller and step through to room 3229
			game()->flagSet(-815, true);
			game()->enqueue({
				seqCall(kPrimDialogue, { 0, 0x32d, 0x32d, 0, 0, 0 }),
				seqCall(kPrimWalk, { xy(17, 72), 168 }),
				seqCall(kPrimChangeRoom, { 0xc9d }),
				seqEnd()
			});
			return 0;
		}
		return 0;

	// Hotspot lookup: which id the safe area (0xe, 0xf, 0x32a) stands for
	case kEvHotspotOverride:
		if (param == 0xe || param == 0xf || param == 0x32a) {
			// Back destroyed: the plate; else item 9 while it is there
			if (game()->flagTest(-811))
				return 0x32b;
			if (game()->flagTest(9))
				return 9;
		}
		// Items 0xf / 0xe taken: their spots become the safe
		if (param == 0xf && !game()->flagTest(0xf))
			return 0x32a;
		if (param == 0xe && !game()->flagTest(0xe))
			return 0x32a;
		// Safe still closed: 0xe and 0xf are part of it
		if (game()->flagTest(0x32e) && (param == 0xe || param == 0xf))
			return 0x32a;
		return 0;

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom0809() {
	return new Room0809();
}

} // End of namespace Gooka
