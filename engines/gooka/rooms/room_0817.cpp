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
 * Room 817: the father's study with his body, the judge, the sister Baroda
 * and the guard Albert (port of the room handler FUN_00428060).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 817 (original handler FUN_00428060): the opening interrogation
 * (first visit, flag 0x335), the key in the dead father's hand (0x332),
 * showing the pouch to the judge (0x346) after which Albert takes the
 * sister away (flag 0x348 = both still here), and the judge stopping the
 * hero from walking into the room while flag 0x350 is set.
 */
class Room0817 : public Room {
public:
	/** Room id 817; the objects are set up when the room is entered. */
	Room0817();
	/** Static descriptor 0x444548. */
	const RoomDescriptor *getDescriptor() override;
	/** Enter/leave/tick, the body, floor clicks, the pouch action and a hotspot override. */
	int handleEvent(int event, int param) override;

private:
	/** Fireplace animation, frame (ticks / 4) % 8 (shared tail of events 5 and 0x12). */
	void animateFire();

	Sprite _sprFire; ///< 0x47bf20: fire in the fireplace (8-frame loop)
	Sprite _s47c020; ///< 0x47c020: static object (frame 0x11)
	Sprite _s47c450; ///< 0x47c450: constructed by the original but never used
	Actor _actFather;  ///< 0x47bf70: the father's body (hotspot 0x332)
	Actor _actBaroda;  ///< 0x47c4e0: the sister Baroda
	Actor _actJudge;  ///< 0x47c310: the judge (hotspot 0x333)
	Actor _actGuard;  ///< 0x47c1d0: the guard Albert
	int _hSndFire = kNoSound; ///< 0x47bccc: ambient sound loop
};

Room0817::Room0817() : Room(817) {
}

const RoomDescriptor *Room0817::getDescriptor() {
	return findRoomData(_id);
}

void Room0817::animateFire() {
	_sprFire.setFrame((int)ticks() / 4 % 8);
}

int Room0817::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: ambient loop, objects and people; the first time the sister accuses the hero
	case kEvEnter:
		game()->playSound(&_hSndFire, 3824, 0, true, 0, 0);
		game()->placeObject(&_sprFire, 94, 186, 0, 0, 0, -1);
		game()->placeObjectIfFlag(0x332, &_actFather, 260, 253, 1, 8, 1);
		game()->placeObject(&_s47c020, 308, 260, 1, 0x11, 0, -1);
		game()->placeObjectIfFlagNoHotspot(0x348, &_actBaroda, 250, 120, 0, 9, 500);
		game()->placeObjectIfFlagNoHotspot(0x348, &_actGuard, 250, 120, 0, 0x1b, 0x1f5);
		game()->placeObjectIfFlag(0x333, &_actJudge, 40, 150, 0, 0xe, 2);
		if (game()->flagTestAndClear(0x335)) {
			game()->enqueue({
				seqCall(kPrimPlaceActor, { &hero(), xy(159, 76), 1 }),
				seqCall(kPrimWalk, { xy(150, 76), 0x22 }),
				seqCall(kPrimSay, { &_actBaroda, 821, 0xc, 0xc, 2 }),
				seqCall(kPrimAnimate, { &_actBaroda, 9, 5 }),
				seqCall(kPrimSay, { &_actJudge, 822, 0xe, 0xe, 2 }),
				seqCall(kPrimSay, { &_actBaroda, 823, 9, 9, 2 }),
				seqCall(kPrimSay, { &_actJudge, 824, 0xe, 0xe, 2 }),
				seqCall(kPrimDialogue, { 0, 825, 825, 0, 0, 0 }),
				seqCall(kPrimSay, { &_actJudge, 826, 0xe, 0xe, 2 }),
				seqCall(kPrimDialogue, { 0, 827, 827, 0, 0, 0 }),
				seqCall(kPrimSay, { &_actBaroda, 828, 9, 9, 2 }),
				seqCall(kPrimSay, { &_actJudge, 829, 0xe, 0xe, 2 }),
				seqCall(kPrimSay, { &_actJudge, 830, 0xe, 0xe, 2 }),
				seqCall(kPrimSay, { &_actBaroda, 831, 9, 9, 2 }),
				seqCall(kPrimDialogue, { &_actJudge, -0x340, 837, 0xe, 0xe, 2 }),
				seqEnd()
			});
		}
		return 0;

	// Room left: stop the ambient loop
	case kEvLeave:
		game()->stopSound(&_hSndFire);
		return 0;

	// Every tick: fireplace animation
	case kEvTick:
		animateFire();
		return 0;

	// Hotspot clicked
	case kEvHotspot:
		// Hotspot 0x332: the father's body - take the key from his hand (once)
		if (param == 0x332) {
			if (game()->flagTest(0x334)) {
				game()->enqueue({
					seqCall(kPrimWalk, { xy(58, 94), 36 }),
					seqCall(kPrimDialogue, { 0, 820, 820, 0, 0, 0 }),
					seqCall(kPrimFace, { 0x62 }),
					seqCall(kPrimWait, { 5 }),
					seqCall(kPrimAddItem, { 0x10 }),
					seqCall(kPrimWait, { 5 }),
					seqCall(kPrimFace, { 0 }),
					seqEnd()
				});
			}
			return 1;
		}
		return 0;

	// Floor click left of x 119: the judge stops the hero while flag 0x350 is set
	case kEvFloorClick:
		if ((param & 0xFFFF) < 119 && game()->flagTest(0x350)) {
			game()->enqueue({
				seqCall(kPrimWalk, { xy(116, 76), 34 }),
				seqCall(kPrimSay, { &_actJudge, 851, 0xe, 0xe, 2 }),
				seqCall(kPrimWalk, { xy(130, 76), 0 }),
				seqCall(kPrimDialogue, { 0, 852, 852, 0, 0, 0 }),
				seqWaitChildren(),
				seqEnd()
			});
			return -1;
		}
		return 0;

	// Held item over a hotspot: item 0x16 (pouch) on the judge while the sister is here
	case kEvItemOnHotspot:
		if (param == 0x160333)
			return (!game()->flagTest(0x376)) * game()->flagTest(0x348) * 0x346;
		return 0;

	// Action executed: show the pouch - the sister is exposed and Albert takes her away
	case kEvAction:
		if (param == 0x346 && game()->flagTest(0x348)) {
			game()->enqueue({
				seqCall(kPrimWalk, { xy(140, 76), 34 }),
				seqCall(kPrimDialogue, { 0, 840, 840, 0, 0, 0 }),
				seqCall(kPrimSay, { &_actBaroda, 841, 9, 9, 2 }),
				seqCall(kPrimDialogue, { 0, 842, 842, 0, 0, 0 }),
				seqCall(kPrimFace, { 0x22 }),
				seqCall(kPrimSetFrame, { &hero(), 0x87 }),
				seqCall(kPrimAnimate, { &hero(), 0x89, 4 }),
				seqCall(kPrimWait, { 30 }),
				seqCall(kPrimAnimate, { &hero(), 0x87, 4 }),
				seqCall(kPrimSetFrame, { &hero(), 0x22 }),
				seqCall(kPrimDialogue, { 0, 843, 843, 0, 0, 0 }),
				seqCall(kPrimSay, { &_actBaroda, 844, 9, 9, 2 }),
				seqCall(kPrimSay, { &_actJudge, 845, 0xe, 0xe, 2 }),
				seqCall(kPrimSay, { &_actBaroda, 846, 9, 9, 2 }),
				seqCall(kPrimSay, { &_actJudge, 847, 0xe, 0xe, 2 }),
				seqSpawn(),
				seqCall(kPrimWalk, { xy(152, 70), 0 }),
				seqJoin(),
				seqSpawn(),
				seqCall(kPrimSetFrame, { &_actBaroda, 0x1a }),
				seqCall(kPrimShowAt, { &_actBaroda, xy(245, 135), 0x18f0000 }),
				seqCall(kPrimMoveAxis, { &_actBaroda, 1, 0x280, 0x14, -1, -1, 5 }),
				seqJoin(),
				seqCall(kPrimShowAt, { &_actGuard, xy(165, 120), 0x1900000 }),
				seqCall(kPrimSetFrame, { &_actGuard, 0x13 }),
				seqCall(kPrimMoveAxis, { &_actGuard, 1, 0x280, 0x14, 0x12, 0x19, 5 }),
				seqKillChildren(),
				seqCall(kPrimDialogue, { &_actJudge, 848, 850, 0xe, 0xe, 2 }),
				seqEnd()
			});
		}
		// The original falls through into the tick case
		animateFire();
		return 0;

	// Hotspot lookup: spot 10 stands for item 8 while it is there
	case kEvHotspotOverride:
		if (param == 10 && game()->flagTest(8))
			return 8;
		return 0;

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom0817() {
	return new Room0817();
}

} // End of namespace Gooka
