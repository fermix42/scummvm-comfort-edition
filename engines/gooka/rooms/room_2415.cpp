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
 * Room2415: port of the room handler FUN_004258c0.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 2415 (original handler FUN_004258c0): the grate (0x970) in front of
 * the way to room 2424. The file (item 0x37) cuts it (action 0x971 "saw off the
 * grate"); while it stands, clicks on the exit 0x978 go to the grate.
 */
class Room2415 : public Room {
public:
	Room2415();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;

private:
	Sprite _s47b388; ///< object at 0x47b388 (FUN_0040ff60()): animated, frames 5..12
	Sprite _s47b720; ///< object at 0x47b720 (FUN_0040ff60()): the grate
};

/** Constructs the room statics. */
Room2415::Room2415() : Room(2415) {
}

/** Static descriptor 0x4433e8. */
const RoomDescriptor *Room2415::getDescriptor() {
	return findRoomData(_id);
}

/** Room events (FUN_004258c0, `what` >= 2). */
int Room2415::handleEvent(int event, int param) {
	switch (event) {
	// Enter: animation and the grate (hidden once sawn off: flag 0x971 clear)
	case kEvEnter:
		game()->placeObject(&_s47b388, 400, 2, 0, 5, 0, -1);
		game()->placeObjectIfFlag(0x970, &_s47b720, 0x194, 0x117, 2, 0xd, 0);
		if (!game()->flagTest(0x971)) {
			_s47b720.hide();
			return 0;
		}
		break;

	// Tick: frames 5..12, one step every 4 ticks
	case kEvTick:
		_s47b388.setFrame((int)ticks() / 4 % 8 + 5);
		return 0;

	// Held item over a hotspot: the file 0x37 on the grate 0x970 = action 0x971 (while flag 0x978 is set)
	case kEvItemOnHotspot:
		if (param == 0x370970 && game()->flagTest(0x978))
			return 0x971;
		break;

	// Action 0x971: saw off the grate
	case kEvAction:
		if (param == 0x971) {
			game()->enqueue({
				seqCall(kPrimWalk, { xy(104, 85), 144 }),
				seqCall(kPrimSoundWait, { 3873 }),
				seqCall(kPrimClearFlag, { 2417 }),
				seqCall(kPrimHide, { &_s47b720 }),
				seqCall(kPrimSound, { 3874 }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
			return 0;
		}
		break;

	// Hotspot lookup: the exit 0x978 counts as the grate 0x970 while the grate stands
	case kEvHotspotOverride:
		if (param == 0x978 && game()->flagTest(0x971))
			return 0x970;
		break;

	default:
		break;
	}
	return 0;
}

/** Factory used by the room registry. */
Room *createRoom2415() {
	return new Room2415();
}

} // End of namespace Gooka
