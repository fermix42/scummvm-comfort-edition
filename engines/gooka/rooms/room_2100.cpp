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
 * Room2100: room 2100 (0x834), port of the room handler FUN_00422140, a thin
 * wrapper around the maze template FUN_00421a20 (room_template_421a20.cpp).
 */

#include "gooka/rooms/room_template_421a20.h"

namespace Gooka {

/** Destination rooms of the passages 0x83d..0x842 (2099, 2101, 2099, 2098, 2100, 2098). */
static const int kPassages2100[RoomTemplate421a20::kNumPassages] = { 0x833, 0x835, 0x833, 0x832, 0x834, 0x832 };

/** Maze room 2100 (original handler FUN_00422140). */
class Room2100 : public RoomTemplate421a20 {
public:
	/** Passes the room id and the passage table to the template. */
	Room2100();
};

Room2100::Room2100() : RoomTemplate421a20(0x834, kPassages2100) {
}

/** Creates the handler (registered in rooms/ported.cpp). */
Room *createRoom2100() {
	return new Room2100();
}

} // End of namespace Gooka
