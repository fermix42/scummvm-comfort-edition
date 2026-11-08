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
 * Room 2651: a corridor crossing of the spaceship maze, background 2603
 * (port of the room handler FUN_0041aa10, a thin wrapper around the shared
 * template FUN_00419b70).
 */

#include "gooka/rooms/room_template_419.h"

namespace Gooka {

/**
 * Room 2651 (original handler FUN_0041aa10): template FUN_00419b70 with the
 * exits back = 2650, left = 2647, ahead = 2645, right = 2634.
 */
class Room2651 : public RoomTemplate419b70 {
public:
	/** FUN_0041aa10 = FUN_00419b70(2651, what, arg, 2650, 2647, 2645, 2634). */
	Room2651() : RoomTemplate419b70(2651, 2650, 2647, 2645, 2634) {}
};

/** Factory used by the room registry. */
Room *createRoom2651() {
	return new Room2651();
}

} // End of namespace Gooka
