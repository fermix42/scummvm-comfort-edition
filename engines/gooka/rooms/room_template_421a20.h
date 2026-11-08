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
 * Shared room template FUN_00421a20: the particle-field maze (rooms 2093,
 * 2098..2106). Every maze room is a thin wrapper that passes its id and a
 * table of the six rooms its six passages (hotspots 0x83d..0x842) lead to.
 */

#ifndef GOOKA_ROOMS_ROOM_TEMPLATE_421A20_H
#define GOOKA_ROOMS_ROOM_TEMPLATE_421A20_H

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * The static objects of the template (DAT_0047ab08 ... DAT_00479ec0). The
 * original keeps them in function statics, so all maze rooms share one
 * set; here the rooms share one reference-counted instance.
 */
class MazeObjects {
public:
	/** Number of particle fields (one per passage). */
	static const int kNumFields = 6;

	/** Creates the six particle fields (FUN_00412910(100) each). */
	MazeObjects();
	/** Deletes the particle fields. */
	~MazeObjects();

	/** DAT_0047ab08, 0047a110, 0047a858, 0047ab58, 00479c40, 0047ac80 (FUN_00412910(100)). */
	ParticleSprite *fields[kNumFields];
	/** DAT_0047a0c0: item 0x32 lying in room 2102. */
	Sprite item;
	/** DAT_00479ec0: handle of the ambient sound 0xf1b. */
	int ambient;
};

/**
 * Room template FUN_00421a20 (maze of particle fields). Derived rooms only
 * pass their id and their passage table (the wrappers FUN_004219e0,
 * FUN_00422050, FUN_004220c0 ... FUN_00422450).
 */
class RoomTemplate421a20 : public Room {
public:
	/** Number of passages (hotspots 0x83d..0x842). */
	static const int kNumPassages = 6;

	/**
	 * @param id        room id (what == 0 returns it)
	 * @param passages  destination room of hotspot 0x83d + i
	 */
	RoomTemplate421a20(int id, const int passages[kNumPassages]);
	/** Releases the shared objects when the last maze room goes. */
	~RoomTemplate421a20() override;

	/** The shared static descriptor DAT_00442618. */
	const RoomDescriptor *getDescriptor() override;
	/** The template body (what >= 2). */
	int handleEvent(int event, int param) override;

protected:
	/**
	 * FUN_004218d0: walks to grid (x, y), jumps into the passage (sound
	 * 0x831, hero frames 0x95..0x9c), and changes to room `room`.
	 */
	void enterPassage(int x, int y, int room);

	/** Destination rooms of the passages (the wrapper's local table). */
	int _passages[kNumPassages];

private:
	/** The objects shared by all maze rooms. */
	static MazeObjects *_objects;
	/** Number of maze rooms alive (owners of _objects). */
	static int _refCount;
};

} // End of namespace Gooka

#endif // GOOKA_ROOMS_ROOM_TEMPLATE_421A20_H
