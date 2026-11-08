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
 * Room 1239: the throne room of the king of Avaral. Every visit is one
 * scripted audience, chosen by the story flags (port of the room handler
 * FUN_00416230).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 1239 (original handler FUN_00416230): four water shimmers, the
 * king, Penza (early game, flag 0x846 set) or Lidra and Digor (later), and
 * the audience played on entry: the introduction, Ruiciman's book (Gooka
 * is sent to the ship, room 1200), the return with Lidra, the farewell
 * with Digor.
 */
class Room1239 : public Room {
public:
	/** Constructs the room objects. */
	Room1239();
	/** Static descriptor 0x43fb68. */
	const RoomDescriptor *getDescriptor() override;
	/** Enter (the audience), leave and tick. */
	int handleEvent(int event, int param) override;

private:
	int _h475360 = kNoSound; ///< 0x475360: ambient sound loop (WAV 0xd48)
	ShimmerSprite _e474300;  ///< 0x474300: water (FUN_00417080())
	ShimmerSprite _e474b40;  ///< 0x474b40: water (FUN_00417080())
	ShimmerSprite _e474110;  ///< 0x474110: water (FUN_00417080())
	ShimmerSprite _e474898;  ///< 0x474898: water (FUN_00417080())
	Actor _n4752b0;          ///< 0x4752b0: the king (hotspot 0x4d9)
	Actor _n4751b0;          ///< 0x4751b0: Penza, the adviser (hotspot 0x4d8)
	Actor _n475e38;          ///< 0x475e38: princess Lidra
	Actor _n474500;          ///< 0x474500: Digor
	Sprite _s474f70;         ///< 0x474f70: room object, frame 5/6 by flag 0x846
	Sprite _s474f28;         ///< 0x474f28: room object, frame 6/7 by flag 0x846
	Sprite _s4745b0;         ///< 0x4745b0: constructed by the original but never used
};

Room1239::Room1239() : Room(1239) {
}

const RoomDescriptor *Room1239::getDescriptor() {
	return findRoomData(_id);
}

int Room1239::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: ambient sound, water, the people present and the audience chosen by the story flags
	case kEvEnter:
		game()->playSound(&_h475360, 0xd48, 0, true, -2000, 0);
		game()->placeShimmer(&_e474300, 0x4a, 0x8c, -1, 0x6e, 0x50, 0, 0x10, -1);
		game()->placeShimmer(&_e474b40, 0xd6, 0x8c, -1, 0x6e, 0x50, 0, 0x10, -1);
		game()->placeShimmer(&_e474110, 0x155, 0x8c, -1, 0x5a, 0x46, 0, 0x10, -1);
		game()->placeShimmer(&_e474898, 0x1cd, 0x8c, -1, 0x5a, 0x46, 0, 0x10, -1);
		game()->placeObjectIfFlag(0x4d9, &_n4752b0, 0x1fc, 0xa0, 0, 1, 0);
		if (!game()->flagTest(0x846)) {
			if (!game()->flagTest(0x53e))
				game()->placeObject(&_n474500, 0x17c, 0x78, 1, 8, 0, -1);
			game()->placeObject(&_n475e38, 0x1cc, 0xd2, 2, 10, 0, -1);
		} else {
			game()->placeObjectIfFlag(0x4d8, &_n4751b0, 0x17c, 0x82, 0, 3, 0);
		}
		game()->placeObject(&_s474f70, 0xa0, 0x6e, 0, game()->flagTest(0x846) + 5, -1, -1);
		game()->placeObject(&_s474f28, 300, 0x5f, 0, game()->flagTest(0x846) + 6, -1, -1);

		if (game()->flagTest(0x4da)) {
			// First audience: Gooka introduces himself, asks for Digor and is sent away
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x34, 0x59), 0x24 }),
				seqCall(kPrimDialogue, { &_n4752b0, 0x4da, 0x4e8, 1, 1, 2 }),
				seqCall(kPrimDialogue, { &_n4751b0, -0x4e9, 0x4f0, 3, 3, 2 }),
				seqCall(kPrimSay, { &_n4752b0, 0x4f1, 1, 1, 2 }),
				seqCall(kPrimFace, { 0 }),
				seqCall(kPrimWalk, { xy(0x05, 0x59), -1 }),
				seqCall(kPrimChangeRoom, { 0x4c7 }),
				seqEnd()
			});
			return 0;
		}
		if (game()->flagTest(0x4f2) && !game()->flagTest(0xd56)) {
			// Ruiciman's book: Penza calls it heresy and Gooka is taken to the ship (room 1200)
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x34, 0x59), 0x24 }),
				seqCall(kPrimDialogue, { &_n4752b0, 0x4f2, 0x4f6, 1, 1, 2 }),
				seqCall(kPrimDialogue, { 0, 0x4f7, 0x4f7, 0, 0, 0 }),
				seqCall(kPrimSay, { &_n4751b0, 0x4f8, 3, 3, 2 }),
				seqCall(kPrimDialogue, { &_n4752b0, -0x4f9, 0x4fa, 1, 1, 2 }),
				seqCall(kPrimSay, { &_n4751b0, 0x4fb, 3, 3, 2 }),
				seqCall(kPrimDialogue, { &_n4752b0, 0x4fc, 0x4fd, 1, 1, 2 }),
				seqCall(kPrimFace, { 0 }),
				seqCall(kPrimChangeRoom, { 0x4b0 }),
				seqEnd()
			});
			return 0;
		}
		if (game()->flagTest(0x4fe)) {
			// Back from Glux: Lidra and the king hear about Gurha and let Gooka try to cure Digor
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x34, 0x59), 0x24 }),
				seqCall(kPrimDialogue, { &_n475e38, 0x4fe, 0x500, 10, 10, 2 }),
				seqCall(kPrimDialogue, { &_n4752b0, 0x502, 0x506, 1, 1, 2 }),
				seqCall(kPrimSay, { &_n475e38, 0x507, 10, 10, 2 }),
				seqCall(kPrimSay, { &_n4752b0, 0x508, 1, 1, 2 }),
				seqCall(kPrimDialogue, { &_n475e38, -0x509, 0x50b, 10, 10, 2 }),
				seqCall(kPrimDialogue, { &_n4752b0, 0x50c, 0x511, 1, 1, 2 }),
				seqCall(kPrimDialogue, { &_n475e38, -0x512, 0x515, 10, 10, 2 }),
				seqCall(kPrimFace, { 0 }),
				seqCall(kPrimWalk, { xy(0x05, 0x59), -1 }),
				seqCall(kPrimChangeRoom, { 0x4c7 }),
				seqEnd()
			});
			return 0;
		}
		// Digor is cured: he sends Gooka to the fortress; the gloves (item 0x40) while flag 0x81b is clear
		game()->enqueue({
			seqCall(kPrimWalk, { xy(0x34, 0x59), 0x24 }),
			seqCall(kPrimSay, { &_n474500, 0x516, 8, 8, 2 }),
			seqCall(kPrimDialogue, { &_n475e38, 0x517, 0x518, 10, 10, 2 }),
			seqCall(kPrimDialogue, { &_n474500, 0x519, 0x524, 8, 8, 2 }),
			seqCall(kPrimSay, { &_n4752b0, 0x525, 1, 1, 2 }),
			seqCall(kPrimDialogue, { &_n474500, -0x526, 0x52e, 8, 8, 2 }),
			seqIf(kPrimIfNotFlag, { 0x81b }),
			seqCall(kPrimSay, { &_n475e38, 0x52f, 10, 10, 2 }),
			seqCall(kPrimSay, { &_n474500, 0x530, 8, 8, 2 }),
			seqCall(kPrimAddItem, { 0x40 }),
			seqEndIf(),
			seqCall(kPrimDialogue, { &_n475e38, 0x531, 0x532, 10, 10, 2 }),
			seqCall(kPrimSay, { &_n4752b0, 0x533, 1, 1, 2 }),
			seqCall(kPrimSay, { &_n474500, 0x534, 8, 8, 2 }),
			seqCall(kPrimFace, { 0 }),
			seqCall(kPrimWalk, { xy(0x05, 0x59), -1 }),
			seqCall(kPrimChangeRoom, { 0x4c7 }),
			seqEnd()
		});
		return 0;

	// Room left: stop the ambient sound
	case kEvLeave:
		game()->stopSound(&_h475360);
		return 0;

	// Every 3rd tick: all four water shimmers move
	case kEvTick:
		if ((int)ticks() % 3 != 0)
			return 0;
		_e474300.step();
		_e474b40.step();
		_e474110.step();
		_e474898.step();
		return 0;

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom1239() {
	return new Room1239();
}

} // End of namespace Gooka
