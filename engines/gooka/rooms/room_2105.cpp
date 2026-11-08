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
 * Room2105: room 2105 (0x839), port of the room handler FUN_004223d0, a thin
 * wrapper around the maze template FUN_00421a20 (room_template_421a20.cpp).
 */

#include "gooka/rooms/room_template_421a20.h"

namespace Gooka {

/** Destination rooms of the passages 0x83d..0x842 (2100, 2093, 2104, 2106, 2099, 2100). */
static const int kPassages2105[RoomTemplate421a20::kNumPassages] = { 0x834, 0x82d, 0x838, 0x83a, 0x833, 0x834 };

/** Maze room 2105 (original handler FUN_004223d0). */
class Room2105 : public RoomTemplate421a20 {
public:
	/** Passes the room id and the passage table to the template. */
	Room2105();
};

Room2105::Room2105() : RoomTemplate421a20(0x839, kPassages2105) {
}

/** Creates the handler (registered in rooms/ported.cpp). */
Room *createRoom2105() {
	return new Room2105();
}

} // End of namespace Gooka
