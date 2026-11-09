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
 * The game world: rooms, flags, inventory, hero, the main sequencer and
 * every engine function the room code calls. Method comments name the
 * original function so a room can be ported line by line from GAME.c.
 */

#ifndef GOOKA_GAME_H
#define GOOKA_GAME_H

#include "common/array.h"
#include "common/random.h"

#include "gooka/actor.h"
#include "gooka/image.h"
#include "gooka/inventory.h"
#include "gooka/room.h"
#include "gooka/sequencer.h"
#include "gooka/state.h"
#include "gooka/walk.h"

namespace Gooka {

class ShimmerSprite;

/* Room and position of a new game (FUN_0041df40). */
enum {
	kStartRoom = 854,
	kStartGridX = 78,
	kStartGridY = 80,
	kStartFrame = 39,
	kMaxExitSprites = 16, ///< DAT_004692b0: one sprite per exit/object record
	kHeroSheet = 406
};

/* A sprite registered as a clickable hotspot (list DAT_00449628). */
struct DynamicHotspot {
	int id;
	Sprite *sprite;
};

/*
 * Game logic hub. Everything the original kept in globals lives here;
 * the ported rooms reach it through g_engine->_game.
 */
class Game {
public:
	Game();
	~Game();

	/* Creates the rooms and the hero. */
	void init();
	/* New-game reset without entering a room: sounds, state, inventory, hero, event 2 to all rooms. */
	void reset();
	/* Starts a new game in the first room (FUN_0041df40 + event 2 broadcast FUN_0040ff20). */
	void newGame();

	/* @name State (flags keyed by object/line/room ids) */
	///@{
	bool flagTest(int id) const { return _state.flagTest(id); }          ///< FUN_0040e770
	int flagTestId(int id) const { return flagTest(id) ? id : 0; }       ///< FUN_0040e7a0
	void flagSet(int id, bool value) { _state.flagSet(id, value); }      ///< FUN_0040e7c0
	void flagClear(int id) { _state.flagSet(id, false); }                ///< FUN_0040e7f0
	bool flagTestAndClear(int id);                                       ///< FUN_0040e800
	int &var(int index) { return _state.var(index); }                    ///< FUN_0040e820
	///@}

	/* @name Inventory */
	///@{
	bool hasItem(int item) const { return _inventory.has(item); }        ///< FUN_0040ff10
	void addItem(int item) { _inventory.add(item); }                     ///< FUN_0040fed0
	void removeItem(int item) { _inventory.remove(item); }               ///< FUN_0040fee0
	void replaceItem(int oldItem, int newItem, bool sparkle) { _inventory.replace(oldItem, newItem, sparkle); } ///< FUN_0040fef0
	void dropHeldItem() { _inventory.dropHeld(); }                       ///< FUN_0041d230
	void consumeHeldItem() { _inventory.consumeHeld(); }                 ///< FUN_0041d260
	///@}

	/* @name Room objects (sprites of the room sheet) */
	///@{
	/* Position, priority, frame, sheet = room sheet, register as hotspot, show (FUN_0040ea80). */
	void placeObject(Sprite *s, int x, int y, int priority, int frame, int subPriority, int hotspotId);
	/* placeObject(), hidden when flag `flagId` is clear (FUN_0040ead0). */
	void placeObjectIfFlag(int flagId, Sprite *s, int x, int y, int priority, int frame, int subPriority);
	/* Like placeObjectIfFlag() without a hotspot (FUN_0040eb20). */
	void placeObjectIfFlagNoHotspot(int flagId, Sprite *s, int x, int y, int priority, int frame, int subPriority);
	/* No hotspot, hidden when flag `flagId` is set (FUN_0040eb70). */
	void placeObjectIfNotFlag(int flagId, Sprite *s, int x, int y, int priority, int frame, int subPriority);
	/* No hotspot, hidden until a sequence shows it (FUN_0040ebc0). */
	void placeObjectHidden(Sprite *s, int x, int y, int priority, int frame, int subPriority);
	/* Shimmer effect over a frame of the room sheet (FUN_0040ec00). */
	void placeShimmer(ShimmerSprite *s, int x, int y, int priority, int w, int h, int sheetFrame, int amplitude, int direction);
	/* Registers a sprite as dynamic hotspot (-1 = none) and shows it (FUN_0040e8a0). */
	void registerHotspot(Sprite *s, int hotspotId);
	/* registerHotspot(s, flagId), hidden when the flag is clear (FUN_0040e9b0). */
	void registerHotspotIfFlag(int flagId, Sprite *s);
	/* Position + priority without sheet binding, then registerHotspot (FUN_0040e9e0). */
	void setRectHotspot(Sprite *s, int x, int y, int priority, int subPriority, int hotspotId);
	/* Sprite registered for a dynamic hotspot id, nullptr if none (FUN_0040ea10). */
	Sprite *dynHotspotSprite(int hotspotId) const;
	/* The room sprite sheet (descriptor[4]) (FUN_0040eca0). */
	Image *roomSheet() { return &_roomSheet; }
	/* Hides the door/object sprite of an exit record (FUN_0040ef20). */
	void hideExitSprite(int id);
	/* Shows the door/object sprite of an exit record (FUN_0040ef60). */
	void showExitSprite(int id);
	/* Dynamic hotspots of the current room. */
	const Common::Array<DynamicHotspot> &getDynamicHotspots() const { return _dynHotspots; }
	///@}

	/* @name Sequences */
	///@{
	/* Stops the hero, blocks input and starts a sequence (FUN_0040f2e0). */
	void enqueue(const SeqProgram &program);
	/* Same without blocking input (FUN_0040f310). */
	void enqueueNonBlocking(const SeqProgram &program);
	/* True while a blocking sequence runs (FUN_0040f340). */
	bool sequenceBusy() const;
	/* Clears the "non-blocking" flag (FUN_0040f370). */
	void clearBusy() { _nonBlocking = false; }
	/* Hero faces front and says `line` (FUN_0040f7e0). */
	void sayIdle(int line);
	/* Subtitles for explicitly marked off-screen speech, independent of actors. */
	void showOffscreenSubtitle(int line);
	void hideOffscreenSubtitle();
	/* The main sequencer (DAT_00449598). */
	Sequencer &mainSequencer() { return _sequencer; }
	///@}

	/* @name World */
	///@{
	/* Leaves the current room and enters `roomId` (FUN_0040efb0). */
	void changeRoom(int roomId) { changeRoom(roomId, _roomId); }
	/*
	 * Same, but the room is entered as if coming from `fromRoom` (exit records
	 * and the enter event see it); loading uses -1 so no exit places the hero.
	 */
	void changeRoom(int roomId, int fromRoom);
	/* Leaves the current room: leave event, dynamic hotspots dropped (FUN_0040ecb0). */
	void leaveRoom();
	/* Room handler first, then the global fallback (FUN_0040e870). */
	int raiseEvent(int event, int param);
	/* Clicked hotspot: event 7, else pick-up or exit (FUN_0040ed40). */
	void actOnHotspot(int hotspotId, int param);
	/* Walk to an exit and change room (FUN_0040f380; dir -1 = FUN_0040f410). */
	void walkToExit(int x, int y, int dir, int x2, int y2, int room, int param);
	/* Walk target set by a floor click (FUN_0040f880). */
	void setWalkTarget(int x, int y, int dir);
	/* Clears the walk target (FUN_0040f8a0). */
	void clearWalkTarget();
	/* Per-tick driver: hero walk target, sequencer, room tick event (FUN_0040f8c0). */
	void sequencerTick();
	/* Global fallback for unhandled events (FUN_004102a0). */
	int fallbackEvent(int event, int param);
	/* Starts the room music (descriptor[5]; -1 stops) (FUN_0040e830). */
	void setRoomMusic(int musicId);
	///@}

	/* @name Helpers used by rooms */
	///@{
	/* Random integer in [0, n) (FUN_00407e70). */
	int rnd(int n);
	/* amplitude * sin(phase), phase 0..0x3ff per turn (FUN_00407e30). */
	static int sinScaled(int amplitude, int phase);
	/* Plays a sound (Sound_play FUN_0040a2e0). */
	bool playSound(int *handle, int wavId, int priority = 0, bool loop = false, int volume = 0, int pan = 0);
	/* Stops a sound (FUN_0040a4c0). */
	void stopSound(int *handle);
	/* True while the sound plays (FUN_0040a490). */
	bool isSoundPlaying(const int *handle) const;
	/* Music volume 0..0x100, scaled by the configured volume (FUN_0041be50 / FUN_0041be80). */
	void setMusicVolume(int volume);
	int getMusicVolume() const { return _musicVolume; }
	///@}

	/* @name Current room */
	///@{
	int currentRoomId() const { return _roomId; }
	Room *currentRoom() const { return _room; }
	const RoomDescriptor *descriptor() const { return _desc; }
	/* Room handler by id, nullptr if unknown. */
	Room *findRoom(int roomId) const;
	///@}

	/* @name Public state, accessed directly by the UI, primitives and rooms */
	///@{
	GameState _state;
	Inventory _inventory;
	Actor _hero;          ///< DAT_00471990
	WalkMap _walk;
	int _roomTick;        ///< DAT_00449590, parameter of the tick event
	bool _nonBlocking;    ///< DAT_00469298
	bool _skip;           ///< DAT_0048142c: Esc fast-forward requested
	bool _subtitles;      ///< DAT_00478890
	int _ambientHandle;   ///< DAT_00481470: shared handle of sequence sounds
	int _walkTargetX, _walkTargetY, _walkTargetDir; ///< DAT_00449614 / 18 / 10
	///@}

private:
	/* Places door/object sprites and puts the hero at the arrival exit. */
	void setupExits(int previousRoom);

	Common::Array<Room *> _rooms;
	Room *_room;
	const RoomDescriptor *_desc;
	int _roomId;
	int _musicId;          ///< DAT_00449620
	int _musicVolume;
	Image _roomSheet;      ///< DAT_004495f0
	Sprite _exitSprites[kMaxExitSprites];
	Common::Array<DynamicHotspot> _dynHotspots;
	Sequencer _sequencer;
	Common::RandomSource _random;
	TextSprite _offscreenSubtitle;
};

/* Runs the primitive of the current sequencer instruction (primitives.cpp). */
int runPrimitive(Sequencer &seq);

} // End of namespace Gooka

#endif // GOOKA_GAME_H
