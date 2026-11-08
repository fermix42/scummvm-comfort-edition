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
 * Room 1200: the harbour of Avaral. The ship takes Gooka away to Glux
 * (room 3243) and Glux brings him and Lidra back (port of the room
 * handler FUN_004142a0).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 1200 (original handler FUN_004142a0): sea shimmer and ambient sound, the
 * departing ship when coming from the throne room (1239), the return with
 * Glux and Lidra when coming back from room 3243, the credits on event 0x17.
 */
class Room1200 : public Room {
public:
	/** Constructs the room objects. */
	Room1200();
	/** Static descriptor 0x43ee40. */
	const RoomDescriptor *getDescriptor() override;
	/** Enter/leave/tick and the credits request 0x17. */
	int handleEvent(int event, int param) override;

private:
	int _hAmbientSnd = kNoSound; ///< 0x4753b0: ambient sound loop (WAV 0xd48)
	ShimmerSprite _sprSea;  ///< 0x474d30: sea (FUN_00417080())
	Sprite _sprShip;         ///< 0x475260: the departing ship (4-frame animation)
	Actor _actGlux;          ///< 0x4753b8: Glux arriving from the sea

	/**
	 * Stand-in for the global actor DAT_00478720 (Lidra, constructed by
	 * FUN_0041bc40, sheet = image 408 set at new game by FUN_0041de10).
	 * TODO(port): the core has no shared Lidra actor; the original uses the
	 * same object in several rooms (1200, 2000, 2049, ...). Use the core's
	 * instance once it exists.
	 */
	Actor _lidra;
};

Room1200::Room1200() : Room(1200) {
}

const RoomDescriptor *Room1200::getDescriptor() {
	return findRoomData(_id);
}

int Room1200::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: ambient sound and sea; from the throne room (0x4d7) the ship sails off to 3243,
	// back from 3243 (0xcab) Glux brings Gooka home and Lidra welcomes him
	case kEvEnter:
		game()->playSound(&_hAmbientSnd, 3400, 0, true, -2000, 0);
		game()->placeShimmer(&_sprSea, 0, 280, -1, 320, 120, 4, 0x10, -1);
		if (param == 0x4d7) {
			game()->placeObject(&_sprShip, 100, 280, 0, 0, 2, -1);
			g_engine->_music->play(425, true);
			game()->enqueue({
				seqCall(kPrimHide, { &hero() }),
				seqCall(kPrimMoveLinear, { &_sprShip, 0x3b6, -1, 6, -1, 0x18 }),
				seqCall(kPrimSetFlag, { -0xcaf }),
				seqCall(kPrimShow, { &hero() }),
				seqCall(kPrimChangeRoom, { 3243 }),
				seqEnd()
			});
		}
		if (param == 0xcab && !game()->flagTest(0x846) && game()->flagTest(0x4b1)) {
			game()->placeObject(&_actGlux, 140, 350, 0, 5, 2, -1);
			hero().place(52, 98, 34);
			// The original sets Lidra's sheet once at new game (FUN_0041de10)
			_lidra.sheet = g_engine->getImage(408);
			game()->registerHotspot(&_lidra, -1);
			_lidra.x = 235;
			_lidra.y = 375;
			_lidra.priority = 3;
			_lidra.subPriority = 2;
			_lidra.setFrame(0);
			_lidra.setScale(20);
			game()->enqueue({
				seqYield(),
				seqSpawn(),
				seqCall(kPrimMoveLinear, { &_actGlux, 0x12c, -1, 2, -1, 0x18 }),
				seqJoin(),
				seqCall(kPrimDialogue, { &_actGlux, 1201, 1202, 5, 5, 1 }),
				seqCall(kPrimFace, { 0x24 }),
				seqCall(kPrimDialogue, { &_lidra, 1203, 1207, 2, 2, 2 }),
				seqCall(kPrimWalk, { xy(66, 59), -1 }),
				seqCall(kPrimChangeRoom, { 1679 }),
				seqEnd()
			});
		}
		return 0;

	// Room left: stop the ambient sound
	case kEvLeave:
		game()->stopSound(&_hAmbientSnd);
		return 0;

	// Every tick: sea shimmer every 3rd tick, ship frame 0..3
	case kEvTick:
		if ((int)ticks() % 3 == 0)
			_sprSea.step();
		_sprShip.setFrame((int)ticks() / 3 & 3);
		return 0;

	// Room-private event 0x17: end of the game, show the credits
	case kEvRoomPrivate1:
		game()->stopSound(&_hAmbientSnd);
		g_engine->setState(kStateCredits);
		return 0;

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom1200() {
	return new Room1200();
}

} // End of namespace Gooka
