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
 * Port of the shared room template FUN_00421a20 (particle-field maze) and
 * its sequence helper FUN_004218d0.
 */

#include "gooka/rooms/room_template_421a20.h"

namespace Gooka {

// Descriptor DAT_00442618, shared by all maze rooms: background/walk map
// 0x82d, sprite sheet 0x83c, music 0x1aa. (The generated room_data.cpp
// entries for 2099..2106 put the room id into [0]; the original uses 0x82d.)
static const ZoneRecord kMazeZones[] = {
	{ 4, 180, 99, 0, 0, 0, 0 },
	{ 3, 180, 99, 0, 0, 0, 0 },
	{ 2, 180, 99, 0, 0, 0, 0 },
};
static const int kMazeHotspots[] = { 0x1083d, 0x1083e, 0x2083f, 0x20840, 0x30841, 0x30842, -1 };
static const RoomDescriptor kMazeDescriptor = {
	0x82d, kMazeZones, 3, kMazeHotspots, nullptr, 0x83c, 0x1aa, 0, 0, 0, 0, 0
};

/** Screen position of each particle field (FUN_0040e9e0 calls of event 3). */
static const int kFieldPos[MazeObjects::kNumFields][2] = {
	{ 0x1a, 0x92 }, { 0x9b, 0xa3 }, { 0xf4, 0xa6 }, { 0x160, 0xa2 }, { 0x1bd, 0xa1 }, { 0x222, 0x95 }
};

/** Grid point in front of each passage (arguments of the FUN_004218d0 calls). */
static const int kPassagePos[RoomTemplate421a20::kNumPassages][2] = {
	{ 4, 0x5f }, { 0x22, 0x53 }, { 0x43, 0x48 }, { 0x5d, 0x48 }, { 0x7c, 0x53 }, { 0x9c, 0x5f }
};

MazeObjects *RoomTemplate421a20::_objects = nullptr;
int RoomTemplate421a20::_refCount = 0;

// The lazy constructors of the template's statics (FUN_00412910 x 6, FUN_0040ff60, FUN_0040a6f0).
MazeObjects::MazeObjects() : ambient(kNoSound) {
	for (int i = 0; i < kNumFields; i++)
		fields[i] = new ParticleSprite(100);
}

MazeObjects::~MazeObjects() {
	for (int i = 0; i < kNumFields; i++)
		delete fields[i];
}

RoomTemplate421a20::RoomTemplate421a20(int id, const int passages[kNumPassages]) : Room(id) {
	for (int i = 0; i < kNumPassages; i++)
		_passages[i] = passages[i];
	if (_refCount++ == 0)
		_objects = new MazeObjects();
}

RoomTemplate421a20::~RoomTemplate421a20() {
	if (--_refCount == 0) {
		delete _objects;
		_objects = nullptr;
	}
}

const RoomDescriptor *RoomTemplate421a20::getDescriptor() {
	return &kMazeDescriptor;
}

void RoomTemplate421a20::enterPassage(int x, int y, int room) {
	game()->enqueue({
		seqCall(kPrimWalk, { xy(x, y), -1 }),
		seqCall(kPrimSound, { 0x831 }),
		seqCall(kPrimSetFrame, { &hero(), 0x95 }),
		seqCall(kPrimShowPriority, { &hero(), (int)0xffff0000 }),
		seqCall(kPrimWait, { 2 }),
		seqCall(kPrimAnimate, { &hero(), 0x9c, 2 }),
		seqCall(kPrimHide, { &hero() }),
		seqCall(kPrimWait, { 1 }),
		seqCall(kPrimChangeRoom, { room }),
		seqCall(kPrimShow, { &hero() }),
		seqEnd()
	});
}

int RoomTemplate421a20::handleEvent(int event, int param) {
	MazeObjects &obj = *_objects;

	switch (event) {
	// Enter: ambient sound, item 0x32 in room 2102, the hero climbs out of the
	// passage when coming from the maze or room 2083, particle fields
	case kEvEnter:
		game()->playSound(&obj.ambient, 0xf1b, 0, true, -2000, 0);
		if (_id == 0x836)
			game()->placeObjectIfFlag(0x32, &obj.item, 0xa5, 0x158, 3, 0, 0);
		if ((param >= 0x82d && param <= 0x83b) || param == 0x823) {
			hero().place(0x50, 0x5f, 0x9c);
			hero().hide();
			game()->enqueue({
				seqCall(kPrimWait, { 1 }),
				seqCall(kPrimSound, { 0x830 }),
				seqCall(kPrimAnimate, { &hero(), 0x95, 2 }),
				seqCall(kPrimSetFrame, { &hero(), 0x27 }),
				seqIf(kPrimIfFlag, { 0x82e }),
				seqCall(kPrimDialogue, { 0, 0x82e, 0x82e, 0, 0, 0 }),
				seqEndIf(),
				seqEnd()
			});
		}
		for (int i = 0; i < MazeObjects::kNumFields; i++)
			game()->setRectHotspot(obj.fields[i], kFieldPos[i][0], kFieldPos[i][1], 0, 0, -1);
		for (int i = 0; i < MazeObjects::kNumFields; i++)
			obj.fields[i]->width = 0x46;
		for (int i = 0; i < MazeObjects::kNumFields; i++)
			obj.fields[i]->height = 0xfa;
		return 0;

	// Leave: stop the ambient sound
	case kEvLeave:
		game()->stopSound(&obj.ambient);
		return 0;

	// Tick: the first three particle fields move on even ticks, the others on odd ticks
	case kEvTick:
		if ((ticks() & 1) == 0) {
			obj.fields[0]->step();
			obj.fields[1]->step();
			obj.fields[2]->step();
			return 0;
		}
		obj.fields[3]->step();
		obj.fields[4]->step();
		obj.fields[5]->step();
		return 0;

	// Hotspot clicked (the original returns 0 even when it starts a sequence)
	case kEvHotspot:
		switch (param) {
		// Hotspot 0x32: pick up item 0x32 (room 2102)
		case 0x32:
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x37, 0x5f), 0x77 }),
				seqCall(kPrimDialogue, { 0, 0x82f, 0x82f, 0, 0, 0 }),
				seqCall(kPrimHide, { &obj.item }),
				seqCall(kPrimAddItem, { 0x32 }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
			return 0;
		// Hotspots 0x83d..0x842: jump into passage i and go to the room of the table
		case 0x83d:
		case 0x83e:
		case 0x83f:
		case 0x840:
		case 0x841:
		case 0x842: {
			const int i = param - 0x83d;
			enterPassage(kPassagePos[i][0], kPassagePos[i][1], _passages[i]);
			return 0;
		}
		default:
			break;
		}
		return 0;

	// Hotspot override: the original disables hotspot 0x843 when a 7th table
	// entry is 0, but the table has six entries and the 7th read hits the
	// wrapper's return address, which is never 0 - so nothing is overridden
	case kEvHotspotOverride:
		return 0;

	default:
		break;
	}
	return 0;
}

} // End of namespace Gooka
