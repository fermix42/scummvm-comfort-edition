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
 * Room2304: port of the room handler FUN_00422e90.
 * Holds item 0x3e (object/hotspot 0x901) and the exit to room 2312
 * (hotspot 0x908), which saves the game to GAME.SAVE before the hero
 * leaves. While flag 0x90a is set the hero cannot stay: arriving from 2312
 * he turns back into 2312 at once, otherwise he walks off to room 2300
 * after about 500 ticks.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** Room 2304 (original handler FUN_00422e90). */
class Room2304 : public Room {
public:
	Room2304();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;

private:
	// The original builds these two with an inlined Sprite constructor
	// (vtable 0x437930, frame 0, scale 0x100), so the stub generator missed them.
	Sprite _s47b040; ///< object at 0x47b040: the object holding item 0x3e (hotspot 0x901; frame 1 = taken)
	Sprite _s47b1e8; ///< object at 0x47b1e8: shown on the return from room 2312
	int _timer;      ///< DAT_0047b278: ticks in the room, starts at rnd(100)
};

/** Creates the room objects. */
Room2304::Room2304() : Room(2304), _timer(0) {
}

/** Static descriptor DAT_00442978. */
const RoomDescriptor *Room2304::getDescriptor() {
	return findRoomData(_id);
}

/** Room events: item 0x3e, the autosave exit and the timed send-back. */
int Room2304::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: object with item 0x3e, return from 2312, restart the timer
	case kEvEnter:
		if (game()->flagTest(0x904))
			game()->flagSet(0x8fe, true);
		if (!game()->flagTest(0x8fe) && !game()->flagTest(0x904)) {
			if (game()->flagTest(0x906))
				game()->sayIdle(0x906);
			game()->placeObjectIfFlag(0x901, &_s47b040, 0x50, 0x25, 0, game()->flagTest(0x3e) ? 0 : 1, 1000);
		}
		if (param == 0x908 && game()->flagTest(0x90a)) {
			game()->placeObject(&_s47b1e8, 0xdc, 0xa0, 0, 2, 0x3e9, -1);
			game()->enqueue({
				seqCall(kPrimWalk, { xy(59, 68), 0 }),
				seqCall(kPrimDialogue, { 0, 0x902, 0x902, 0, 0, 0 }),
				seqCall(kPrimWalk, { xy(59, 67), -1 }),
				seqCall(kPrimChangeRoom, { 2312 }),
				seqEnd()
			});
		}
		_timer = game()->rnd(100);
		return 0;
	// Tick: after 500 ticks with flag 0x90a the hero gives up and walks to room 2300
	case kEvTick:
		_timer++;
		if (_timer > 500 && !game()->sequenceBusy() && game()->flagTest(0x90a)) {
			game()->enqueue({
				seqCall(kPrimDropHeld),
				seqCall(kPrimFace, { 39 }),
				seqCall(kPrimDialogue, { 0, 0x903, 0x903, 0, 0, 0 }),
				seqCall(kPrimWalk, { xy(109, 99), -1 }),
				seqCall(kPrimChangeRoom, { 2300 }),
				seqEnd()
			});
			return 0;
		}
		return 0;
	// Hotspot clicked
	case kEvHotspot:
		// Hotspot 0x901: take item 0x3e if it is still there
		if (param == 0x901) {
			if (game()->flagTest(0x3e)) {
				game()->enqueue({
					seqCall(kPrimDialogue, { 0, 0xe00, 0xe00, 0, 0, 0 }),
					seqCall(kPrimWalk, { xy(55, 84), 91 }),
					seqCall(kPrimSoundWait, { 0xf05 }),
					seqCall(kPrimSetFrame, { &_s47b040, 1 }),
					seqCall(kPrimAddItem, { 0x3e }),
					seqCall(kPrimClearFlag, { 0x3e }),
					seqCall(kPrimFace, { 0 }),
					seqEnd()
				});
			}
			return 1;
		}
		// Hotspot 0x908: exit to room 2312 - save the game first, then leave through the exit
		if (param == 0x908) {
			// FUN_0041cd20("GAME.SAVE"): checkpoint, as ScummVM's autosave (the original also
			// re-entered the room afterwards, which only rebuilds its sprites)
			g_engine->saveAutosaveIfEnabled();
			return 0;
		}
		return 0;
	// Held item 0x34 over hotspot 0x905: action 0x904 while flag 0x904 is set
	case kEvItemOnHotspot:
		if (param == 0x340905)
			return game()->flagTestId(0x904);
		return 0;
	// Action 0x904: item 0x34 used up on hotspot 0x905
	case kEvAction:
		if (param == 0x904) {
			game()->consumeHeldItem();
			game()->flagClear(0x904);
			game()->enqueue({
				seqCall(kPrimWalk, { xy(49, 77), 42 }),
				seqCall(kPrimWait, { 10 }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
			return 0;
		}
		return 0;
	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom2304() {
	return new Room2304();
}

} // End of namespace Gooka
