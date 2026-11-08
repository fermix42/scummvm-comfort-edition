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
 * Shared room templates FUN_00419b70 ... FUN_00419dd0 (the corridor
 * close-ups 2626-2658). The static descriptors below are copied from
 * GAME.EXE; the hotspot entries and exit ids the original fills in at run
 * time are -1 placeholders.
 */

#include "gooka/rooms/room_template_419.h"

namespace Gooka {

/** Hotspot table of a template before the room fills it in (12 dwords of -1). */
static const int kNoHotspots[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

// FUN_00419b70: descriptor 0x440dc8, zones 0x440bd8, exits 0x440c60
static const ZoneRecord kZones419b70[] = {
	{ 0, 256, 99, 6, 0, 0, 0 },
	{ 1, 256, 99, 6, 0, 0, 0 },
	{ 2, 256, 99, 6, 0, 0, 0 }
};
static const ExitRecord kExits419b70[] = {
	{ { -1, 64, 97, -1, 64, 99, 39, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { -1, 48, 62, -1, 39, 62, 36, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { -1, 70, 56, -1, 70, 56, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { -1, 124, 74, -1, 133, 74, 34, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { 0 } }
};
static const RoomDescriptor kDesc419b70 = {
	2603, kZones419b70, 3, kNoHotspots, kExits419b70, 0, 432, 0, 0, 0, 0, 12
};

// FUN_00419c00: descriptor 0x440fe8, zones 0x440df8, exits 0x440e80
static const ZoneRecord kZones419c00[] = {
	{ 0, 256, 99, 6, 0, 0, 0 },
	{ 1, 256, 99, 6, 0, 0, 0 },
	{ 2, 256, 99, 6, 0, 0, 0 }
};
static const ExitRecord kExits419c00[] = {
	{ { -1, 93, 97, -1, 93, 99, 39, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { -1, 34, 74, -1, 26, 74, 36, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { -1, 90, 57, -1, 90, 57, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { -1, 110, 64, -1, 117, 64, 34, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { 0 } }
};
static const RoomDescriptor kDesc419c00 = {
	2604, kZones419c00, 3, kNoHotspots, kExits419c00, 0, 432, 0, 0, 0, 0, 12
};

// FUN_00419c90: descriptor 0x4411a0, zones 0x441018, exits 0x441080
static const ZoneRecord kZones419c90[] = {
	{ 1, 256, 99, 6, 0, 0, 0 },
	{ 0, 256, 99, 6, 0, 0, 0 }
};
static const ExitRecord kExits419c90[] = {
	{ { -1, 78, 97, -1, 78, 99, 39, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { -1, 48, 50, -1, 40, 50, 36, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { -1, 111, 50, -1, 119, 50, 34, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { 0 } }
};
static const RoomDescriptor kDesc419c90 = {
	2605, kZones419c90, 2, kNoHotspots, kExits419c90, 0, 432, 0, 0, 0, 0, 12
};

// FUN_00419d10: descriptor 0x4412f8, zones 0x4411d0, exits 0x441220
static const ZoneRecord kZones419d10[] = {
	{ 0, 256, 99, 6, 0, 0, 0 }
};
static const ExitRecord kExits419d10[] = {
	{ { -1, 63, 45, -1, 63, 45, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { -1, 63, 97, -1, 63, 99, 39, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { 0 } }
};
static const RoomDescriptor kDesc419d10 = {
	2606, kZones419d10, 1, kNoHotspots, kExits419d10, 0, 432, 0, 0, 0, 0, 12
};

// FUN_00419d70: descriptor 0x441450, zones 0x441328, exits 0x441378
static const ZoneRecord kZones419d70[] = {
	{ 0, 256, 99, 6, 0, 0, 0 }
};
static const ExitRecord kExits419d70[] = {
	{ { -1, 97, 46, -1, 97, 46, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { -1, 97, 97, -1, 97, 99, 39, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { 0 } }
};
static const RoomDescriptor kDesc419d70 = {
	2607, kZones419d70, 1, kNoHotspots, kExits419d70, 0, 432, 0, 0, 0, 0, 12
};

// FUN_00419dd0: descriptor 0x4415c0, zones 0x441480, exits 0x4414e8;
// slot 3 of its hotspot table is the lift button 0xa32 (cursor type 0)
static const ZoneRecord kZones419dd0[] = {
	{ 1, 256, 99, 0, 0, 0, 0 },
	{ 0, 256, 99, 0, 0, 0, 0 }
};
static const int kHotspots419dd0[] = { -1, -1, 0xa32, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
static const ExitRecord kExits419dd0[] = {
	{ { -1, 5, 97, -1, 0, 97, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { -1, 155, 97, -1, 159, 97, 39, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { 0 } }
};
static const RoomDescriptor kDesc419dd0 = {
	2608, kZones419dd0, 2, kHotspots419dd0, kExits419dd0, 2609, 432, 0, 0, 0, 0, 12
};

// ---------------------------------------------------------------------------
// RoomTemplate419
// ---------------------------------------------------------------------------

RoomTemplate419::RoomTemplate419(int id, const RoomDescriptor &layout) : Room(id), _desc(layout) {
	for (int i = 0; i < kNumHotspotSlots; i++)
		_hotspots[i] = layout.hotspots[i];

	// Copy the exit records; the unused ones (at least the last) stay zero = terminator
	for (int i = 0; i < kMaxExits; i++) {
		for (uint f = 0; f < ARRAYSIZE(_exits[i].f); f++)
			_exits[i].f[f] = 0;
	}
	for (int i = 0; i < kMaxExits - 1 && layout.exits[i].id() != 0; i++)
		_exits[i] = layout.exits[i];

	_desc.hotspots = _hotspots;
	_desc.numHotspots = kNumHotspotSlots;
	_desc.exits = _exits;
}

const RoomDescriptor *RoomTemplate419::getDescriptor() {
	return &_desc;
}

int RoomTemplate419::handleEvent(int event, int param) {
	return 0;
}

void RoomTemplate419::setHotspot(int slot, int id, int cursor) {
	_hotspots[slot] = (id & 0xffff) | (cursor << 16);
}

void RoomTemplate419::setExitId(int index, int id) {
	_exits[index].f[0] = id;
}

// ---------------------------------------------------------------------------
// The templates: the writes the original does at the start of every call
// ---------------------------------------------------------------------------

RoomTemplate419b70::RoomTemplate419b70(int id, int back, int left, int ahead, int right) :
		RoomTemplate419(id, kDesc419b70) {
	setExitId(0, back);
	setExitId(1, left);
	setHotspot(0, back, 4);
	setExitId(2, ahead);
	setHotspot(1, left, 1);
	setExitId(3, right);
	setHotspot(2, ahead, 2);
	setHotspot(3, right, 3);
}

RoomTemplate419c00::RoomTemplate419c00(int id, int back, int left, int ahead, int right) :
		RoomTemplate419(id, kDesc419c00) {
	setExitId(0, back);
	setExitId(1, left);
	setHotspot(0, back, 4);
	setExitId(2, ahead);
	setHotspot(1, left, 1);
	setExitId(3, right);
	setHotspot(2, ahead, 2);
	setHotspot(3, right, 3);
}

RoomTemplate419c90::RoomTemplate419c90(int id, int back, int left, int right) :
		RoomTemplate419(id, kDesc419c90) {
	setExitId(0, back);
	setExitId(1, left);
	setHotspot(0, back, 4);
	setExitId(2, right);
	setHotspot(1, left, 1);
	setHotspot(2, right, 3);
}

RoomTemplate419d10::RoomTemplate419d10(int id, int back, int left) :
		RoomTemplate419(id, kDesc419d10) {
	setExitId(1, back);
	setExitId(0, left);
	setHotspot(0, back, 4);
	setHotspot(1, left, 1);
}

RoomTemplate419d70::RoomTemplate419d70(int id, int back, int right) :
		RoomTemplate419(id, kDesc419d70) {
	setExitId(1, back);
	setExitId(0, right);
	setHotspot(0, back, 4);
	setHotspot(1, right, 3);
}

// ---------------------------------------------------------------------------
// RoomTemplate419dd0: lift landing
// ---------------------------------------------------------------------------

RoomTemplate419dd0::RoomTemplate419dd0(int id, int left, int right) :
		RoomTemplate419(id, kDesc419dd0) {
	setExitId(0, left);
	setExitId(1, right);
	setHotspot(0, left, 1);
	setHotspot(1, right, 3);
}

int RoomTemplate419dd0::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: lift door shut; coming from the lift panel it closes behind the hero
	case kEvEnter:
		if (param != 0xa32) {
			game()->placeObject(&_s476d48, 0x9b, -0x1a, 0, 0, 0, -1);
			return 0;
		}
		game()->placeObject(&_s476d48, 0x9b, -0x146, 0, 0, 0, -1);
		hero().place(0x49, 0x45, 0);
		hero().show();
		game()->enqueue({
			seqSpawn(),
			seqCall(kPrimWait, { 40 }),
			seqCall(kPrimAmbient, { 0xf08 }),
			seqCall(kPrimMoveLinear, { &_s476d48, 100, 0, 1, 3, 1 }),
			seqJoin(),
			seqCall(kPrimWalk, { xy(0x49, 0x61), 0 }),
			seqWaitChildren(),
			seqCall(kPrimSound, { 0xf09 }),
			seqCall(kPrimAmbientStop),
			seqEnd()
		});
		return 0;

	// Hotspot clicked
	case kEvHotspot:
		// Hotspot 0xa32: lift button - the door opens, the hero steps in and rides to the lift panel (2610)
		if (param != 0xa32)
			return 0;
		game()->enqueue({
			seqCall(kPrimWalk, { xy(0x86, 0x59), 0xab }),
			seqCall(kPrimSoundWait, { 0xee3 }),
			seqSpawn(),
			seqCall(kPrimAmbient, { 0xf08 }),
			seqCall(kPrimMoveLinear, { &_s476d48, 150, 0, 1, -2, 1 }),
			seqJoin(),
			seqIf(kPrimIfFlag, { 0xa34 }),
			seqCall(kPrimWalk, { xy(0x34, 0x46), 0x22 }),
			seqCall(kPrimDialogue, { 0, 0xa34, 0xa34, 0, 0, 0 }), // "There is something here."
			seqEndIf(),
			seqCall(kPrimWalk, { xy(0x34, 0x46), 0x5e }),
			seqWaitChildren(),
			seqCall(kPrimSound, { 0xf09 }),
			seqCall(kPrimAmbientStop),
			seqCall(kPrimChangeRoom, { 0xa32 }),
			seqEnd()
		});
		return 0;

	default:
		return 0;
	}
}

} // End of namespace Gooka
