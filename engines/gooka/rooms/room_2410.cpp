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
 * Room2410: port of the room handler FUN_00424ce0.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** Where the hero walks to try each locked door 0x1b4..0x1ba: grid x, y, stand frame. */
static const int kRoom2410Doors[7][3] = {
	{ 143, 95, 99 }, { 122, 82, 99 }, { 110, 75, 99 }, { 44, 76, 91 },
	{ 33, 82, 91 }, { 63, 63, 166 }, { 95, 63, 166 }
};

/**
 * Room 2410 (original handler FUN_00424ce0): the hall with the teleport to
 * room 3246, the arrival from room 2610, seven locked doors and the door to
 * Gurham (2419), which opens with the controller (item 0x47, action 0x96c
 * "use the controller"). A particle effect runs all the time.
 */
class Room2410 : public Room {
public:
	Room2410();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;

private:
	ParticleSprite _p47bb88; ///< object at 0x47bb88 (FUN_00412910(100)): particle effect
	Sprite _s47b460;         ///< object at 0x47b460 (inline Sprite ctor): teleport part, rests at y 0xbf, up at 0x5b while teleporting
	Sprite _s47b768;         ///< object at 0x47b768 (inline Sprite ctor): comes down from the top on arrival from 2610, rests at y 0x9e
};

/** Constructs the room statics. */
Room2410::Room2410() : Room(2410), _p47bb88(100) {
}

/** Static descriptor 0x4430e8. */
const RoomDescriptor *Room2410::getDescriptor() {
	return findRoomData(_id);
}

/** Room events (FUN_00424ce0, `what` >= 2). */
int Room2410::handleEvent(int event, int param) {
	switch (event) {
	// Enter: particles and objects; arrivals by teleport (from 3246) and from 2610 are animated
	case kEvEnter:
		game()->setRectHotspot(&_p47bb88, 0x12d, 0xbf, 0, 0, -1);
		_p47bb88.width = 0x32;
		_p47bb88.height = 100;
		if (param == 0xcae) {
			// From 3246: the hero materialises, then the teleport part comes down
			game()->placeObject(&_s47b460, 0x12d, 0x5b, 1, 4, 999, -1);
			hero().place(0x51, 0x49, 0x9c);
			hero().hide();
			game()->enqueue({
				seqCall(kPrimWait, { 1 }),
				seqCall(kPrimSound, { 2096 }),
				seqCall(kPrimAnimate, { &hero(), 149, 2 }),
				seqCall(kPrimSetFrame, { &hero(), 39 }),
				seqSpawn(),
				seqCall(kPrimFace, { 0 }),
				seqJoin(),
				seqCall(kPrimAmbient, { 3833 }),
				seqCall(kPrimMoveLinear, { &_s47b460, 50, 0, 1, 2, 1 }),
				seqCall(kPrimSound, { 3849 }),
				seqCall(kPrimAmbientStop),
				seqEnd()
			});
		} else {
			game()->placeObject(&_s47b460, 0x12d, 0xbf, 1, 4, 999, -1);
		}
		if (param != 0xa32) {
			game()->placeObject(&_s47b768, 0x171, 0x9e, 0, 5, 0, -1);
			return 0;
		}
		// From 2610: the hero steps forward while the object comes down from the top
		game()->placeObject(&_s47b768, 0x171, 0, 0, 5, 0, -1);
		hero().place(0x60, 0x3c, 0);
		hero().show();
		game()->enqueue({
			seqSpawn(),
			seqCall(kPrimWalk, { xy(96, 63), 0 }),
			seqJoin(),
			seqCall(kPrimAmbient, { 3833 }),
			seqCall(kPrimMoveLinear, { &_s47b768, 79, 0, 1, 2, 1 }),
			seqCall(kPrimSound, { 3849 }),
			seqCall(kPrimAmbientStop),
			seqWaitChildren(),
			seqEnd()
		});
		return 0;

	// Tick: particles move every other tick
	case kEvTick:
		if ((ticks() & 1) == 0) {
			_p47bb88.step();
			return 0;
		}
		break;

	// Hotspot clicked
	case kEvHotspot:
		if (param == 0x973) {
			// Hotspot 0x973: door to 2419 (Gurham) - autosave, then the exit logic walks there.
			// FUN_0041cd20("GAME.SAVE"): the ScummVM autosave slot stands in for GAME.SAVE, as
			// Ui does for FUN_0042c000 (unlike the original it follows the autosave setting).
			g_engine->saveAutosaveIfEnabled();
			return 0;
		}
		if (param >= 0x1b4 && param <= 0x1ba) {
			// Hotspots 0x1b4..0x1ba: locked doors - try the door, "it won't open" (0x96b)
			const int *door = kRoom2410Doors[param - 0x1b4];
			game()->enqueue({
				seqCall(kPrimWalk, { xy(door[0], door[1]), door[2] }),
				seqCall(kPrimSoundWait, { 803 }),
				seqCall(kPrimFace, { 0 }),
				seqCall(kPrimDialogue, { 0, 2411, 2411, 0, 0, 0 }),
				seqEnd()
			});
			return 0;
		}
		if (param == 0xcae) {
			// Hotspot 0xcae: teleport - the teleport part rises, the hero dematerialises to room 3246
			game()->enqueue({
				seqCall(kPrimWalk, { xy(81, 73), 166 }),
				seqSpawn(),
				seqCall(kPrimWait, { 20 }),
				seqCall(kPrimFace, { 39 }),
				seqJoin(),
				seqCall(kPrimAmbient, { 3833 }),
				seqCall(kPrimMoveLinear, { &_s47b460, 100, 0, 1, -1, 1 }),
				seqCall(kPrimSound, { 3849 }),
				seqCall(kPrimAmbientStop),
				seqCall(kPrimWait, { 30 }),
				seqCall(kPrimSound, { 2097 }),
				seqCall(kPrimSetFrame, { &hero(), 149 }),
				seqCall(kPrimWait, { 2 }),
				seqCall(kPrimAnimate, { &hero(), 156, 2 }),
				seqCall(kPrimHide, { &hero() }),
				seqCall(kPrimWait, { 1 }),
				seqCall(kPrimChangeRoom, { 0xcae }),
				seqEnd()
			});
		}
		break;

	// Held item over a hotspot: controller 0x47 on the door 0x973 = action 0x96c (while its flag is set)
	case kEvItemOnHotspot:
		if (param == 0x470973)
			return game()->flagTestId(0x96c);
		break;

	// Action 0x96c: use the controller on the door and enter Gurham's room
	case kEvAction:
		if (param == 0x96c) {
			game()->enqueue({
				seqCall(kPrimDialogue, { 0, 2413, 2413, 0, 0, 0 }),
				seqCall(kPrimWalk, { xy(39, 59), 34 }),
				seqCall(kPrimClearFlag, { 2412 }),
				seqCall(kPrimSoundWait, { 3869 }),
				seqCall(kPrimChangeRoom, { 2419 }),
				seqEnd()
			});
			return 0;
		}
		break;

	default:
		break;
	}
	return 0;
}

/** Factory used by the room registry. */
Room *createRoom2410() {
	return new Room2410();
}

} // End of namespace Gooka
