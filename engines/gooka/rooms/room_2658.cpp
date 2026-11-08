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
 * Room 2658: a T-junction of the spaceship maze, background 2605 (port of
 * the room handler FUN_0041add0, a thin wrapper around the shared template
 * FUN_00419c90).
 */

#include "gooka/rooms/room_template_419.h"

namespace Gooka {

/**
 * Room 2658 (original handler FUN_0041add0): template FUN_00419c90 with the
 * exits back = 2654, left = 2635, right = 2656.
 */
class Room2658 : public RoomTemplate419c90 {
public:
	/** FUN_0041add0 = FUN_00419c90(2658, what, arg, 2654, 2635, 2656). */
	Room2658() : RoomTemplate419c90(2658, 2654, 2635, 2656) {}
};

/** Factory used by the room registry. */
Room *createRoom2658() {
	return new Room2658();
}

} // End of namespace Gooka
