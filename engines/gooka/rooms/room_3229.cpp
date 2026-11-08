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
 * Room3229: port of the room handler FUN_0042a220.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** The code that opens the safe, one digit per display (table 0x4450c8). */
static const int kRoom3229Code[6] = { 3, 1, 9, 7, 4, 5 };

/**
 * Room 3229 (original handler FUN_0042a220): close-up of the safe (flag
 * 0xc9d = locked) with a six-digit display and a 3x3 keypad (hotspots
 * 0xca0..0xca8 = digits 1..9, each key shifts its digit in from the
 * right). Controller I (item 0x45) on the keypad starts the decoder
 * (action 0x32f "decode the lock", flag -0x32f): the display runs through
 * random digits and fixes the right ones one by one. When the display
 * shows kRoom3229Code the door slides open and the pick-ups 11 (paper)
 * and 66 (shem) behind it can be taken. Hotspot 0xc97 goes back to room
 * 809.
 */
class Room3229 : public Room {
public:
	Room3229();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;

private:
	Sprite _a47cc68[6];  ///< array at 0x47cc68 (FUN_0040ff60): the display digits (frame = digit)
	Sprite _a47c898[10]; ///< array at 0x47c898 (FUN_0040ff60): the keys (frame 20 up, 22 down; 9 used)
	Sprite _a47e478[10]; ///< array at 0x47e478 (FUN_0040ff60): the key labels (frames 11..19; 9 used)
	Sprite _s47e3a0;     ///< object at 0x47e3a0 (FUN_0040ff60()): the safe door
	int _lockDelay;      ///< DAT_0047c5e4: fixes the next digit when the countdown drops below it (halves each time)
	int _countdown;      ///< DAT_0047c894: decoder countdown
	int _scramble;       ///< DAT_0047e474: source of the running digits
	int _prevRoom;       ///< DAT_0047cb40: previous room, written on entry but never read
};

/** Constructs the room statics. */
Room3229::Room3229() : Room(3229), _lockDelay(0), _countdown(0), _scramble(0), _prevRoom(0) {
}

/** Static descriptor 0x445098. */
const RoomDescriptor *Room3229::getDescriptor() {
	return findRoomData(_id);
}

/** Room events (FUN_0042a220, `what` >= 2). */
int Room3229::handleEvent(int event, int param) {
	switch (event) {
	// Enter: display, keypad, the door while locked; no hero in the close-up; decoder timing reset
	case kEvEnter: {
		int i = 0;
		for (int x = 0x1c7; x < 0x23f; x += 0x14, i++)
			game()->placeObject(&_a47cc68[i], x, 0x42, 1, 0, 0, -1);
		int key = 0;
		int labelRow = 0xb;
		for (int y = 0xa0; y < 0x160; y += 0x40, labelRow += 3) {
			int label = labelRow;
			for (int x = 0x1a0; x < 0x260; x += 0x40, label++, key++) {
				game()->placeObject(&_a47c898[key], x, y, 1, 0x14, 0, -1);
				game()->placeObject(&_a47e478[key], x, y, 1, label, 1, -1);
			}
		}
		game()->placeObjectIfFlagNoHotspot(0xc9d, &_s47e3a0, 0x41, 0xbd, 0, 0x17, 1);
		hero().hide();
		_lockDelay = 0x40;
		_prevRoom = param;
		_countdown = 0x80;
		return 0;
	}

	// Leave: show the hero again
	case kEvLeave:
		hero().show();
		return 0;

	// Tick: run the decoder every other tick; open the safe once the display shows the code
	case kEvTick:
		if (game()->flagTest(-0x32f) && (int)ticks() % 2 == 0 && game()->flagTest(0xc9d)) {
			game()->playSound(nullptr, 0xee4, 0, false, 0, 0);
			// Digits not fixed yet show running numbers that never equal the right one
			for (int i = 0; i < 6; i++) {
				int digit = _scramble % 10;
				_scramble++;
				if (kRoom3229Code[i] != _a47cc68[i].frame) {
					if (digit == kRoom3229Code[i])
						digit = (digit + 1) % 9 + 1;
					_a47cc68[i].setFrame(digit);
				}
			}
			if (_countdown < _lockDelay) {
				// Fix a random wrong digit
				_lockDelay >>= 1;
				// Port: the original loops forever when every digit is already right (only possible
				// when a key sequence delays the open check below for two decoder steps)
				bool allRight = true;
				for (int i = 0; i < 6; i++) {
					if (_a47cc68[i].frame != kRoom3229Code[i])
						allRight = false;
				}
				if (!allRight) {
					int i;
					do {
						i = game()->rnd(6);
					} while (_a47cc68[i].frame == kRoom3229Code[i]);
					_a47cc68[i].setFrame(kRoom3229Code[i]);
					game()->playSound(nullptr, 0xee3, 0, false, 0, 0);
				}
			} else {
				_countdown--;
			}
		}
		if (!game()->sequenceBusy() && game()->flagTest(0xc9d)) {
			for (int i = 0; i < 6; i++) {
				if (kRoom3229Code[i] != _a47cc68[i].frame)
					return 0;
			}
			// The display shows the code: unlock and slide the door open
			game()->flagClear(0xc9d);
			game()->enqueue({
				seqCall(kPrimAmbient, { 3859 }),
				seqCall(kPrimMoveAxis, { &_s47e3a0, 0, 20, -1, 23, 23, 1 }),
				seqCall(kPrimSound, { 3860 }),
				seqCall(kPrimAmbientStop),
				seqEnd()
			});
			return 0;
		}
		break;

	// Hotspots clicked
	case kEvHotspot:
		if (param >= 0xca0 && param <= 0xca8) {
			// Hotspots 0xca0..0xca8: a key - press it, event 0x17 shifts its digit (1..9) into the display
			const int index = param - 0xca0;
			Sprite *keySprite = &_a47c898[index];
			game()->enqueue({
				seqCall(kPrimAnimate, { keySprite, 22, 2 }),
				seqCall(kPrimSound, { 3811 }),
				seqCall(kPrimRaiseEvent, { 23, index + 1 }),
				seqCall(kPrimAnimate, { keySprite, 20, 2 }),
				seqEnd()
			});
		}
		if (param == 0xc97) {
			// Hotspot 0xc97: leave the close-up to room 809
			game()->changeRoom(0x329);
			return 0;
		}
		break;

	// Held item over a hotspot: Controller I (0x45) on a key = action 0x32f "decode the lock"
	case kEvItemOnHotspot:
		if ((param & 0xFFFF) >= 0xca0 && (param & 0xFFFF) <= 0xca8 && (uint32)param >> 16 == 0x45)
			return 0x32f;
		break;

	// Action 0x32f: start the decoder
	case kEvAction:
		if (param == 0x32f) {
			game()->flagSet(-0x32f, true);
			return 0;
		}
		break;

	// Hotspot lookup: while the safe is locked, the pick-ups 0xb and 0x42 behind the door act as the exit 0xc97
	case kEvHotspotOverride:
		if (game()->flagTest(0xc9d) && param == 0xb)
			return 0xc97;
		if (game()->flagTest(0xc9d) && param == 0x42)
			return 0xc97;
		break;

	// Event 0x17 (raised by the key sequence): shift the display left, the new digit enters on the right
	case kEvRoomPrivate1:
		for (int i = 0; i < 5; i++)
			_a47cc68[i].setFrame(_a47cc68[i + 1].frame);
		_a47cc68[5].setFrame(param);
		break;

	default:
		break;
	}
	return 0;
}

/** Factory used by the room registry. */
Room *createRoom3229() {
	return new Room3229();
}

} // End of namespace Gooka
