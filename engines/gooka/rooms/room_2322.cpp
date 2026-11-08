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
 * Room2322: port of the room handler FUN_004238d0.
 * Four sockets (hotspot 0x914, flags 0x915..0x918) take the items
 * 0x3a..0x3d in order, and item 0x3f lies on its place (object/hotspot 0x919,
 * put back with hotspot 0x913). With all sockets filled and item 0x3f put
 * back (flag 0x91d clear) a ritual plays once: four flames, sounds, music
 * pause.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** Room 2322 (original handler FUN_004238d0): the four sockets. */
class Room2322 : public Room {
public:
	Room2322();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;

private:
	/** Takes item 0x3f (hotspot 0x919, also reached as action 0x919). */
	void takeItem();

	Sprite _a47aee0[4]; ///< array at 0x47aee0 (FUN_0040ff60): the socket contents (frames 0..3)
	Sprite _a47b088[4]; ///< array at 0x47b088 (FUN_0040ff60): the flames (frames 4..7, animated)
	Sprite _s47b230; ///< object at 0x47b230 (FUN_0040ff60()): item 0x3f on its place (frame 8)
};

/** Positions of socket content and flame i (DAT_00442c40: x, y, flameX, flameY). */
static const int kSocketPos[4][4] = {
	{ 212, 225, 219, 223 },
	{ 324, 214, 336, 215 },
	{ 265, 204, 274, 201 },
	{ 179, 208, 186, 206 }
};

/** Creates the room objects. */
Room2322::Room2322() : Room(2322) {
}

/** Static descriptor DAT_00442c10. */
const RoomDescriptor *Room2322::getDescriptor() {
	return findRoomData(_id);
}

/** Room events: sockets, flames, item 0x3f and the ritual. */
int Room2322::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: filled sockets, hidden flames, item 0x3f unless flag 0x91a is set
	case kEvEnter:
		for (int i = 0; i < 4; i++) {
			game()->placeObjectHidden(&_a47aee0[i], kSocketPos[i][0], kSocketPos[i][1], 1, i, 0);
			if (!game()->flagTest(i + 0x915))
				_a47aee0[i].show();
			game()->placeObjectHidden(&_a47b088[i], kSocketPos[i][2], kSocketPos[i][3], 1, 4, 1);
		}
		game()->placeObjectIfFlag(0x919, &_s47b230, 0xfc, 0xd0, 1, 8, 0);
		if (game()->flagTest(0x91a))
			_s47b230.hide();
		return 0;
	// Tick: flicker the flames; start the ritual once everything is in place
	case kEvTick:
		for (int i = 0; i < 4; i++)
			_a47b088[i].setFrame(((int)ticks() / 4 + i) % 4 + 4);
		if (!game()->flagTest(0x915) && !game()->flagTest(0x916) && !game()->flagTest(0x917) &&
				!game()->flagTest(0x918) && !game()->flagTest(0x91d) && !game()->sequenceBusy()) {
			game()->enqueue({
				seqCall(kPrimWalk, { xy(99, 76), 34 }),
				seqCall(kPrimDialogue, { 0, 0x91c, 0x91c, 0, 0, 0 }),
				seqSpawn(),
				seqCall(kPrimMusicFade, { 0, -1, 1 }),
				seqCall(kPrimMusicPause),
				seqJoin(),
				seqCall(kPrimWait, { 50 }),
				seqCall(kPrimSoundWait, { 0x91f }),
				seqCall(kPrimShow, { &_a47b088[2] }),
				seqCall(kPrimWait, { 30 }),
				seqCall(kPrimSoundWait, { 0x91f }),
				seqCall(kPrimShow, { &_a47b088[0] }),
				seqCall(kPrimWait, { 5 }),
				seqCall(kPrimSoundWait, { 0x91f }),
				seqCall(kPrimShow, { &_a47b088[1] }),
				seqCall(kPrimWait, { 20 }),
				seqCall(kPrimSoundWait, { 0x91f }),
				seqCall(kPrimShow, { &_a47b088[3] }),
				seqCall(kPrimWait, { 100 }),
				seqWaitChildren(),
				seqCall(kPrimSoundWait, { 0xed8 }),
				seqCall(kPrimSoundWait, { 0xedb }),
				seqCall(kPrimSoundWait, { 0xedc }),
				seqCall(kPrimSoundWait, { 0xedd }),
				seqCall(kPrimSoundWait, { 0xed8 }),
				seqCall(kPrimSoundWait, { 0xedb }),
				seqCall(kPrimSoundWait, { 0xedc }),
				seqCall(kPrimSoundWait, { 0xedb }),
				seqSpawn(),
				seqCall(kPrimMusicResume),
				seqCall(kPrimMusicFade, { 256, 1, 1 }),
				seqJoin(),
				seqCall(kPrimWait, { 10 }),
				seqCall(kPrimHide, { &_a47b088[0] }),
				seqCall(kPrimWait, { 40 }),
				seqCall(kPrimHide, { &_a47b088[1] }),
				seqCall(kPrimWait, { 20 }),
				seqCall(kPrimHide, { &_a47b088[2] }),
				seqCall(kPrimWait, { 10 }),
				seqCall(kPrimHide, { &_a47b088[3] }),
				seqCall(kPrimWait, { 100 }),
				seqCall(kPrimFace, { 0 }),
				seqCall(kPrimDialogue, { 0, 0x91d, 0x91d, 0, 0, 0 }),
				seqCall(kPrimSetFlag, { 0x91d }),
				seqWaitChildren(),
				seqEnd()
			});
			return 0;
		}
		return 0;
	// Hotspot clicked
	case kEvHotspot:
		// Hotspot 0x919: item 0x3f - take it
		if (param == 0x919)
			takeItem();
		return 0;
	// Held item over a hotspot
	case kEvItemOnHotspot:
		// Items 0x3a..0x3d on the sockets 0x914: action of the first empty socket
		if (param == 0x3a0914 || param == 0x3b0914 || param == 0x3c0914 || param == 0x3d0914) {
			if (game()->flagTest(0x915))
				return 0x915;
			if (game()->flagTest(0x916))
				return 0x916;
			return game()->flagTest(0x917) ? 0x917 : 0x918;
		}
		// Item 0x3f on its place 0x913: action 0x91a (put it back)
		if (param == 0x3f0913)
			return 0x91a;
		return 0;
	// Action executed
	case kEvAction:
		switch (param) {
		// Actions 0x915..0x918: fill socket 0..3
		case 0x915:
			game()->consumeHeldItem();
			game()->flagClear(0x915);
			game()->enqueue({
				seqCall(kPrimWalk, { xy(60, 83), 166 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimShow, { &_a47aee0[0] }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
			break;
		case 0x916:
			game()->consumeHeldItem();
			game()->flagClear(0x916);
			game()->enqueue({
				seqCall(kPrimWalk, { xy(95, 76), 91 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimShow, { &_a47aee0[1] }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
			break;
		case 0x917:
			game()->consumeHeldItem();
			game()->flagClear(0x917);
			game()->enqueue({
				seqCall(kPrimWalk, { xy(78, 65), 40 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimShow, { &_a47aee0[2] }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
			break;
		case 0x918:
			game()->consumeHeldItem();
			game()->flagClear(0x918);
			game()->enqueue({
				seqCall(kPrimWalk, { xy(54, 65), 40 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimShow, { &_a47aee0[3] }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
			break;
		// Action 0x919: same as clicking item 0x3f (the original jumps into the hotspot code)
		case 0x919:
			takeItem();
			break;
		// Action 0x91a: put item 0x3f back, which re-arms the ritual (flag 0x91d)
		case 0x91a:
			game()->consumeHeldItem();
			game()->flagClear(0x91a);
			game()->flagClear(0x91d);
			game()->enqueue({
				seqCall(kPrimWalk, { xy(72, 65), 0 }),
				seqCall(kPrimDialogue, { 0, 0x91b, 0x91b, 0, 0, 0 }),
				seqCall(kPrimWalk, { xy(72, 65), 40 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimShow, { &_s47b230 }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
			break;
		default:
			break;
		}
		return 0;
	default:
		return 0;
	}
}

/** Picks up item 0x3f and blocks the ritual until it is put back. */
void Room2322::takeItem() {
	game()->flagSet(0x91a, true);
	game()->flagSet(0x91d, true);
	game()->enqueue({
		seqCall(kPrimWalk, { xy(72, 65), 40 }),
		seqCall(kPrimWait, { 4 }),
		seqCall(kPrimHide, { &_s47b230 }),
		seqCall(kPrimAddItem, { 0x3f }),
		seqCall(kPrimFace, { 0 }),
		seqEnd()
	});
}

/** Factory used by the room registry. */
Room *createRoom2322() {
	return new Room2322();
}

} // End of namespace Gooka
