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
 * Room 2615: close-up of the code lock next to the computer room door
 * (port of the room handler FUN_0041b7a0). Keys 1-9 shift digits into an
 * eight-digit display; the code 63829417 opens the door in room 2613.
 * A remote control (items 0x45-0x48) used on a key "decodes the lock":
 * the display flickers random digits until the next key press.
 *
 * The original code block (0x41b780-0x41e1f0) ends with the handler at
 * 0x41bbf0; the rest is engine code (Game globals, tick, menu, save) that
 * the room does not use.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** The lock code (table 0x4419c8), compared with the display frames. */
static const int kRoom2615Code[8] = { 6, 3, 8, 2, 9, 4, 1, 7 };

/**
 * Room 2615 (original handler FUN_0041b7a0): display, keypad (hotspots
 * 0xca0-0xca8 = keys 1-9, 0xc97 = back) and the decoder (flag -0xa39).
 */
class Room2615 : public Room {
public:
	Room2615();
	/** Static descriptor 0x441998. */
	const RoomDescriptor *getDescriptor() override;
	/** Enter/leave, the code check and decoder tick, keys, the decoder action. */
	int handleEvent(int event, int param) override;

private:
	Sprite _a4775e0[8];  ///< display digits (frames 1-9)
	Sprite _a476ed0[10]; ///< key caps of keys 1-9 (frame 20 up, 22 pressed; the 10th is unused)
	Sprite _a476a98[10]; ///< key digits 1-9 (frames 11-19; the 10th is unused)
};

Room2615::Room2615() : Room(2615) {
}

const RoomDescriptor *Room2615::getDescriptor() {
	return findRoomData(_id);
}

int Room2615::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: empty display, keys 1-9 in rows from the top; the hero is hidden
	case kEvEnter: {
		Sprite *digit = _a4775e0;
		for (int x = 0xff; x < 0x187; x += 0x11) {
			game()->placeObject(digit, x, 0x7a, 0, 0, 0, -1);
			digit++;
		}
		int id = 0xca0;
		int i = 0;
		for (int y = 0xcc; y < 0x156; y += 0x2e) {
			for (int x = 0x109; x < 0x193; x += 0x2e) {
				game()->placeObjectIfFlag(id, &_a476ed0[i], x - 0xc, y - 8, 1, 0x14, 0);
				game()->placeObjectIfFlag(id, &_a476a98[i], x, y, 1, id - 0xc95, 1);
				i++;
				id++;
			}
		}
		hero().hide();
		return 0;
	}

	// Room left: hero visible again, decoder off
	case kEvLeave:
		hero().show();
		game()->flagClear(-0xa39);
		return 0;

	// Every tick: without the decoder the right code opens the door (flag 0xa3a cleared, back to 2613); with it the display flickers every other tick
	case kEvTick:
		if (!game()->flagTest(-0xa39)) {
			for (int i = 0; i < 8; i++) {
				if (_a4775e0[i].frame != kRoom2615Code[i])
					return 0;
			}
			debugC(1, kDebugScript, "Room 2615: lock code accepted");
			game()->flagClear(0xa3a);
			game()->changeRoom(0xa35);
			return 0;
		}
		if ((int)ticks() % 2 == 0) {
			game()->playSound(nullptr, 0xee4, 0, false, 0, 0);
			for (int i = 0; i < 8; i++)
				_a4775e0[i].setFrame(game()->rnd(9) + 1);
			return 0;
		}
		return 0;

	// Hotspot clicked
	case kEvHotspot:
		// Hotspots 0xca0-0xca8: key 1-9 - decoder off, key pressed, digit shifted in (event 0x17)
		if (param >= 0xca0 && param <= 0xca8) {
			const int n = param - 0xca0;
			game()->flagClear(-0xa39);
			game()->enqueue({
				seqCall(kPrimAnimate, { &_a476ed0[n], 0x16, 1 }),
				seqCall(kPrimSound, { 0xee3 }),
				seqCall(kPrimRaiseEvent, { 0x17, n + 1 }),
				seqCall(kPrimAnimate, { &_a476ed0[n], 0x14, 1 }),
				seqEnd()
			});
		}
		// Hotspot 0xc97: back to the corridor (2613)
		if (param == 0xc97) {
			game()->changeRoom(0xa35);
			return 0;
		}
		return 0;

	// Item on a hotspot: remote control I-IV (0x45-0x48) on a key = "decode the lock" (action 0xa39)
	case kEvItemOnHotspot: {
		const uint hotspot = (uint)param & 0xffff;
		const uint item = ((uint)param >> 16) & 0xffff;
		if (hotspot >= 0xca0 && hotspot <= 0xca8 && item >= 0x45 && item <= 0x48)
			return 0xa39;
		return 0;
	}

	// Action executed: 0xa39 attaches the decoder
	case kEvAction:
		if (param == 0xa39) {
			game()->flagSet(-0xa39, true);
			return 0;
		}
		return 0;

	// Key released (raised by the key sequence): the display shifts left, the key's digit enters on the right
	case kEvRoomPrivate1:
		for (int i = 0; i < 7; i++)
			_a4775e0[i].setFrame(_a4775e0[i + 1].frame);
		_a4775e0[7].setFrame(param);
		return 0;

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom2615() {
	return new Room2615();
}

} // End of namespace Gooka
