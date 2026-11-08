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
 * Rooms: static room descriptors and the base class of the ported
 * room handlers (the original `room(what, arg)` functions).
 */

#ifndef GOOKA_ROOM_H
#define GOOKA_ROOM_H

#include "common/array.h"
#include "common/scummsys.h"

namespace Gooka {

/**
 * Walk zone record (0x1c bytes). Walk map cells hold a 1-based index into
 * the room's zone table in their low 7 bits.
 */
struct ZoneRecord {
	int layer;       ///< render priority of an actor standing in the zone
	int scale;       ///< actor scale at yEnd (kScaleFull = 100 %)
	int yEnd;        ///< grid row where `scale` is reached, -1 = constant scale
	int yStart;      ///< grid row where the scale ramp starts (scale 0)
	int footstepSfx; ///< footstep sound, -1 = none, 0 = default 0xd49
	int type;        ///< 0 floor; 1/2, 3/4, 7/8 stairs/ladders; 5/6 pass-through
	int pad;
};

/**
 * Exit or pick-up object record (0x48 bytes = 18 ints), terminated by a
 * record with id 0. Stored raw; the accessors name the fields as far as
 * they are known (GAMEEXE.md 6.1). id >= 0x301 is an exit to room `id`,
 * smaller ids are pick-up items.
 */
struct ExitRecord {
	int f[18];

	int id() const { return f[0]; }
	bool isExit() const { return f[0] >= 0x301; }
	int walkX() const { return f[1]; }   ///< grid x to walk to before using it
	int walkY() const { return f[2]; }
	int walkDir() const { return f[3]; } ///< -1 = keep direction
	int walkX2() const { return f[4]; }  ///< second walk point / arrival point
	int walkY2() const { return f[5]; }
	int dir2() const { return f[6]; }    ///< direction / hero frame at the second point
	int sprX() const { return f[7]; }    ///< door or object sprite position (0,0 = none)
	int sprY() const { return f[8]; }
	int sprPriority() const { return f[9]; }
	int sprFrame() const { return f[10]; }
	int sprSubPriority() const { return f[15]; }
};

/**
 * Room descriptor (11 dwords in the original), returned for `what == 1`.
 * Arrays are owned by the room (static data or members).
 */
struct RoomDescriptor {
	int roomId;               ///< [0] room = background image = walk map id
	const ZoneRecord *zones;  ///< [1] zone records, index 1-based in the walk map
	int numZones;             ///<     number of zone records
	const int *hotspots;      ///< [2] cursorType << 16 | hotspotId per slot (index = slot - 1), -1 = empty slot
	const ExitRecord *exits;  ///< [3] exit/object records, terminated by id 0 (may be nullptr)
	int sheetId;              ///< [4] sprite sheet image of the room objects, 0 = none
	int musicId;              ///< [5] music, 0 = keep, -1 = stop
	int field6;               ///< [6]
	int layerFlags;           ///< [7] (FUN_0040fe00)
	int field8;               ///< [8]
	int field9;               ///< [9]
	/**
	 * Number of entries in `hotspots` (the highest slot the hotspot map uses).
	 * 0 = not known: the table is then read up to its first -1.
	 */
	int numHotspots;
};

/** Events passed to Room::handleEvent (the original `what` values >= 2). */
enum RoomEvent {
	kEvNewGame = 2,       ///< reset room statics (sent to all rooms)
	kEvEnter = 3,         ///< param = previous room id
	kEvLeave = 4,
	kEvTick = 5,          ///< param = room tick counter
	kEvHoverName = 6,     ///< param = hotspot id, return = name string id override
	kEvHotspot = 7,       ///< hotspot clicked after walking; return != 0 = handled
	kEvFloorClick = 8,    ///< param = gridX | gridY << 16
	kEvBeforeItemRoom = 9,
	kEvLeftDown = 0xa,
	kEvClick = 0xb,
	kEvLeftUp = 0xc,
	kEvRightDown = 0xd,
	kEvRightUp = 0xe,
	kEvItemOnHotspot = 0x10, ///< param = item << 16 | hotspot, return = action id
	kEvItemOnItem = 0x11,    ///< param = item << 16 | item, return = action id
	kEvAction = 0x12,        ///< action id executed
	kEvLookItem = 0x13,      ///< param = item id
	kEvHotspotOverride = 0x14,
	kEvRoomPrivate1 = 0x17,  ///< raised by sequences for room-specific callbacks
	kEvRoomPrivate2 = 0x18
};

/**
 * Base class of the 99 ported room handlers. A room object lives for the
 * whole game (like the original statics) and owns its sprites.
 */
class Room {
public:
	explicit Room(int id) : _id(id) {}
	virtual ~Room() {}

	/** Room id (`what == 0`). */
	int getId() const { return _id; }
	/** Static descriptor (`what == 1`). */
	virtual const RoomDescriptor *getDescriptor() = 0;
	/**
	 * Handles an event (`what >= 2`, see RoomEvent). Return 0 when the room
	 * does not handle it, so that the global fallback runs.
	 */
	virtual int handleEvent(int event, int param) = 0;

protected:
	int _id;
};

/** Creates all room handlers (rooms/rooms.cpp). */
void createRooms(Common::Array<Room *> &rooms);

} // End of namespace Gooka

#endif // GOOKA_ROOM_H
