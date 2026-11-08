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
 * Room 2655: a corridor bending left of the spaceship maze, background 2606
 * (port of the room handler FUN_0041ae90, a thin wrapper around the shared
 * template FUN_00419d10).
 */

#include "gooka/rooms/room_template_419.h"

namespace Gooka {

/**
 * Room 2655 (original handler FUN_0041ae90): template FUN_00419d10 with the
 * exits back = 2653, left = 2657.
 */
class Room2655 : public RoomTemplate419d10 {
public:
	/** FUN_0041ae90 = FUN_00419d10(2655, what, arg, 2653, 2657). */
	Room2655() : RoomTemplate419d10(2655, 2653, 2657) {}
};

/** Factory used by the room registry. */
Room *createRoom2655() {
	return new Room2655();
}

} // End of namespace Gooka
