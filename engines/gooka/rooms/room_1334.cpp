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
 * Room 1334: Digor's flat in Avaral (port of the room handler
 * FUN_00416bc0).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 1334 (original handler FUN_00416bc0): the confused old man Digor
 * (hotspot 0x537, present while the flag is set) mumbles at random; giving
 * him item 0x3e (the fruit) cures him, he tells the story of the triumvirate,
 * hands over his sheme (item 0x41) and leaves.
 */
class Room1334 : public Room {
public:
	/** Room id 1334; the objects are set up when the room is entered. */
	Room1334();
	/** Static descriptor 0x43fc80. */
	const RoomDescriptor *getDescriptor() override;
	/** Enter/leave/tick and giving the fruit. */
	int handleEvent(int event, int param) override;

private:
	/** Mumbling: random sounds 0x539..0x53c, frame toggles while one plays (tail of events 4 and 5). */
	void mumble();

	Actor _n4745f8;          ///< 0x4745f8: Digor
	int _h4753ac = kNoSound; ///< 0x4753ac: his mumbling
};

Room1334::Room1334() : Room(1334) {
}

const RoomDescriptor *Room1334::getDescriptor() {
	return findRoomData(_id);
}

void Room1334::mumble() {
	if (!game()->flagTest(0x537))
		return;
	if (game()->isSoundPlaying(&_h4753ac)) {
		// Mumbling: toggle between frames 0 and 1 now and then
		if (game()->rnd(6) == 0)
			_n4745f8.setFrame(_n4745f8.frame == 0);
		return;
	}
	// Silent: frame 1; 1 % chance per tick to start another mumble
	_n4745f8.setFrame(1);
	if (game()->rnd(100) == 0)
		game()->playSound(&_h4753ac, 0x539 + game()->rnd(4), 0, false, 0, 0);
}

int Room1334::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: Digor is there until he leaves after the fruit
	case kEvEnter:
		game()->placeObjectIfFlag(0x537, &_n4745f8, 0x198, 0x5c, 0, 0, 0);
		return 0;

	// Room left: stop the mumbling; the original then falls through into the tick case
	case kEvLeave:
		game()->stopSound(&_h4753ac);
		mumble();
		return 0;

	// Every tick: mumbling
	case kEvTick:
		mumble();
		return 0;

	// Held item over a hotspot: item 0x3e (fruit) on Digor gives it to him
	case kEvItemOnHotspot:
		if (param == 0x3e0537)
			return 0x565;
		return 0;

	// Action executed: give Digor the fruit - he wakes up, explains everything and hands over his sheme
	case kEvAction:
		if (param == 0x565) {
			game()->consumeHeldItem();
			game()->enqueue({
				seqCall(kPrimClearFlag, { 0x537 }),
				seqCall(kPrimWalk, { xy(0x88, 0x50), 0x27 }),
				seqCall(kPrimWaitHandle, { &_h4753ac }),
				seqCall(kPrimDialogue, { 0, 0x53d, 0x53d, 0, 0, 0 }),
				seqCall(kPrimFace, { 0xaa }),
				seqCall(kPrimSoundWait, { 0xf06 }),
				seqCall(kPrimSetFrame, { &_n4745f8, 1 }),
				seqCall(kPrimFace, { 0x27 }),
				seqSpawn(),
				seqCall(kPrimWait, { 0xa }),
				seqCall(kPrimFace, { 0x22 }),
				seqJoin(),
				seqCall(kPrimWait, { 0xa }),
				seqCall(kPrimSound, { 0xf2a }),
				seqCall(kPrimAnimate, { &_n4745f8, 7, 5 }),
				seqWaitChildren(),
				seqCall(kPrimDialogue, { &_n4745f8, -0x53e, 0x54c, 7, 7, 2 }),
				seqCall(kPrimSay, { &_n4745f8, 0x54d, 7, 7, 2 }),
				seqCall(kPrimSay, { &_n4745f8, 0x54e, 7, 7, 2 }),
				seqCall(kPrimDialogue, { &_n4745f8, -0x54f, 0x553, 7, 7, 2 }),
				seqCall(kPrimDialogue, { &_n4745f8, -0x554, 0x558, 7, 7, 2 }),
				seqCall(kPrimDialogue, { &_n4745f8, -0x559, 0x55a, 7, 7, 2 }),
				seqCall(kPrimSetFrame, { &hero(), 0xa1 }),
				seqCall(kPrimWait, { 3 }),
				seqCall(kPrimSetFrame, { &hero(), 0xa2 }),
				seqCall(kPrimSay, { &_n4745f8, 0x55b, 7, 7, 2 }),
				seqCall(kPrimSetFrame, { &hero(), 0xa1 }),
				seqCall(kPrimWait, { 3 }),
				seqCall(kPrimSetFrame, { &hero(), 0x22 }),
				seqCall(kPrimDialogue, { &_n4745f8, 0x55c, 0x55f, 7, 7, 2 }),
				seqCall(kPrimDialogue, { &_n4745f8, -0x560, 0x564, 7, 7, 2 }),
				seqCall(kPrimAddItem, { 0x41 }),
				seqCall(kPrimMoveAxis, { &_n4745f8, 0, 0xfa, 8, 9, 0x10, 5 }),
				seqCall(kPrimHide, { &_n4745f8 }),
				seqEnd()
			});
		}
		return 0;

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom1334() {
	return new Room1334();
}

} // End of namespace Gooka
