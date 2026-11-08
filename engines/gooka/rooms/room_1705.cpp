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
 * Room 1705: the secret entrance of the Orint monastery: the password
 * "Danim-Glux!" opens a trapdoor and Gooka falls into the hall below (1708)
 * (port of the room handler FUN_0041e210).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 1705 (original handler FUN_0041e210): shimmer and ambient sound, the
 * trapdoor opened by the password at hotspot 0x6ab once line 0x85e has
 * been said (its flag is clear); the game is autosaved before the fall.
 */
class Room1705 : public Room {
public:
	/** Constructs the room objects. */
	Room1705();
	/** Static descriptor 0x441b40. */
	const RoomDescriptor *getDescriptor() override;
	/** Enter/leave/tick and the password at hotspot 0x6ab. */
	int handleEvent(int event, int param) override;

private:
	ShimmerSprite _e479648;  ///< 0x479648: shimmer (FUN_00412690(0x10, -1))
	int _h47987c = kNoSound; ///< 0x47987c: ambient sound loop (WAV 0xd48)
	Sprite _s4799e0;         ///< 0x4799e0: object animated 2..8..2 by the password (inline Sprite ctor)
	Sprite _s479408;         ///< 0x479408: the trapdoor, hidden before the fall (inline Sprite ctor)
};

Room1705::Room1705() : Room(1705), _e479648(16, -1) {
}

const RoomDescriptor *Room1705::getDescriptor() {
	return findRoomData(_id);
}

int Room1705::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: back from the hall below (0x6ac) the hero stands at the trapdoor; shimmer, objects, sound
	case kEvEnter:
		if (param == 0x6ac)
			hero().place(0x4d, 0x61, 0);
		game()->placeShimmer(&_e479648, 0xa4, 0xcd, -1, 0x10e, 0x3c, 1, 0x10, -1);
		game()->placeObject(&_s4799e0, 0xf8, 0, 0, 2, 0, -1);
		game()->placeObject(&_s479408, 0xe6, 0x176, 0, 0, 0, -1);
		game()->playSound(&_h47987c, 0xd48, 0, true, -4000, 0);
		return 0;

	// Room left: stop the ambient sound
	case kEvLeave:
		game()->stopSound(&_h47987c);
		return 0;

	// Every 3rd tick: shimmer
	case kEvTick:
		if ((int)ticks() % 3 == 0)
			_e479648.step();
		return 0;

	// Hotspot clicked
	case kEvHotspot:
		// Hotspot 0x6ab: say the password (once flag 0x85e is clear), the trapdoor opens and Gooka falls to 1708
		if (param == 0x6ab && !game()->flagTest(0x85e)) {
			// FUN_0041cd20("GAME.SAVE"): checkpoint before the trapdoor, as ScummVM's autosave
			g_engine->saveAutosaveIfEnabled();
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x4d, 0x61), 0x27 }),
				seqCall(kPrimDialogue, { 0, 0x6aa, 0x6aa, 0, 0, 0 }),
				seqCall(kPrimSound, { 0xefe }),
				seqCall(kPrimAnimate, { &_s4799e0, 8, 4 }),
				seqCall(kPrimAnimate, { &_s4799e0, 2, 4 }),
				seqSpawn(),
				seqCall(kPrimSound, { 0xeff }),
				seqCall(kPrimWait, { 0x28 }),
				seqJoin(),
				seqCall(kPrimHide, { &_s479408 }),
				seqCall(kPrimMoveAccel, { &hero(), 0x190, 0, 3, 0x27, 0x27, 1 }),
				seqCall(kPrimChangeRoom, { 0x6ac }),
				seqEnd()
			});
			return 1;
		}
		return 0;

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom1705() {
	return new Room1705();
}

} // End of namespace Gooka
