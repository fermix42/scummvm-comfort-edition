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
 * Room 2644: a corridor bending left of the spaceship maze, background 2606
 * (port of the room handler FUN_0041a950, a thin wrapper around the shared
 * template FUN_00419d10).
 */

#include "gooka/rooms/room_template_419.h"

namespace Gooka {

/**
 * Room 2644 (original handler FUN_0041a950): template FUN_00419d10 with the
 * exits back = 2633, left = 2643.
 */
class Room2644 : public RoomTemplate419d10 {
public:
	/** FUN_0041a950 = FUN_00419d10(2644, what, arg, 2633, 2643). */
	Room2644() : RoomTemplate419d10(2644, 2633, 2643) {}
};

/** Factory used by the room registry. */
Room *createRoom2644() {
	return new Room2644();
}

} // End of namespace Gooka
