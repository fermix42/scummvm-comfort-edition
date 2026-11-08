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
 * Savegames: the content of the original GAME.SAVE (FUN_0041c850, format
 * version 5) as a versioned ScummVM stream. The ScummVM header with the
 * description and thumbnail is appended by Engine::saveGameState().
 *
 * Version 1 layout (little-endian int32 unless noted):
 *   'GOOK' (BE), version (u32)
 *   room, music, hero frame, hero visible, hero scale, hero x, hero y,
 *   hero priority, hero sub-priority, hero posX, hero posY (fixed point),
 *   walk target x, y, dir
 *   GameState::sync (flags, variables, random table)
 *   Inventory::sync (scroll offset, 50 slots)
 */

#include "common/ptr.h"
#include "common/serializer.h"
#include "common/stream.h"

#include "gooka/game.h"
#include "gooka/gooka.h"
#include "gooka/music.h"
#include "gooka/sound.h"

namespace Gooka {

/** Savegame identification. */
enum {
	kSaveMagic = MKTAG('G', 'O', 'O', 'K'),
	kSaveVersion = 1
};

/** Room and hero part of a save (DAT_004781d4 and the fields FUN_0041c850 writes after it). */
struct SavedPlace {
	int32 room;           ///< DAT_004781d4
	int32 music;          ///< DAT_00449620 (Music::getCurrent(), -1 = none)
	int32 heroFrame;      ///< DAT_00478df8
	int32 heroVisible;    ///< DAT_0047789c
	int32 heroScale;      ///< DAT_004786cc
	int32 heroX, heroY;   ///< DAT_0047867c / DAT_00478680
	int32 heroPriority;   ///< DAT_00478684
	int32 heroSubPriority; ///< DAT_0047871c
	int32 heroPosX, heroPosY; ///< port: fixed-point position, so a restored walk starts at the right cell
	int32 walkX, walkY, walkDir; ///< DAT_00449614 / 18 / 10
};

/** Reads or writes the room and hero part. */
static void syncPlace(Common::Serializer &s, SavedPlace &p) {
	s.syncAsSint32LE(p.room);
	s.syncAsSint32LE(p.music);
	s.syncAsSint32LE(p.heroFrame);
	s.syncAsSint32LE(p.heroVisible);
	s.syncAsSint32LE(p.heroScale);
	s.syncAsSint32LE(p.heroX);
	s.syncAsSint32LE(p.heroY);
	s.syncAsSint32LE(p.heroPriority);
	s.syncAsSint32LE(p.heroSubPriority);
	s.syncAsSint32LE(p.heroPosX);
	s.syncAsSint32LE(p.heroPosY);
	s.syncAsSint32LE(p.walkX);
	s.syncAsSint32LE(p.walkY);
	s.syncAsSint32LE(p.walkDir);
}

// FUN_0041c850 (writing). The original also left and re-entered the room
// around the write (FUN_0041cd20); the port saves without disturbing the game.
Common::Error GookaEngine::saveGameStream(Common::WriteStream *stream, bool isAutosave) {
	const Actor &hero = _game->_hero;
	SavedPlace p;
	p.room = _game->currentRoomId();
	p.music = _music->getCurrent();
	p.heroFrame = hero.frame;
	p.heroVisible = hero.isVisible() ? 1 : 0;
	p.heroScale = hero.scale;
	p.heroX = hero.x;
	p.heroY = hero.y;
	p.heroPriority = hero.priority;
	p.heroSubPriority = hero.subPriority;
	p.heroPosX = hero.posX;
	p.heroPosY = hero.posY;
	p.walkX = _game->_walkTargetX;
	p.walkY = _game->_walkTargetY;
	p.walkDir = _game->_walkTargetDir;

	Common::Serializer s(nullptr, stream);
	uint32 magic = kSaveMagic;
	s.syncAsUint32BE(magic);
	s.syncVersion(kSaveVersion);
	syncPlace(s, p);
	_game->_state.sync(s);
	_game->_inventory.sync(s);

	return stream->err() ? Common::kWritingFailed : Common::kNoError;
}

// FUN_0041cb00: everything is read first, so a broken file leaves the game
// untouched. Then the new-game reset (FUN_0041df70 + FUN_0041de10), the
// state, the hero, the room (FUN_0040efb0), walk target and music.
Common::Error GookaEngine::loadGameStream(Common::SeekableReadStream *stream) {
	Common::Serializer s(stream, nullptr);
	uint32 magic = 0;
	s.syncAsUint32BE(magic);
	if (magic != kSaveMagic)
		return Common::Error(Common::kReadingFailed, "Not a Gooka savegame");
	if (!s.syncVersion(kSaveVersion))
		return Common::Error(Common::kReadingFailed, "The savegame is from a newer version");

	SavedPlace p;
	syncPlace(s, p);
	Common::ScopedPtr<GameState> state(new GameState());
	state->sync(s);
	Common::ScopedPtr<Inventory> inventory(new Inventory());
	inventory->sync(s);
	if (stream->err() || stream->eos())
		return Common::kReadingFailed;
	if (!_game->findRoom(p.room))
		return Common::Error(Common::kReadingFailed, "The savegame refers to an unknown room");

	// Reset like a new game: event 2 to all rooms, fresh UI
	_ui->reset();
	_game->reset();
	_soundMan->stopEverything();

	_game->_state = *state;
	Inventory &inv = _game->_inventory;
	for (int i = 0; i < Inventory::kNumSlots; i++)
		inv.slots[i] = inventory->slots[i];
	inv.scrollOffset = inventory->scrollOffset;
	inv.heldItem = 0;
	inv.heldSlot = -1;

	Actor &hero = _game->_hero;
	hero.stopWalking();
	hero.setScale(p.heroScale);
	hero.y = p.heroY;
	hero.priority = p.heroPriority;
	hero.x = p.heroX;
	hero.subPriority = p.heroSubPriority;
	hero.setFrame(p.heroFrame);
	hero.posX = hero.targetX = p.heroPosX;
	hero.posY = hero.targetY = p.heroPosY;
	hero.waypointX = p.heroPosX >> 10;
	hero.waypointY = p.heroPosY >> 10;
	hero.walkState = 0;
	if (p.heroVisible)
		hero.show();
	else
		hero.hide();

	// Entered from room -1 like the original, so no exit record places the hero
	_game->changeRoom(p.room, -1);
	_game->setWalkTarget(p.walkX, p.walkY, p.walkDir);
	_game->setRoomMusic(p.music);

	if (_state != kStatePlaying)
		setState(kStatePlaying);
	return Common::kNoError;
}

// F12 and the menu of the original saved at any time; the port refuses while
// a blocking sequence runs, so that autosaves never catch half an action.
bool GookaEngine::canSaveGameStateCurrently(Common::U32String *msg) {
	return _state == kStatePlaying && _game && _ui && !_ui->isMenuOpen() && !_game->sequenceBusy();
}

// Loading replaces whatever runs, as the original's title screen and menu did.
bool GookaEngine::canLoadGameStateCurrently(Common::U32String *msg) {
	return _game && _ui && (_state == kStatePlaying || _state == kStateTitle);
}

} // End of namespace Gooka
