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
 * Room_4219e0: room 2093 (0x82d), port of the room handler FUN_004219e0, a thin
 * wrapper around the maze template FUN_00421a20 (room_template_421a20.cpp).
 *
 * The original registers the handler without a fixed id (the generated
 * stub used a placeholder); the template returns 0x82d (2093) for what == 0.
 */

#include "gooka/rooms/room_template_421a20.h"

namespace Gooka {

/** Destination rooms of the passages 0x83d..0x842 (2093, 2093, 2098, 2093, 2093, 2093). */
static const int kPassages_4219e0[RoomTemplate421a20::kNumPassages] = { 0x82d, 0x82d, 0x832, 0x82d, 0x82d, 0x82d };

/** Maze room 2093 (original handler FUN_004219e0). */
class Room_4219e0 : public RoomTemplate421a20 {
public:
	/** Passes the room id and the passage table to the template. */
	Room_4219e0();
};

Room_4219e0::Room_4219e0() : RoomTemplate421a20(0x82d, kPassages_4219e0) {
}

/** Creates the handler (registered in rooms/ported.cpp). */
Room *createRoom_4219e0() {
	return new Room_4219e0();
}

} // End of namespace Gooka
