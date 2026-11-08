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
 * Room 2657: a T-junction of the spaceship maze, background 2605 (port of
 * the room handler FUN_0041ad70, a thin wrapper around the shared template
 * FUN_00419c90).
 */

#include "gooka/rooms/room_template_419.h"

namespace Gooka {

/**
 * Room 2657 (original handler FUN_0041ad70): template FUN_00419c90 with the
 * exits back = 2655, left = 2653, right = 2635.
 */
class Room2657 : public RoomTemplate419c90 {
public:
	/** FUN_0041ad70 = FUN_00419c90(2657, what, arg, 2655, 2653, 2635). */
	Room2657() : RoomTemplate419c90(2657, 2655, 2653, 2635) {}
};

/** Factory used by the room registry. */
Room *createRoom2657() {
	return new Room2657();
}

} // End of namespace Gooka
