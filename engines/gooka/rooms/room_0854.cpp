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
 * Room 854: Gooka's bedroom, the first room of a new game (port of the
 * room handler FUN_004289c0).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 854 (original handler FUN_004289c0). Everything is static data
 * except the wake-up scene: while flag 0x335 (the morning of the murder)
 * is still set, entering the room plays the servant's call, the hero
 * answers and walks out into the hall (room 800).
 */
class Room0854 : public Room {
public:
	/** Room id 854 (the room has no objects of its own). */
	Room0854();
	/** Static descriptor 0x444740. */
	const RoomDescriptor *getDescriptor() override;
	/** Only the enter event is handled; everything else goes to the fallback. */
	int handleEvent(int event, int param) override;
};

Room0854::Room0854() : Room(854) {
}

const RoomDescriptor *Room0854::getDescriptor() {
	return findRoomData(_id);
}

int Room0854::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: wake-up call while the morning of the murder is still pending
	case kEvEnter:
		if (game()->flagTest(0x335)) {
			// The yield lets the new-game code place the hero before the scene starts
			game()->enqueue({
				seqYield(),
				seqCall(kPrimSoundWait, { 3585, 3585 }),        // Off-screen wake-up call
				seqCall(kPrimDialogue, { 0, 3586, 3586, 0, 0, 0 }), // "Yeah, sure, coming."
				seqCall(kPrimActOnHotspot, { 800 }),             // leave to the hall
				seqEnd()
			});
		}
		return 0;

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom0854() {
	return new Room0854();
}

} // End of namespace Gooka
