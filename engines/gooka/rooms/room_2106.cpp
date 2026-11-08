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
 * Room2106: room 2106 (0x83a), port of the room handler FUN_00422450, a thin
 * wrapper around the maze template FUN_00421a20 (room_template_421a20.cpp).
 */

#include "gooka/rooms/room_template_421a20.h"

namespace Gooka {

/** Destination rooms of the passages 0x83d..0x842 (2116, 2098, 2099, 2101, 2100, 2100). */
static const int kPassages2106[RoomTemplate421a20::kNumPassages] = { 0x844, 0x832, 0x833, 0x835, 0x834, 0x834 };

/** Maze room 2106 (original handler FUN_00422450). */
class Room2106 : public RoomTemplate421a20 {
public:
	/** Passes the room id and the passage table to the template. */
	Room2106();
};

Room2106::Room2106() : RoomTemplate421a20(0x83a, kPassages2106) {
}

/** Creates the handler (registered in rooms/ported.cpp). */
Room *createRoom2106() {
	return new Room2106();
}

} // End of namespace Gooka
