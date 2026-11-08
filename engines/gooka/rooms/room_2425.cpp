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
 * Room2425: port of the room handler FUN_00425c30.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 2425 (original handler FUN_00425c30): the wall (0x97a) in front of
 * the way to room 2431. The dagger (item 0x11) loosens its joints (action 0x97b
 * "loosen the joints") and the hero takes it apart stone by stone; while
 * it stands, clicks on the exit 0x97f go to the wall.
 */
class Room2425 : public Room {
public:
	Room2425();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;

private:
	Sprite _s47b500; ///< object at 0x47b500 (FUN_0040ff60()): the wall, frames 0..9 while it is taken apart
	Sprite _s47b5e0; ///< object at 0x47b5e0 (FUN_0040ff60()): the pile of stones, frames 10..19
};

/** Constructs the room statics. */
Room2425::Room2425() : Room(2425) {
}

/** Static descriptor 0x4436a8. */
const RoomDescriptor *Room2425::getDescriptor() {
	return findRoomData(_id);
}

/** Room events (FUN_00425c30, `what` >= 2). */
int Room2425::handleEvent(int event, int param) {
	switch (event) {
	// Enter: the wall (hidden once taken apart: flag 0x97b clear), the pile hidden
	case kEvEnter:
		game()->placeObjectIfFlag(0x97a, &_s47b500, 0, 0x6a, 0, 0, 0);
		if (!game()->flagTest(0x97b))
			_s47b500.hide();
		game()->placeObjectHidden(&_s47b5e0, 0x96, 200, 0, 0, 0);
		return 0;

	// Held item over a hotspot: the dagger 0x11 on the wall 0x97a = action 0x97b
	case kEvItemOnHotspot:
		if (param == 0x11097a)
			return 0x97b;
		break;

	// Action 0x97b: loosen the joints and take the wall apart
	case kEvAction:
		if (param == 0x97b) {
			game()->enqueue({
				seqCall(kPrimDialogue, { 0, 2428, 2428, 0, 0, 0 }),
				seqCall(kPrimWalk, { xy(18, 87), 96 }),
				seqCall(kPrimSetFrame, { &hero(), 134 }),
				seqCall(kPrimSound, { 3879 }),
				seqCall(kPrimWait, { 16 }),
				seqCall(kPrimAnimate, { &hero(), 132, 4 }),
				seqCall(kPrimAnimate, { &hero(), 134, 4 }),
				seqCall(kPrimSetFrame, { &hero(), 96 }),
				seqCall(kPrimFace, { 0 }),
				seqCall(kPrimDialogue, { 0, 2429, 2429, 0, 0, 0 }),
				seqCall(kPrimWalk, { xy(18, 87), 96 }),
				seqCall(kPrimSound, { 3880 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimSetFrame, { &_s47b500, 1 }),
				seqCall(kPrimWalk, { xy(41, 80), 144 }),
				seqCall(kPrimShow, { &_s47b5e0 }),
				seqCall(kPrimSound, { 3881 }),
				seqCall(kPrimSetFrame, { &_s47b5e0, 10 }),
				seqCall(kPrimWalk, { xy(22, 87), 95 }),
				seqCall(kPrimSound, { 3880 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimSetFrame, { &_s47b500, 2 }),
				seqCall(kPrimWalk, { xy(41, 80), 144 }),
				seqCall(kPrimSound, { 3881 }),
				seqCall(kPrimSetFrame, { &_s47b5e0, 11 }),
				seqCall(kPrimWalk, { xy(29, 87), 92 }),
				seqCall(kPrimSound, { 3880 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimSetFrame, { &_s47b500, 3 }),
				seqCall(kPrimWalk, { xy(41, 80), 144 }),
				seqCall(kPrimSound, { 3881 }),
				seqCall(kPrimSetFrame, { &_s47b5e0, 12 }),
				seqCall(kPrimWalk, { xy(25, 87), 93 }),
				seqCall(kPrimSound, { 3880 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimSetFrame, { &_s47b500, 4 }),
				seqCall(kPrimWalk, { xy(41, 80), 144 }),
				seqCall(kPrimSound, { 3881 }),
				seqCall(kPrimSetFrame, { &_s47b5e0, 13 }),
				seqCall(kPrimWalk, { xy(20, 87), 95 }),
				seqCall(kPrimSound, { 3880 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimSetFrame, { &_s47b500, 5 }),
				seqCall(kPrimWalk, { xy(41, 80), 144 }),
				seqCall(kPrimSound, { 3881 }),
				seqCall(kPrimSetFrame, { &_s47b5e0, 14 }),
				seqCall(kPrimWalk, { xy(28, 87), 90 }),
				seqCall(kPrimSound, { 3880 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimSetFrame, { &_s47b500, 6 }),
				seqCall(kPrimWalk, { xy(41, 80), 144 }),
				seqCall(kPrimSound, { 3881 }),
				seqCall(kPrimSetFrame, { &_s47b5e0, 15 }),
				seqCall(kPrimWalk, { xy(22, 87), 90 }),
				seqCall(kPrimSound, { 3880 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimSetFrame, { &_s47b500, 7 }),
				seqCall(kPrimWalk, { xy(41, 80), 144 }),
				seqCall(kPrimSound, { 3881 }),
				seqCall(kPrimSetFrame, { &_s47b5e0, 16 }),
				seqCall(kPrimWalk, { xy(18, 87), 90 }),
				seqCall(kPrimSound, { 3880 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimSetFrame, { &_s47b500, 8 }),
				seqCall(kPrimWalk, { xy(41, 80), 144 }),
				seqCall(kPrimSound, { 3881 }),
				seqCall(kPrimSetFrame, { &_s47b5e0, 17 }),
				seqCall(kPrimWalk, { xy(19, 87), 40 }),
				seqCall(kPrimSound, { 3880 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimSetFrame, { &_s47b500, 9 }),
				seqCall(kPrimWalk, { xy(41, 80), 144 }),
				seqCall(kPrimSound, { 3881 }),
				seqCall(kPrimSetFrame, { &_s47b5e0, 18 }),
				seqCall(kPrimWalk, { xy(19, 87), 40 }),
				seqCall(kPrimHide, { &_s47b500 }),
				seqCall(kPrimWalk, { xy(41, 80), 144 }),
				seqCall(kPrimSound, { 3881 }),
				seqCall(kPrimSetFrame, { &_s47b5e0, 19 }),
				seqCall(kPrimFace, { 0 }),
				seqCall(kPrimClearFlag, { 2427 }),
				seqEnd()
			});
			return 1;
		}
		break;

	// Hotspot lookup: the exit 0x97f counts as the wall 0x97a while the wall stands
	case kEvHotspotOverride:
		if (param == 0x97f && game()->flagTest(0x97b))
			return 0x97a;
		break;

	default:
		break;
	}
	return 0;
}

/** Factory used by the room registry. */
Room *createRoom2425() {
	return new Room2425();
}

} // End of namespace Gooka
