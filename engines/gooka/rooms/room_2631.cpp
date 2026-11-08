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
 * Room 2631: a T-junction of the spaceship maze, background 2605 (port of
 * the room handler FUN_0041b190, a thin wrapper around the shared template
 * FUN_00419c90).
 */

#include "gooka/rooms/room_template_419.h"

namespace Gooka {

/**
 * Room 2631 (original handler FUN_0041b190): template FUN_00419c90 with the
 * exits back = 2630, left = 2626, right = 2629.
 */
class Room2631 : public RoomTemplate419c90 {
public:
	/** FUN_0041b190 = FUN_00419c90(2631, what, arg, 2630, 2626, 2629). */
	Room2631() : RoomTemplate419c90(2631, 2630, 2626, 2629) {}
};

/** Factory used by the room registry. */
Room *createRoom2631() {
	return new Room2631();
}

} // End of namespace Gooka
