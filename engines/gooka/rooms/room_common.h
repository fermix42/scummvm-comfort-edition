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
 * Common includes and shortcuts for the ported rooms (rooms/room_*.cpp).
 */

#ifndef GOOKA_ROOMS_ROOM_COMMON_H
#define GOOKA_ROOMS_ROOM_COMMON_H

#include "common/debug.h"
#include "common/textconsole.h"

#include "gooka/actor.h"
#include "gooka/detection.h"
#include "gooka/effects.h"
#include "gooka/game.h"
#include "gooka/gooka.h"
#include "gooka/image.h"
#include "gooka/music.h"
#include "gooka/room.h"
#include "gooka/rooms/room_data.h"
#include "gooka/scene.h"
#include "gooka/sequencer.h"
#include "gooka/sound.h"
#include "gooka/text.h"

namespace Gooka {

/** The game world (the original globals). */
inline Game *game() { return g_engine->_game; }
/** The hero (DAT_00471990). */
inline Actor &hero() { return g_engine->_game->_hero; }
/** Global tick counter (DAT_00448390), used for animation phases like `ticks() % 3`. */
inline uint32 ticks() { return g_engine->_tickCounter; }

} // End of namespace Gooka

#endif // GOOKA_ROOMS_ROOM_COMMON_H
