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
 * Room 923: the storeroom with the coal heap (port of the room handler
 * FUN_004278c0).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 923 (original handler FUN_004278c0): scooping coal (0x39c) with the
 * coal scuttle uncovers the wire (item 0x2a), which starts hidden.
 */
class Room0923 : public Room {
public:
	/** Room id 923 (the room has no objects of its own). */
	Room0923();
	/** Static descriptor 0x444168. */
	const RoomDescriptor *getDescriptor() override;
	/** New game, scooping coal and the hotspot override of the hidden wire. */
	int handleEvent(int event, int param) override;
};

Room0923::Room0923() : Room(923) {
}

const RoomDescriptor *Room0923::getDescriptor() {
	return findRoomData(_id);
}

int Room0923::handleEvent(int event, int param) {
	switch (event) {
	// New game: the wire is buried under the coal
	case kEvNewGame:
		game()->flagClear(0x2a);
		return 0;

	// Held item over a hotspot: item 0x17 (coal scuttle) on the coal heap scoops coal
	case kEvItemOnHotspot:
		if (param == 0x17039c)
			return 0x39d;
		return 0;

	// Action executed: scoop coal; the first time the wire turns up
	case kEvAction:
		if (param == 0x39d) {
			if (!game()->hasItem(0x2a))
				game()->flagSet(0x2a, true);
			game()->enqueue({
				seqCall(kPrimWalk, { xy(112, 82), 38 }),
				seqCall(kPrimSetFrame, { &hero(), 0x93 }),
				seqCall(kPrimSoundWait, { 925 }),
				seqIf(kPrimIfNotItem, { 0x2a }),
				seqCall(kPrimShowExit, { 0x2a }),
				seqCall(kPrimSetFlag, { 0x2a }),
				seqEndIf(),
				seqCall(kPrimSetFrame, { &hero(), 0x26 }),
				seqCall(kPrimDropHeld),
				seqCall(kPrimReplaceItem, { 0x17, 0x19 }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
		}
		return 0;

	// Hotspot lookup: the wire's spot is part of the coal heap while the wire is hidden
	case kEvHotspotOverride:
		if (param == 0x2a && !game()->flagTest(0x2a))
			return 0x39c;
		return 0;

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom0923() {
	return new Room0923();
}

} // End of namespace Gooka
