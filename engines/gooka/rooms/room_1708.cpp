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
 * Room 1708: the hidden temple of Gurha below the monastery. Gooka falls
 * into the service, the high priest has him taken to the torture chamber
 * (1719); later the ladder leads back up to 1705 (port of the room handler
 * FUN_0041e5a0).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 1708 (original handler FUN_0041e5a0): a wave shimmer, two animated
 * objects, the fall from 1705 with the high priest's speech, and the ladder
 * (item 6, action 0x6b5) that makes the way up (0x6a9) usable.
 */
class Room1708 : public Room {
public:
	/** Constructs the room objects. */
	Room1708();
	/** Static descriptor 0x441cd8. */
	const RoomDescriptor *getDescriptor() override;
	/** Enter/leave/tick, the way up and the ladder. */
	int handleEvent(int event, int param) override;

private:
	ShimmerSprite _e479210;  ///< 0x479210: wave shimmer (FUN_00417080())
	Sprite _s479a28;         ///< 0x479a28: animated object, frames 1..8
	Sprite _s479600;         ///< 0x479600: animated object, frames 9..16
	Sprite _s479bb0;         ///< 0x479bb0: the ladder, shown once it is put down
	Actor _n479458;          ///< 0x479458: worshipper at the service
	Actor _n479880;          ///< 0x479880: worshipper at the service
	Actor _n479ab8;          ///< 0x479ab8: worshipper at the service
	Actor _n479930;          ///< 0x479930: the high priest
	int _h47944c = kNoSound; ///< 0x47944c: ambient sound loop
};

Room1708::Room1708() : Room(1708) {
}

const RoomDescriptor *Room1708::getDescriptor() {
	return findRoomData(_id);
}

int Room1708::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: sound, animations, ladder, shimmer; falling in from 1705 (0x6a9) during the
	// service (flag 0x6ad) the high priest condemns Gooka and he is taken to 1719
	case kEvEnter:
		game()->playSound(&_h47944c, 0xef2, 0, true, 0, 0);
		g_engine->_soundMan->setPan(&_h47944c, -1000);
		game()->placeObject(&_s479a28, 0x82, 0x104, 0, 1, 0, -1);
		game()->placeObject(&_s479600, 0x1bb, 0x104, 0, 8, 0, -1);
		game()->placeObjectIfNotFlag(0x6b5, &_s479bb0, 0x10e, 0x88, 0, 0x19, 0);
		game()->placeShimmer(&_e479210, 0xc2, 0x5f, -1, 0xfa, 200, 0, 6, -10);
		_e479210.regenerate();
		if (param == 0x6a9) {
			if (game()->flagTest(0x6ad)) {
				game()->placeObject(&_n479458, 0xb4, 0x96, 1, 0x11, 0, -1);
				game()->placeObject(&_n479880, 0x154, 0x82, 1, 0x12, 0, -1);
				game()->placeObject(&_n479ab8, 0x78, 0xfa, 2, 0x13, 0, -1);
				game()->placeObject(&_n479930, 0x1a4, 0xaa, 2, 0x15, 0, -1);
			}
			hero().place(0x4e, 0x55, 0x27);
			game()->enqueue({
				seqCall(kPrimShowAt, { &hero(), xy(0xfe, 0), 0 }),
				seqCall(kPrimMoveAccel, { &hero(), 100, 0, 3, 0x27, 0x27, 1 }),
				seqCall(kPrimSound, { 0xf15 }),
				seqCall(kPrimPlaceActor, { &hero(), xy(0x4e, 0x55), 0x770000 }),
				seqCall(kPrimFace, { 0 }),
				seqIf(kPrimIfFlag, { 0x6ad }),
				seqCall(kPrimAnimate, { &_n479930, 0x17, 4 }),
				seqCall(kPrimSay, { &_n479930, 0x6ad, 0x17, 0x17, 2 }),
				seqCall(kPrimAnimate, { &_n479930, 0x15, 4 }),
				seqCall(kPrimSay, { &_n479930, 0x6ae, 0x15, 0x14, 2 }),
				seqCall(kPrimSay, { &_n479930, 0x6af, 0x15, 0x14, 2 }),
				seqCall(kPrimDialogue, { 0, 0x6b0, 0x6b0, 0, 0, 0 }),
				seqCall(kPrimSay, { &_n479930, 0x6b1, 0x15, 0x14, 2 }),
				seqCall(kPrimSay, { &_n479930, 0x6b2, 0x15, 0x14, 2 }),
				seqCall(kPrimSay, { &_n479930, 0x6b3, 0x15, 0x14, 2 }),
				seqCall(kPrimAnimate, { &_n479930, 0x17, 4 }),
				seqCall(kPrimSay, { &_n479930, 0x6b4, 0x17, 0x17, 2 }),
				seqCall(kPrimAnimate, { &_n479930, 0x15, 4 }),
				seqCall(kPrimChangeRoom, { 0x6b7 }),
				seqEndIf(),
				seqEnd()
			});
		}
		return 0;

	// Room left: stop the ambient sound
	case kEvLeave:
		game()->stopSound(&_h47944c);
		return 0;

	// Every tick: the wave scrolls every 2nd tick, the two animations advance a frame per 4 ticks
	case kEvTick:
		if ((ticks() & 1) == 0) {
			_e479210.scroll();
			_e479210.phase = (int)ticks() >> 2;
		}
		_s479a28.setFrame(((int)ticks() / 4 & 7) + 1);
		_s479600.setFrame(((int)ticks() / 4 & 7) + 9);
		return 0;

	// Hotspot 0x6a9 (the way up to 1705): too high until the ladder stands there
	case kEvHotspot:
		if (param == 0x6a9 && game()->flagTest(0x6b5)) {
			game()->sayIdle(0x7d1);
			return -1;
		}
		return 0;

	// Held item over a hotspot: the ladder (item 6) on 0x6b6 = put it down
	case kEvItemOnHotspot:
		if (param == 0x606b6)
			return 0x6b5;
		return 0;

	// Action 0x6b5: put the ladder down (it is used up)
	case kEvAction:
		if (param == 0x6b5) {
			game()->flagClear(0x6b5);
			game()->consumeHeldItem();
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x4d, 0x4a), 0xa6 }),
				seqCall(kPrimShow, { &_s479bb0 }),
				seqCall(kPrimSoundWait, { 0xf03 }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
		}
		return 0;

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom1708() {
	return new Room1708();
}

} // End of namespace Gooka
