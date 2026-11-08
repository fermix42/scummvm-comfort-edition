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
 * Room 2610: close-up of the lift control panel inside the spaceship (port
 * of the room handler FUN_0041a0b0). The upper display shows the floor the
 * lift is at (var 0xa32), the lower one the floor typed on the keypad;
 * GO rides there (floor 315 = room 2626, 420 = 2632, 447 = 2410, any other
 * floor below 448 = one of the landings 2633-2635), EXIT leaves the lift.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 2610 (original handler FUN_0041a0b0): keypad buttons 0-9, GO and
 * EXIT (hotspots 0xc9f-0xcaa), two three-digit displays and the ride.
 * Flag 0xa32 is cleared while the lift moves.
 */
class Room2610 : public Room {
public:
	Room2610();
	/** Static descriptor 0x441640. */
	const RoomDescriptor *getDescriptor() override;
	/** Enter, floor counter tick, button presses and their effect (event 0x17). */
	int handleEvent(int event, int param) override;

private:
	/** Shows `floor` on the upper display (the jump target LAB_0041a34a). */
	void showCurrentFloor(int floor);
	/** Number on the upper display (frames of its three digits). */
	int currentFloor() const;
	/** Number typed on the keypad (frames of the lower display). */
	int typedFloor() const;

	Sprite _a4774d8[3];  ///< lower display: typed floor (frames 0-9; 10, 11, 11 = error)
	Sprite _a476df8[3];  ///< upper display: floor the lift is at
	Sprite _a477188[12]; ///< buttons 0-9, GO (10), EXIT (11); frame 12 + n up, 24 + n pressed
};

Room2610::Room2610() : Room(2610) {
}

const RoomDescriptor *Room2610::getDescriptor() {
	return findRoomData(_id);
}

void Room2610::showCurrentFloor(int floor) {
	_a476df8[0].setFrame((floor / 100) % 10);
	_a476df8[1].setFrame((floor / 10) % 10);
	_a476df8[2].setFrame(floor % 10);
}

int Room2610::currentFloor() const {
	return _a476df8[2].frame + (_a476df8[1].frame + _a476df8[0].frame * 10) * 10;
}

int Room2610::typedFloor() const {
	return _a4774d8[2].frame + (_a4774d8[1].frame + _a4774d8[0].frame * 10) * 10;
}

int Room2610::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: keypad (0 bottom left, 1-9 in rows from the bottom, GO, EXIT), both displays; the hero is hidden
	case kEvEnter: {
		game()->placeObjectIfFlag(0xc9f, &_a477188[0], 0x10e, 0x13c, 1, 0xc, 0);
		game()->placeObjectIfFlag(0xca9, &_a477188[10], 0x12e, 0x13c, 1, 0x16, 0);
		game()->placeObjectIfFlag(0xcaa, &_a477188[11], 0x14e, 0x13c, 1, 0x17, 0);
		int id = 0xca0;
		Sprite *button = &_a477188[1];
		for (int y = 0x11c; y > 0xdb; y -= 0x20) {
			for (int x = 0x10e; x < 0x16e; x += 0x20) {
				game()->placeObjectIfFlag(id, button, x, y, 1, id - 0xc93, 0);
				button++;
				id++;
			}
		}
		hero().hide();
		int i = 0;
		for (int x = 0x112; x < 0x16c; x += 0x1e) {
			game()->placeObject(&_a4774d8[i], x, 0x99, 0, 0, 0, -1);
			game()->placeObject(&_a476df8[i], x, 0x52, 0, 0, 0, -1);
			i++;
		}
		showCurrentFloor(game()->var(0xa32));
		return 0;
	}

	// Every tick: while the lift moves (flag 0xa32 clear) the upper display counts towards the typed floor; arrival sets the flag
	case kEvTick:
		if (!game()->flagTest(0xa32)) {
			int floor = currentFloor();
			const int target = typedFloor();
			if (floor == target)
				game()->flagSet(0xa32, true);
			else if (floor < target)
				floor++;
			else
				floor--;
			showCurrentFloor(floor);
		}
		return 0;

	// Hotspot clicked
	case kEvHotspot:
		// Hotspots 0xc9f-0xcaa: keypad button n - pressed with a click, then event 0x17 with n
		if (param >= 0xc9f && param <= 0xcaa) {
			const int n = param - 0xc9f;
			game()->enqueue({
				seqCall(kPrimSetFrame, { &_a477188[n], n + 0x18 }),
				seqCall(kPrimSound, { 0xee3 }),
				seqCall(kPrimWait, { 5 }),
				seqCall(kPrimSetFrame, { &_a477188[n], n + 0xc }),
				seqCall(kPrimRaiseEvent, { 0x17, n }),
				seqEnd()
			});
		}
		return 0;

	// Button released (raised by the press sequence): digits 0-9 shift into the lower display, 10 = GO, 11 = EXIT
	case kEvRoomPrivate1: {
		const uint button = (uint)param;
		if (button < 10) {
			// After the error display a digit starts a new number
			if (_a4774d8[0].frame == 10) {
				_a4774d8[0].setFrame(0);
				_a4774d8[1].setFrame(0);
				_a4774d8[2].setFrame(0);
			}
			_a4774d8[0].setFrame(_a4774d8[1].frame);
			_a4774d8[1].setFrame(_a4774d8[2].frame);
			_a4774d8[2].setFrame(button);
		}
		const int floor = typedFloor();
		// EXIT: leave the lift onto a landing chosen by the typed number
		if (button == 11) {
			hero().show();
			game()->changeRoom(floor % 3 + 0xa49);
		}
		// GO: floors above 447 do not exist (error display), else ride there
		if (button == 10) {
			if (floor > 0x1bf) {
				_a4774d8[0].setFrame(10);
				_a4774d8[1].setFrame(0xb);
				_a4774d8[2].setFrame(0xb);
				return 0;
			}
			game()->var(0xa32) = floor;
			int room;
			if (floor == 0x1a4)
				room = 0xa48;
			else if (floor == 0x13b)
				room = 0xa42;
			else if (floor == 0x1bf)
				room = 0x96a;
			else
				room = floor % 3 + 0xa49;
			debugC(1, kDebugScript, "Room 2610: lift to floor %d, room %d", floor, room);
			game()->enqueue({
				seqCall(kPrimClearFlag, { 0xa32 }),
				seqCall(kPrimSound, { 0xf17 }),
				seqCall(kPrimWait, { 10 }),
				seqCall(kPrimAmbient, { 0xf18 }),
				seqWhile(kPrimIfNotFlag, { 0xa32 }),
				seqYield(),
				seqEndWhile(),
				seqCall(kPrimSetFlag, { 0xa32 }),
				seqCall(kPrimSound, { 0xf19 }),
				seqCall(kPrimAmbientStop),
				seqCall(kPrimWait, { 50 }),
				seqCall(kPrimSoundWait, { 0xa30 }),
				seqCall(kPrimChangeRoom, { room }),
				seqCall(kPrimShow, { &hero() }),
				seqEnd()
			});
		}
		return 0;
	}

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom2610() {
	return new Room2610();
}

} // End of namespace Gooka
