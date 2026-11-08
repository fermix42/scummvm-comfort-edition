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
 * Room 2647: a corridor bending right of the spaceship maze, background 2607
 * (port of the room handler FUN_0041ab90, a thin wrapper around the shared
 * template FUN_00419d70).
 */

#include "gooka/rooms/room_template_419.h"

namespace Gooka {

/**
 * Room 2647 (original handler FUN_0041ab90): template FUN_00419d70 with the
 * exits back = 2648, right = 2651.
 */
class Room2647 : public RoomTemplate419d70 {
public:
	/** FUN_0041ab90 = FUN_00419d70(2647, what, arg, 2648, 2651). */
	Room2647() : RoomTemplate419d70(2647, 2648, 2651) {}
};

/** Factory used by the room registry. */
Room *createRoom2647() {
	return new Room2647();
}

} // End of namespace Gooka
