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
 * The game world: room changes, events, room objects, exits and the
 * global fallback handler (see game.h).
 */

#include "common/config-manager.h"
#include "common/debug.h"
#include "common/ptr.h"
#include "common/textconsole.h"

#include "gooka/detection.h"
#include "gooka/effects.h"
#include "gooka/game.h"
#include "gooka/gooka.h"
#include "gooka/music.h"
#include "gooka/resource.h"
#include "gooka/scene.h"
#include "gooka/sound.h"

namespace Gooka {

/** Quarter sine wave, 256 entries 0..255 (table at 0x43a678). */
static const byte kSineTable[256] = {
	0, 2, 3, 5, 6, 8, 9, 11, 13, 14, 16, 17, 19, 20, 22, 23, 25, 27, 28, 30, 31, 33, 34, 36, 37, 39, 41, 42, 44, 45, 47, 48,
	50, 51, 53, 54, 56, 57, 59, 60, 62, 63, 65, 67, 68, 70, 71, 73, 74, 76, 77, 79, 80, 81, 83, 84, 86, 87, 89, 90, 92, 93,
	95, 96, 98, 99, 100, 102, 103, 105, 106, 108, 109, 110, 112, 113, 115, 116, 117, 119, 120, 122, 123, 124, 126, 127, 128, 130, 131, 132, 134, 135,
	136, 138, 139, 140, 142, 143, 144, 146, 147, 148, 149, 151, 152, 153, 154, 156, 157, 158, 159, 161, 162, 163, 164, 165, 167, 168, 169, 170, 171, 172,
	174, 175, 176, 177, 178, 179, 180, 181, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204,
	205, 206, 207, 208, 208, 209, 210, 211, 212, 213, 214, 215, 215, 216, 217, 218, 219, 220, 220, 221, 222, 223, 223, 224, 225, 226, 226, 227, 228, 228,
	229, 230, 231, 231, 232, 232, 233, 234, 234, 235, 236, 236, 237, 237, 238, 238, 239, 240, 240, 241, 241, 242, 242, 243, 243, 244, 244, 244, 245, 245,
	246, 246, 247, 247, 247, 248, 248, 248, 249, 249, 249, 250, 250, 250, 251, 251, 251, 252, 252, 252, 252, 252, 253, 253, 253, 253, 253, 254, 254, 254,
	254, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
};

/** An id and the line the hero says about it. */
struct IdLine {
	int id;
	int line;
};

/** Comments when looking at inventory items (table 0x43e838). */
static const IdLine kItemLookLines[] = {
	{ 1, 3500 }, { 2, 3501 }, { 2, 3501 }, { 4, 3503 }, { 5, 3504 }, { 6, 3505 }, { 7, 3506 }, { 8, 3507 },
	{ 9, 3508 }, { 10, 3509 }, { 11, 3510 }, { 12, 3511 }, { 13, 3512 }, { 14, 3513 }, { 16, 3514 }, { 17, 3515 },
	{ 18, 3516 }, { 19, 3517 }, { 20, 3518 }, { 21, 3519 }, { 22, 3520 }, { 23, 3521 }, { 24, 3522 }, { 25, 3523 },
	{ 26, 3524 }, { 27, 3525 }, { 28, 3526 }, { 29, 3527 }, { 30, 3528 }, { 31, 3529 }, { 32, 3530 }, { 33, 3531 },
	{ 34, 3532 }, { 35, 3533 }, { 36, 3534 }, { 37, 3535 }, { 38, 3536 }, { 39, 3537 }, { 40, 3538 }, { 41, 3539 },
	{ 42, 3540 }, { 43, 3541 }, { 44, 3542 }, { 45, 3543 }, { 46, 3544 }, { 47, 3545 }, { 48, 3546 }, { 49, 3547 },
	{ 50, 3548 }, { 51, 3549 }, { 52, 3550 }, { 53, 3551 }, { 54, 3552 }, { 55, 3553 }, { 58, 3554 }, { 59, 3555 },
	{ 60, 3556 }, { 61, 3557 }, { 62, 3558 }, { 63, 3559 }, { 64, 3560 }, { 65, 3561 }, { 66, 3562 }, { 67, 3563 },
	{ 68, 3564 }, { 69, 3565 }, { 70, 3566 }, { 71, 3567 }, { 72, 3568 }, { 3, 3502 }, { 0, 0 }
};

/** Generic comments for hotspots no room handles (table 0x43ea70). */
static const IdLine kHotspotLines[] = {
	{ 807, 3570 }, { 924, 3571 }, { 818, 3572 }, { 819, 3573 }, { 408, 3574 }, { 2052, 3575 }, { 2309, 3576 }, { 1335, 3577 },
	{ 2324, 3578 }, { 2323, 3579 }, { 2416, 3580 }, { 2426, 3581 }, { 2446, 3582 }, { 2619, 3583 }, { 2305, 3584 }, { 0, 0 }
};

Game::Game() : _roomTick(0), _nonBlocking(false), _skip(false), _subtitles(true), _ambientHandle(kNoSound),
		_walkTargetX(-1), _walkTargetY(-1), _walkTargetDir(-1), _room(nullptr), _desc(nullptr), _roomId(0),
		_musicId(0), _musicVolume(0x100), _random("gooka") {
}

Game::~Game() {
	_sequencer.stop();
	for (uint i = 0; i < _rooms.size(); i++)
		delete _rooms[i];
}

void Game::init() {
	createRooms(_rooms);
	_subtitles = ConfMan.getBool("subtitles");
	Sequencer::_runner = runPrimitive;
	_hero.sheet = g_engine->getImage(kHeroSheet);
	_hero.flags |= kActorScalable;
}

// FUN_0041de10 (game part): everything back to the start, rooms reset their statics (event 2).
void Game::reset() {
	hideOffscreenSubtitle();
	g_engine->_soundMan->stopAll();
	_sequencer.stop();
	_state.reset();
	_inventory.clear();
	_inventory.heldItem = 0;
	_inventory.heldSlot = -1;
	_inventory.show();
	_hero.stopTalking();
	_hero.show();
	for (uint i = 0; i < _rooms.size(); i++)
		_rooms[i]->handleEvent(kEvNewGame, 0);
	_skip = false;
	clearWalkTarget();
}

// FUN_0041df40: reset, first room, hero on the start cell.
void Game::newGame() {
	reset();
	changeRoom(kStartRoom);
	_hero.place(kStartGridX, kStartGridY, kStartFrame);
}

bool Game::flagTestAndClear(int id) {
	const bool value = flagTest(id);
	flagClear(id);
	return value;
}

Room *Game::findRoom(int roomId) const {
	for (uint i = 0; i < _rooms.size(); i++) {
		if (_rooms[i]->getId() == roomId)
			return _rooms[i];
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// Room objects
// ---------------------------------------------------------------------------

// FUN_0040ea80
void Game::placeObject(Sprite *s, int x, int y, int priority, int frame, int subPriority, int hotspotId) {
	s->x = x;
	s->y = y;
	s->sheet = &_roomSheet;
	s->priority = priority;
	s->subPriority = subPriority;
	s->setFrame(frame);
	registerHotspot(s, hotspotId);
}

// FUN_0040ead0
void Game::placeObjectIfFlag(int flagId, Sprite *s, int x, int y, int priority, int frame, int subPriority) {
	placeObject(s, x, y, priority, frame, subPriority, flagId);
	if (!flagTest(flagId))
		s->hide();
}

// FUN_0040eb20
void Game::placeObjectIfFlagNoHotspot(int flagId, Sprite *s, int x, int y, int priority, int frame, int subPriority) {
	placeObject(s, x, y, priority, frame, subPriority, -1);
	if (!flagTest(flagId))
		s->hide();
}

// FUN_0040eb70
void Game::placeObjectIfNotFlag(int flagId, Sprite *s, int x, int y, int priority, int frame, int subPriority) {
	placeObject(s, x, y, priority, frame, subPriority, -1);
	if (flagTest(flagId))
		s->hide();
}

// FUN_0040ebc0
void Game::placeObjectHidden(Sprite *s, int x, int y, int priority, int frame, int subPriority) {
	placeObject(s, x, y, priority, frame, subPriority, -1);
	s->hide();
}

// FUN_0040ec00
void Game::placeShimmer(ShimmerSprite *s, int x, int y, int priority, int w, int h, int sheetFrame, int amplitude, int direction) {
	s->x = x;
	s->y = y;
	s->width = w;
	s->priority = priority;
	s->subPriority = 0;
	s->height = h;
	s->sheet = &_roomSheet;
	s->direction = direction;
	s->sheetFrame = sheetFrame;
	s->phase = 0;
	s->amplitude = amplitude;
	s->init();
	registerHotspot(s, -1);
}

// FUN_0040e8a0: the list is cleared when the room is left.
void Game::registerHotspot(Sprite *s, int hotspotId) {
	DynamicHotspot h;
	h.id = hotspotId;
	h.sprite = s;
	_dynHotspots.push_back(h);
	s->show();
}

// FUN_0040e9b0
void Game::registerHotspotIfFlag(int flagId, Sprite *s) {
	registerHotspot(s, flagId);
	if (!flagTest(flagId))
		s->hide();
}

// FUN_0040e9e0
void Game::setRectHotspot(Sprite *s, int x, int y, int priority, int subPriority, int hotspotId) {
	s->x = x;
	s->priority = priority;
	s->y = y;
	s->subPriority = subPriority;
	registerHotspot(s, hotspotId);
}

// FUN_0040ea10
Sprite *Game::dynHotspotSprite(int hotspotId) const {
	for (uint i = 0; i < _dynHotspots.size(); i++) {
		if (_dynHotspots[i].id == hotspotId)
			return _dynHotspots[i].sprite;
	}
	return nullptr;
}

// FUN_0040ef20
void Game::hideExitSprite(int id) {
	if (!_desc || !_desc->exits)
		return;
	for (int i = 0; _desc->exits[i].id() != 0 && i < kMaxExitSprites; i++) {
		if (_desc->exits[i].id() == id)
			_exitSprites[i].hide();
	}
}

// FUN_0040ef60
void Game::showExitSprite(int id) {
	if (!_desc || !_desc->exits)
		return;
	for (int i = 0; _desc->exits[i].id() != 0 && i < kMaxExitSprites; i++) {
		if (_desc->exits[i].id() == id)
			_exitSprites[i].show();
	}
}

// ---------------------------------------------------------------------------
// Sequences
// ---------------------------------------------------------------------------

// FUN_0040f2e0
void Game::enqueue(const SeqProgram &program) {
	clearWalkTarget();
	_nonBlocking = false;
	_hero.stopWalking();
	_sequencer.load(program);
}

// FUN_0040f310
void Game::enqueueNonBlocking(const SeqProgram &program) {
	clearWalkTarget();
	_nonBlocking = true;
	_hero.stopWalking();
	_sequencer.load(program);
}

// FUN_0040f340
bool Game::sequenceBusy() const {
	return _sequencer.isRunning() && !_nonBlocking;
}

// FUN_0040f7e0: turn to the front first unless already standing in a front-like frame.
void Game::showOffscreenSubtitle(int line) {
	hideOffscreenSubtitle();
	if (!_subtitles)
		return;
	_offscreenSubtitle.setText(g_engine->_res->getText(line), kFontBold, 0x000000, 0xFFFFFF, kTextBoxed);
	const Common::Point size = _offscreenSubtitle.getSize();
	_offscreenSubtitle.x = MAX(0, (kScreenWidth - size.x) / 2);
	_offscreenSubtitle.y = 16;
	_offscreenSubtitle.priority = 1000;
	_offscreenSubtitle.show();
}

void Game::hideOffscreenSubtitle() {
	_offscreenSubtitle.hide();
	_offscreenSubtitle.clearText();
}

void Game::sayIdle(int line) {
	const int f = _hero.frame;
	if (f != 0 && f != 0x22 && f != 0x24 && f != 0x27 && f != 0x2a && _hero.isVisible()) {
		enqueue({
			seqCall(kPrimFace, { 0 }),
			seqCall(kPrimDialogue, { 0, line, line, 0, 0, 0 }),
			seqEnd()
		});
	} else {
		enqueue({
			seqCall(kPrimDialogue, { 0, line, line, 0, 0, 0 }),
			seqEnd()
		});
	}
}

// ---------------------------------------------------------------------------
// World
// ---------------------------------------------------------------------------

// FUN_0040ecb0 + FUN_0040efb0
void Game::changeRoom(int roomId, int fromRoom) {
	hideOffscreenSubtitle();
	const int previousRoom = fromRoom;
	_skip = false;
	leaveRoom();

	Room *room = findRoom(roomId);
	if (!room) {
		warning("Gooka: unknown room %d", roomId);
		return;
	}
	_roomId = roomId;
	_desc = room->getDescriptor();
	debugC(1, kDebugScript, "Entering room %d from %d", roomId, previousRoom);

	g_engine->_scene->setBackground(g_engine->getImage(_desc->roomId));
	_roomTick = 0;
	_room = room;

	Common::ScopedPtr<Common::SeekableReadStream> zone(g_engine->_res->get(kResZone, _desc->roomId));
	if (zone)
		_walk.load(zone.get(), _desc->zones, _desc->numZones);
	else
		_walk.clear();

	if (_desc->sheetId) {
		Common::ScopedPtr<Common::SeekableReadStream> sheet(g_engine->_res->get(kResImage, _desc->sheetId));
		if (!sheet || !_roomSheet.load(sheet.get()))
			warning("Gooka: cannot load room sheet %d", _desc->sheetId);
	}

	clearWalkTarget();
	setupExits(previousRoom);

	if (_desc->musicId)
		setRoomMusic(_desc->musicId);
	setMusicVolume(0x100);
	raiseEvent(kEvEnter, previousRoom);
}

// FUN_0040ecb0: the room gets its leave event while it is still current.
void Game::leaveRoom() {
	raiseEvent(kEvLeave, 0);
	_room = nullptr;
	for (uint i = 0; i < _dynHotspots.size(); i++)
		_dynHotspots[i].sprite->hide();
	_dynHotspots.clear();
}

// Door/object sprites of the exit records; the record leading back to the previous
// room places the hero and plays the door animation (loop in FUN_0040efb0).
void Game::setupExits(int previousRoom) {
	if (!_desc->exits)
		return;

	for (int i = 0; _desc->exits[i].id() != 0 && i < kMaxExitSprites; i++) {
		const ExitRecord &r = _desc->exits[i];
		Sprite *spr = &_exitSprites[i];

		if (r.f[7] != 0 || r.f[8] != 0) {
			placeObjectIfFlag(r.id(), spr, r.f[7], r.f[8], r.f[9], r.f[10], r.f[15]);
			if (r.f[12] < 0x300 && !flagTest(r.id()))
				spr->hide();
		}

		if (r.id() != previousRoom)
			continue;

		_hero.setGridPos(r.f[4], r.f[5]);
		_hero.setFrame(r.f[6]);
		if (r.f[7] == 0 && r.f[8] == 0) {
			enqueue({
				seqCall(kPrimWalk, { xy(r.f[1], r.f[2]), 0 }),
				seqEnd()
			});
		} else {
			// Door opened on arrival: close it while the hero walks in
			spr->setFrame(r.f[12]);
			enqueue({
				seqSpawn(),
				seqCall(kPrimSound, { r.f[11] }),
				seqCall(kPrimAnimate, { spr, r.f[10], 4 }),
				seqJoin(),
				seqCall(kPrimWalk, { xy(r.f[1], r.f[2]), 0 }),
				seqWaitChildren(),
				seqEnd()
			});
		}
	}
}

// FUN_0040e870
int Game::raiseEvent(int event, int param) {
	int result = 0;
	if (_room)
		result = _room->handleEvent(event, param);
	if (result == 0)
		result = fallbackEvent(event, param);
	return result;
}

// FUN_0040ed40: event 7 first; unhandled pick-up objects and exits get the generic sequences.
void Game::actOnHotspot(int hotspotId, int param) {
	if (raiseEvent(kEvHotspot, hotspotId) != 0 || !_desc || !_desc->exits)
		return;

	for (int i = 0; _desc->exits[i].id() != 0 && i < kMaxExitSprites; i++) {
		const ExitRecord &r = _desc->exits[i];
		if (r.id() != hotspotId)
			continue;

		if (hotspotId < 0x301) {
			// Pick-up: walk there, comment, reach, sound, take, walk back (FUN_0040ed40)
			int wait = r.f[14];
			if (wait == -1)
				wait = 0;
			else if (wait == 0)
				wait = 10;
			enqueue({
				seqCall(kPrimWalk, { xy(r.f[1], r.f[2]), r.f[3] }),
				seqCall(kPrimDialogue, { 0, r.f[11], r.f[11], 0, 0, 0 }),
				seqCall(kPrimWalk, { xy(r.f[4], r.f[5]), r.f[6] }),
				seqCall(kPrimSound, { r.f[13] }),
				seqCall(kPrimWait, { wait }),
				seqCall(kPrimAddItem, { hotspotId }),
				seqCall(kPrimHide, { &_exitSprites[i] }),
				seqCall(kPrimWalk, { xy(r.f[1], r.f[2]), r.f[12] }),
				seqEnd()
			});
			flagClear(hotspotId);
		} else if (r.f[7] == 0 && r.f[8] == 0) {
			walkToExit(r.f[1], r.f[2], r.f[3], r.f[4], r.f[5], hotspotId, param);
		} else {
			// Door with a sprite: open it, then leave (FUN_0040f440)
			flagSet(hotspotId, true);
			const SeqProgram program = {
				seqCall(kPrimWalk, { xy(r.f[1], r.f[2]), r.f[3] }),
				seqCall(kPrimClearBusy),
				seqCall(kPrimSound, { r.f[13] }),
				seqCall(kPrimWait, { r.f[14] }),
				seqCall(kPrimAnimate, { &_exitSprites[i], r.f[12], 4 }),
				seqCall(kPrimWalk, { xy(r.f[4], r.f[5]), -1 }),
				seqCall(kPrimChangeRoom, { hotspotId }),
				seqEnd()
			};
			if (param)
				enqueueNonBlocking(program);
			else
				enqueue(program);
		}
	}
}

// FUN_0040f380
void Game::walkToExit(int x, int y, int dir, int x2, int y2, int room, int param) {
	const SeqProgram program = {
		seqCall(kPrimWalk, { xy(x, y), dir }),
		seqCall(kPrimClearBusy),
		seqCall(kPrimWalk, { xy(x2, y2), -1 }),
		seqCall(kPrimChangeRoom, { room }),
		seqEnd()
	};
	if (param)
		enqueueNonBlocking(program);
	else
		enqueue(program);
}

// FUN_0040f880
void Game::setWalkTarget(int x, int y, int dir) {
	_walkTargetX = x;
	_walkTargetY = y;
	_walkTargetDir = dir;
}

// FUN_0040f8a0
void Game::clearWalkTarget() {
	_walkTargetX = _walkTargetY = _walkTargetDir = -1;
}

// FUN_0040f8c0: the room gets its tick event after the sequencer ran.
void Game::sequencerTick() {
	if (_walkTargetX != -1 || _walkTargetY != -1 || _walkTargetDir != -1) {
		if (_hero.walkTo(_walkTargetX, _walkTargetY, _walkTargetDir) == 0)
			clearWalkTarget();
	}

	if (_sequencer.isRunning())
		_sequencer.run();
	raiseEvent(kEvTick, _roomTick++);
}

// FUN_0040e830
void Game::setRoomMusic(int musicId) {
	if (musicId == 0 || musicId == _musicId)
		return;
	_musicId = musicId;
	if (musicId == -1)
		g_engine->_music->stop();
	else
		g_engine->_music->play(musicId, true);
}

// FUN_004102a0: generic comments, the item combination table and item close-ups.
int Game::fallbackEvent(int event, int param) {
	switch (event) {
	case kEvHotspot:
		if (!sequenceBusy()) {
			for (int i = 0; kHotspotLines[i].id; i++) {
				if (kHotspotLines[i].id == param) {
					sayIdle(kHotspotLines[i].line);
					break;
				}
			}
		}
		return 0;

	case kEvItemOnHotspot:
		if (param == 0x210198)
			return 0x80f;
		return 0;

	case kEvItemOnItem:
		switch (param) {
		case 0x1001a:
		case 0x1a0001:
			return 0x6cc;
		case 0x70011:
		case 0x110007:
			return 0xd4d;
		case 0xd0025:
		case 0x25000d:
			return flagTest(0x2a) ? 0 : 0x364;
		case 0x120024:
		case 0x240012:
			return 0x807;
		case 0x130015:
		case 0x150013:
			return 0xd4e;
		case 0x160023:
		case 0x230016:
			return 0x806;
		case 0x1a001c:
		case 0x1c001a:
			return 0xd50;
		case 0x1a0029:
		case 0x29001a:
			return 0x7f6;
		case 0x1d001f:
		case 0x1f001d:
			return 0xd51;
		case 0x25002b:
		case 0x2b0025:
			return 0x7d3;
		case 0x410033:
			return 0xd52;
		case 0x420045:
			return 0xd53;
		case 0x430046:
			return 0xd54;
		case 0x440047:
			return 0xd55;
		default:
			return 0;
		}

	case kEvAction:
		switch (param) {
		case 0x6cc:
			dropHeldItem();
			replaceItem(1, 2, true);
			break;
		case 0x364:
			enqueue({
				seqCall(kPrimDialogue, { 0, 869, 869, 0, 0, 0 }),
				seqCall(kPrimDropHeld),
				seqCall(kPrimRemoveItem, { 42 }),
				seqCall(kPrimRemoveItem, { 37 }),
				seqCall(kPrimReplaceItem, { 13, 41 }),
				seqCall(kPrimWait, { 40 }),
				seqCall(kPrimDialogue, { 0, 870, 870, 0, 0, 0 }),
				seqEnd()
			});
			break;
		case 0x7f6:
			enqueue({
				seqCall(kPrimDropHeld),
				seqCall(kPrimAddItem, { 42 }),
				seqCall(kPrimWait, { 50 }),
				seqCall(kPrimAddItem, { 13 }),
				seqCall(kPrimWait, { 50 }),
				seqCall(kPrimReplaceItem, { 41, 37 }),
				seqCall(kPrimDialogue, { 0, 2039, 2039, 0, 0, 0 }),
				seqEnd()
			});
			break;
		case 0x7d3:
			dropHeldItem();
			removeItem(0x25);
			replaceItem(0x2b, 0x2d, true);
			break;
		case 0x80f: {
			Sprite *npc = dynHotspotSprite(0x198);
			if (npc) {
				enqueue({
					seqCall(kPrimDialogue, { npc, 2074, 2076, 0, 0, 2 }),
					seqCall(kPrimDropHeld),
					seqCall(kPrimRemoveItem, { 33 }),
					seqEnd()
				});
			}
			break;
		}
		case 0x806:
			enqueue({
				seqCall(kPrimDropHeld),
				seqCall(kPrimSoundWait, { 3813 }),
				seqCall(kPrimReplaceItem, { 35, 36 }),
				seqCall(kPrimDialogue, { 0, 2056, 2056, 0, 0, 0 }),
				seqEnd()
			});
			break;
		case 0x807:
			dropHeldItem();
			replaceItem(0x12, 0x22, true);
			break;
		case 0xd4d:
			enqueue({
				seqCall(kPrimDropHeld),
				seqCall(kPrimSoundWait, { 3814 }),
				seqCall(kPrimReplaceItem, { 7, 19 }),
				seqEnd()
			});
			break;
		case 0xd4e:
			dropHeldItem();
			removeItem(0x15);
			enqueue({
				seqCall(kPrimReplaceItem, { 19, 20 }),
				seqCall(kPrimDialogue, { 0, 3407, 3407, 0, 0, 0 }),
				seqEnd()
			});
			break;
		case 0xd50:
			dropHeldItem();
			removeItem(0x1c);
			replaceItem(0x1a, 0x1e, true);
			break;
		case 0xd51:
			dropHeldItem();
			removeItem(0x1d);
			replaceItem(0x1f, 0x1a, false);
			addItem(0x1b);
			break;
		case 0xd52:
		case 0xd53:
		case 0xd54:
		case 0xd55: {
			static const int kFrom[] = { 0x33, 0x45, 0x46, 0x47 };
			static const int kTo[] = { 0x45, 0x46, 0x47, 0x48 };
			consumeHeldItem();
			replaceItem(kFrom[param - 0xd52], kTo[param - 0xd52], true);
			playSound(nullptr, 0xef4);
			break;
		}
		default:
			break;
		}
		return 0;

	case kEvLookItem:
		if (param == 0xf) {
			if (raiseEvent(kEvBeforeItemRoom, 0) == 0)
				changeRoom(0xc81);
		} else if (param == 0x31) {
			if (raiseEvent(kEvBeforeItemRoom, 0) == 0)
				changeRoom(0xc9a);
		} else if (param == 0x39) {
			enqueue({
				seqCall(kPrimSetFrame, { &_hero, 206 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimSetFrame, { &_hero, 207 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimSay, { &_hero, 3414, 207, 207, 2 }),
				seqCall(kPrimSay, { &_hero, 3415, 207, 207, 2 }),
				seqCall(kPrimSay, { &_hero, 3416, 207, 207, 2 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimSetFrame, { &_hero, 207 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimSetFrame, { &_hero, 206 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimSetFrame, { &_hero, 0 }),
				seqIf(kPrimIfFlag, { 3417 }),
				seqCall(kPrimDialogue, { 0, 3417, 3417, 0, 0, 0 }),
				seqEndIf(),
				seqEnd()
			});
		} else {
			for (int i = 0; kItemLookLines[i].id; i++) {
				if (kItemLookLines[i].id == param) {
					sayIdle(kItemLookLines[i].line);
					break;
				}
			}
		}
		return 0;

	default:
		return 0;
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// FUN_00407e70: MSVC rand() is 15 bits, the result is rand() * n >> 15.
int Game::rnd(int n) {
	return (int)(((int64)_random.getRandomNumber(0x7FFF) * n) >> 15);
}

// FUN_00407e30: bit 8 mirrors the quarter wave, bit 9 negates.
int Game::sinScaled(int amplitude, int phase) {
	const uint index = (phase & 0x100) ? 0xFF - (phase & 0xFF) : (phase & 0xFF);
	int v = (kSineTable[index] * amplitude + 0x80) >> 8;
	if (phase & 0x200)
		v = -v;
	return v;
}

bool Game::playSound(int *handle, int wavId, int priority, bool loop, int volume, int pan) {
	return g_engine->_soundMan->play(handle, wavId, priority, loop, volume, pan);
}

void Game::stopSound(int *handle) {
	g_engine->_soundMan->stop(handle);
}

bool Game::isSoundPlaying(const int *handle) const {
	return g_engine->_soundMan->isPlaying(handle);
}

// FUN_0041be50: game volume 0..0x100 scaled by the ScummVM music volume (0..256).
void Game::setMusicVolume(int volume) {
	_musicVolume = volume;
	const int configured = ConfMan.getInt("music_volume");
	g_engine->_music->setVolume(CLIP(volume * configured / 0x100, 0, 255));
}

} // End of namespace Gooka
