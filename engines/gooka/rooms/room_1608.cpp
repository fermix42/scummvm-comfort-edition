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
 * Room1608: port of the room handler FUN_00418fc0.
 * Six dials (hotspots and variables 0x64a..0x64f, position frames 8..15)
 * and a closed door of two halves (flag 0x650, hotspot 0x6a9). Every click
 * turns a dial one step; when the dials show 15, 9, 10, 8, 12, 11 the door
 * slides open. A shimmer effect and an ambient loop run in the background.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** Room 1608 (original handler FUN_00418fc0): the dial lock. */
class Room1608 : public Room {
public:
	Room1608();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;

private:
	/** Enqueues the turn of dial `index` (0..5) by one position (hotspots 0x64a..0x64f). */
	void turnDial(int index);

	Sprite _s4767b0; ///< object at 0x4767b0 (FUN_0040ff60()): left door half
	Sprite _s476288; ///< object at 0x476288 (FUN_0040ff60()): right door half
	Sprite _a4764c8[6]; ///< array at 0x4764c8 (FUN_0040ff60): the dials
	int _h4768a4 = kNoSound; ///< object at 0x4768a4 (FUN_0040a6f0()): ambient loop 0xd48
	ShimmerSprite _e476090; ///< object at 0x476090 (FUN_00412690(0x10, 0xffffffff))
};

/** Creates the room objects; the shimmer gets amplitude 16 and direction -1. */
Room1608::Room1608() : Room(1608), _e476090(16, -1) {
}

/** Static descriptor DAT_004406a0. */
const RoomDescriptor *Room1608::getDescriptor() {
	return findRoomData(_id);
}

/** Room events: dial state in the variables, dial clicks, the door opening. */
int Room1608::handleEvent(int event, int param) {
	switch (event) {
	// New game: random dial positions, ambient loop off
	case kEvNewGame:
		for (int i = 0; i < 6; i++)
			game()->var(0x64a + i) = game()->rnd(8);
		game()->stopSound(&_h4768a4);
		return 0;
	// Room entered: door halves, dials from their variables, ambient loop, shimmer
	case kEvEnter: {
		game()->placeObjectIfFlag(0x650, &_s4767b0, 0xb9, 0x2d, 0, 0, 0);
		game()->placeObjectIfFlag(0x650, &_s476288, 0x136, 0x2d, 0, 1, 0);
		int x = 0x1f9;
		for (int id = 0x64a; id < 0x650; id++) {
			game()->placeObjectIfFlag(id, &_a4764c8[id - 0x64a], x, 0xe6, 1, (game()->var(id) & 7) + 8, 0);
			x += 0x14;
		}
		game()->playSound(&_h4768a4, 0xd48, 0, true, -6000, 0);
		// The shimmer is set up field by field, not with FUN_0040ec00: no new
		// offset table and the direction keeps its constructor value -1
		_e476090.x = 0xc4;
		_e476090.y = 0xb6;
		_e476090.priority = -1;
		_e476090.subPriority = 0;
		_e476090.width = 0xe0;
		_e476090.height = 0x3a;
		_e476090.sheet = game()->roomSheet();
		_e476090.sheetFrame = 2;
		_e476090.phase = 0;
		game()->registerHotspot(&_e476090, -1);
		return 0;
	}
	// Room left: store the dial positions, ambient loop off
	case kEvLeave:
		for (int i = 0; i < 6; i++)
			game()->var(0x64a + i) = _a4764c8[i].frame;
		game()->stopSound(&_h4768a4);
		return 0;
	// Tick: shimmer step every 3rd tick
	case kEvTick:
		if (ticks() % 3 == 0)
			_e476090.step();
		return 0;
	// Hotspot clicked
	case kEvHotspot:
		// Hotspots 0x64a..0x64f: turn the dial, the sequence then raises event 0x17
		if (param > 0x649 && param < 0x650) {
			turnDial(param - 0x64a);
			return 0;
		}
		// Hotspot 0x6a9: the closed door - the hero comments
		if (param == 0x6a9 && game()->flagTest(0x650)) {
			game()->enqueue({
				seqCall(kPrimSay, { &hero(), 0x649, 0, 104, 4 }),
				seqEnd()
			});
			return 1;
		}
		return 0;
	// Raised after a dial turn: open the door once the dials show the code
	case kEvRoomPrivate1:
		if (_a4764c8[0].frame == 0xf && _a4764c8[1].frame == 9 && _a4764c8[2].frame == 10 &&
				_a4764c8[3].frame == 8 && _a4764c8[4].frame == 0xc && _a4764c8[5].frame == 0xb &&
				game()->flagTestAndClear(0x650)) {
			game()->enqueue({
				seqSpawn(),
				seqCall(kPrimMoveAxis, { &_s4767b0, 1, 50, -1, 0, 0, 1 }),
				seqJoin(),
				seqCall(kPrimAmbient, { 0xef9 }),
				seqCall(kPrimMoveAxis, { &_s476288, 1, 445, 1, 1, 1, 1 }),
				seqCall(kPrimSound, { 0xefa }),
				seqCall(kPrimAmbientStop),
				seqEnd()
			});
		}
		return 0;
	default:
		return 0;
	}
}

/** Walk to the dials, play the turn animation (frames 4..7) and show the next position. */
void Room1608::turnDial(int index) {
	Sprite *dial = &_a4764c8[index];
	// Position frames cycle 8..15; computed when the click happens, like the original
	int next = ((dial->frame + 1) & 7) + 8;
	game()->enqueue({
		seqCall(kPrimWalk, { xy(116, 87), 36 }),
		seqCall(kPrimSetFrame, { dial, 4 }),
		seqCall(kPrimSound, { 0xef8 }),
		seqCall(kPrimAnimate, { dial, 7, 4 }),
		seqCall(kPrimSetFrame, { dial, next }),
		seqWaitChildren(),
		seqCall(kPrimRaiseEvent, { 0x17, 0 }),
		seqEnd()
	});
}

/** Factory used by the room registry. */
Room *createRoom1608() {
	return new Room1608();
}

} // End of namespace Gooka
