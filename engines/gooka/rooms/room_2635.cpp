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
 * Room 2635: a lift landing with the lift button of the spaceship maze,
 * background 2608 (port of the room handler FUN_0041ad10, a thin wrapper
 * around the shared template FUN_00419dd0).
 */

#include "gooka/rooms/room_template_419.h"

namespace Gooka {

/**
 * Room 2635 (original handler FUN_0041ad10): template FUN_00419dd0 with the
 * exits left = 2657, right = 2658.
 */
class Room2635 : public RoomTemplate419dd0 {
public:
	/** FUN_0041ad10 = FUN_00419dd0(2635, what, arg, 2657, 2658). */
	Room2635() : RoomTemplate419dd0(2635, 2657, 2658) {}
};

/** Factory used by the room registry. */
Room *createRoom2635() {
	return new Room2635();
}

} // End of namespace Gooka
