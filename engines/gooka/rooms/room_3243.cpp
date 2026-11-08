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
 * Room3243: port of the room handler FUN_00429420.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 3243 (original handler FUN_00429420): the map of Janatris. The hero
 * is a small animated figure (frames 0x8a..0x8f, unscaled) that walks
 * between the destinations (exits 927, 1200, 2300, 2400). Locations become
 * available as flags of the story are cleared; some arrivals are scripted
 * trips to rooms 1200, 2000 and 2037.
 */
class Room3243 : public Room {
public:
	Room3243();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;

private:
	Sprite _s47c850; ///< object at 0x47c850 (FUN_0040ff60()): always shown, frame 0
	Sprite _s47cc20; ///< object at 0x47cc20 (FUN_0040ff60()): shown until flag 0x399 is cleared
	Sprite _s47cb48; ///< object at 0x47cb48 (FUN_0040ff60()): shown until flag 0x846 is cleared
	Sprite _s47cb90; ///< object at 0x47cb90 (FUN_0040ff60()): shown until flag 0x6ae is cleared
	Sprite _s47e3e8; ///< object at 0x47e3e8 (FUN_0040ff60()): shown until flag 0x516 is cleared
	Sprite _s47c808; ///< object at 0x47c808 (FUN_0040ff60()): always shown, frame 6
	Sprite _s47e358; ///< object at 0x47e358 (FUN_0040ff60()): shown until flag 0x399 is cleared
	Sprite _s47c5a0; ///< object at 0x47c5a0 (FUN_0040ff60()): shown until flag 0x846 is cleared
	Sprite _s47e430; ///< object at 0x47e430 (FUN_0040ff60()): shown until flag 0x6ae is cleared
	Sprite _s47cbd8; ///< object at 0x47cbd8 (FUN_0040ff60()): shown until flag 0x516 is cleared
};

/** Constructs the room statics. */
Room3243::Room3243() : Room(3243) {
}

/** Static descriptor 0x444b80. */
const RoomDescriptor *Room3243::getDescriptor() {
	return findRoomData(_id);
}

/** Room events (FUN_00429420, `what` >= 2). */
int Room3243::handleEvent(int event, int param) {
	switch (event) {
	// Enter: update the map from the story flags, then a scripted trip or the map music
	case kEvEnter:
		// Travelling spills the water: bowl with water (0x26) -> bowl (0x20)
		game()->replaceItem(0x26, 0x20, false);
		game()->flagSet(0x36c, true);
		game()->flagSet(0x36d, true);
		game()->flagClear(0x333);
		game()->placeObject(&_s47c850, 0x118, 0x136, 0, 0, 0, -1);
		game()->placeObject(&_s47c808, 0x11b, 0x126, 0, 6, 0, -1);
		// Destinations 1200 / 2300 / 2400 (flags 0x4b0 / 0x8fc / 0x960) open once flag 0x399 / 0x6ae / 0x516 is cleared
		game()->flagSet(0x4b0, !game()->flagTest(0x399));
		game()->placeObjectIfNotFlag(0x399, &_s47cc20, 0x129, 0x7d, 0, 1, 0);
		game()->placeObjectIfNotFlag(0x399, &_s47e358, 0x167, 0x9e, 0, 6, 0);
		game()->placeObjectIfNotFlag(0x846, &_s47cb48, 0x111, 10, 0, 2, 0);
		game()->placeObjectIfNotFlag(0x846, &_s47c5a0, 0x111, 0x2e, 0, 5, 0);
		game()->flagSet(0x8fc, !game()->flagTest(0x6ae));
		game()->placeObjectIfNotFlag(0x6ae, &_s47cb90, 7, 0xa0, 0, 3, 0);
		game()->placeObjectIfNotFlag(0x6ae, &_s47e430, 0x5b, 0xb2, 0, 6, 0);
		game()->flagSet(0x960, !game()->flagTest(0x516));
		game()->placeObjectIfNotFlag(0x516, &_s47e3e8, 0x1a4, 0xb2, 0, 4, 0);
		game()->placeObjectIfNotFlag(0x516, &_s47cbd8, 0x1af, 0xa6, 0, 6, 0);
		if (param == 0x844) {
			// From room 2116: trip to room 1200
			game()->enqueue({
				seqCall(kPrimPlaceHero, { 74, 15, 138 }),
				seqCall(kPrimWalk, { xy(90, 41), -1 }),
				seqCall(kPrimChangeRoom, { 1200 }),
				seqCall(kPrimPlaceHero, { 48, 96, 0 }),
				seqEnd()
			});
			return 0;
		}
		if (param == 0x4b0 && game()->flagTest(0x846) && !game()->flagTest(0x4fd)) {
			// From room 1200 (0x846 set, 0x4fd clear): trip to room 2000
			g_engine->_music->play(0x1a9, true);
			game()->enqueue({
				seqCall(kPrimPlaceHero, { 90, 41, 138 }),
				seqCall(kPrimWalk, { xy(87, 36), -1 }),
				seqCall(kPrimChangeRoom, { 2000 }),
				seqEnd()
			});
		} else {
			if (param != 2000) {
				g_engine->_music->play(0x1a8, true);
				return 0;
			}
			// From room 2000: trip on to room 2037
			g_engine->_music->play(0x1a9, true);
			game()->enqueue({
				seqCall(kPrimPlaceHero, { 87, 36, 138 }),
				seqCall(kPrimWalk, { xy(74, 24), -1 }),
				seqCall(kPrimChangeRoom, { 2037 }),
				seqEnd()
			});
		}
		return 0;

	// Tick: animate the hero figure (frames 0x8a..0x8f, one step every 4 ticks), always unscaled
	case kEvTick:
		hero().setFrame((int)ticks() / 4 % 6 + 0x8a);
		hero().setScale(0x100);
		return 0;

	// Hotspot 0xcb3: swallowed (-1 = handled, nothing happens)
	case kEvHotspot:
		if (param != 0xcb3)
			return 0;
		return -1;

	default:
		break;
	}
	return 0;
}

/** Factory used by the room registry. */
Room *createRoom3243() {
	return new Room3243();
}

} // End of namespace Gooka
