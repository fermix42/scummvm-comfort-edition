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
 * Room 2654: a corridor bending right of the spaceship maze, background 2607
 * (port of the room handler FUN_0041aef0, a thin wrapper around the shared
 * template FUN_00419d70).
 */

#include "gooka/rooms/room_template_419.h"

namespace Gooka {

/**
 * Room 2654 (original handler FUN_0041aef0): template FUN_00419d70 with the
 * exits back = 2656, right = 2658.
 */
class Room2654 : public RoomTemplate419d70 {
public:
	/** FUN_0041aef0 = FUN_00419d70(2654, what, arg, 2656, 2658). */
	Room2654() : RoomTemplate419d70(2654, 2656, 2658) {}
};

/** Factory used by the room registry. */
Room *createRoom2654() {
	return new Room2654();
}

} // End of namespace Gooka
