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
 * Room2448: port of the room handler FUN_004266b0.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 2448 (original handler FUN_004266b0): the guardian's puzzle, a
 * close-up with twelve switchable elements (hotspots 0xc9f..0xcaa, frame
 * 0 or 1). The right combination solves it (flag 0x991 cleared) and takes
 * the hero back to room 2431.
 */
class Room2448 : public Room {
public:
	Room2448();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;

private:
	Sprite _a47b850[12]; ///< array at 0x47b850 (FUN_0040ff60): the elements
	Sprite _s47b6d8;     ///< object at 0x47b6d8 (FUN_0040ff60()): indicator, frames 4/5
};

/** Constructs the room statics. */
Room2448::Room2448() : Room(2448) {
}

/** Static descriptor 0x443890. */
const RoomDescriptor *Room2448::getDescriptor() {
	return findRoomData(_id);
}

/** Room events (FUN_004266b0, `what` >= 2). */
int Room2448::handleEvent(int event, int param) {
	switch (event) {
	// Enter: the twelve elements in a row (reset to frame 0), the indicator; no hero in the close-up
	case kEvEnter: {
		int i = 0;
		for (int x = 0x1a; x < 0x266; x += 0x31, i++)
			game()->placeObjectIfFlag(0xc9f + i, &_a47b850[i], x, 0x15c, 0, 0, 0);
		game()->placeObject(&_s47b6d8, 0x1f9, 0x14, 0, 4, 0, -1);
		hero().hide();
		return 0;
	}

	// Leave: show the hero again
	case kEvLeave:
		hero().show();
		return 0;

	// Tick: the solution (elements 0, 1, 3, 5, 6, 10 switched, the rest not) blinks the indicator and ends the puzzle
	case kEvTick:
		if (_a47b850[0].frame != 0 && _a47b850[1].frame != 0 && _a47b850[2].frame == 0 &&
				_a47b850[3].frame != 0 && _a47b850[4].frame == 0 && _a47b850[5].frame != 0 &&
				_a47b850[6].frame != 0 && _a47b850[7].frame == 0 && _a47b850[8].frame == 0 &&
				_a47b850[9].frame == 0 && _a47b850[10].frame != 0 && _a47b850[11].frame == 0 &&
				!game()->sequenceBusy()) {
			game()->enqueue({
				seqCall(kPrimSetFrame, { &_s47b6d8, 5 }),
				seqCall(kPrimSound, { 3811 }),
				seqCall(kPrimWait, { 5 }),
				seqCall(kPrimSetFrame, { &_s47b6d8, 4 }),
				seqCall(kPrimWait, { 5 }),
				seqCall(kPrimSetFrame, { &_s47b6d8, 5 }),
				seqCall(kPrimSound, { 3811 }),
				seqCall(kPrimWait, { 5 }),
				seqCall(kPrimSetFrame, { &_s47b6d8, 4 }),
				seqCall(kPrimWait, { 5 }),
				seqCall(kPrimSetFrame, { &_s47b6d8, 5 }),
				seqCall(kPrimSound, { 3885 }),
				seqCall(kPrimClearFlag, { 2449 }),
				seqCall(kPrimWait, { 10 }),
				seqCall(kPrimChangeRoom, { 2431 }),
				seqEnd()
			});
			return 0;
		}
		break;

	// Hotspots 0xc9f..0xcaa: a click toggles the element
	case kEvHotspot:
		if ((uint32)param >= 0xc9f && (uint32)param <= 0xcaa) {
			game()->playSound(nullptr, 0xf2c, 0, false, 0, 0);
			Sprite &element = _a47b850[param - 0xc9f];
			element.setFrame(element.frame == 0 ? 1 : 0);
		}
		break;

	default:
		break;
	}
	return 0;
}

/** Factory used by the room registry. */
Room *createRoom2448() {
	return new Room2448();
}

} // End of namespace Gooka
