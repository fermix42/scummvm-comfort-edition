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
 * Room2000: port of the room handler FUN_0041f5b0.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** Room 2000 (original handler FUN_0041f5b0). */
class Room2000 : public Room {
public:
	/** Room 2000; the members are the original statics. */
	Room2000();
	/** Static descriptor DAT_00442018. */
	const RoomDescriptor *getDescriptor() override;
	/** The room body (what >= 2). */
	int handleEvent(int event, int param) override;

private:
	/** Puts the NPC DAT_00478720 on the screen as hotspot 0x198 (event 3). */
	void placeNpc();

	Sprite _s47ac30; ///< item 0x2c out of reach (frames 2..16 while it is there)
	Sprite _s47aa98; ///< floating object, frame 0x11 (moves on even ticks)
	Sprite _s47a5d0; ///< floating object, frame 0x12 (moves on odd ticks)
	Sprite _s47ad78; ///< place of item 0x2e (frame 0 while it lies there, 0x15 after)
	Sprite _s47abe8; ///< frame 0x16, shown at the hero while he knocks item 0x2c down
	ShimmerSprite _e47a1a8; ///< reflection of _s47aa98 (FUN_00417080())
	ShimmerSprite _e479c88; ///< reflection of _s47a5d0 (FUN_00417080())
	int _h47a614 = kNoSound; ///< ambient sound 0xd48
	/**
	 * TODO(port): DAT_00478720 is a global NPC actor of the original (ctor
	 * FUN_0041bc40, sprite sheet = image 0x198 set by the new-game reset
	 * FUN_0041de10), used by rooms 1200, 2000, 2037, 2049 and the fallback
	 * (action 0x80f). The core has no such object yet, so the room keeps its
	 * own copy and sets it up completely on entry.
	 */
	Actor _n478720;
};

Room2000::Room2000() : Room(2000) {
}

const RoomDescriptor *Room2000::getDescriptor() {
	return findRoomData(_id);
}

void Room2000::placeNpc() {
	_n478720.sheet = g_engine->getImage(0x198);
	game()->registerHotspotIfFlag(0x198, &_n478720);
	_n478720.x = 0x82;
	_n478720.y = 0x9b;
	_n478720.priority = 0;
	_n478720.subPriority = 0x14;
	_n478720.setFrame(0);
	_n478720.setScale(0xd2);
}

int Room2000::handleEvent(int event, int param) {
	switch (event) {
	// Enter: ambient sound, objects, NPC and reflections; coming from room 3243 the NPC talks
	case kEvEnter:
		game()->playSound(&_h47a614, 0xd48, 0, true, -1500, 0);
		game()->placeObjectIfFlag(0x2c, &_s47ac30, 0xfa, 0x20, 0, 2, 0);
		game()->placeObject(&_s47aa98, 0xae, 0x78, -2, 0x11, 0, -1);
		game()->placeObject(&_s47a5d0, 0x157, 0x78, -2, 0x12, 0, -1);
		placeNpc();
		game()->placeObjectHidden(&_s47abe8, 0, 0, 0, 0x16, 0);
		game()->placeObject(&_s47ad78, 0xfa, 0x10e, 0, game()->flagTest(0x2e) ? 0 : 0x15, 0, -1);
		game()->placeShimmer(&_e47a1a8, 0x1a, 0xd2, -1, 0x80, 0x32, 0x13, 0x10, -1);
		game()->placeShimmer(&_e479c88, 0x1a, 0xd2, -1, 0x80, 0x32, 0x13, 0x10, -1);
		if (param == 0xcab) {
			hero().place(0x40, 0x5a, 0x22);
			hero().show();
			game()->enqueue({
				seqYield(),
				seqCall(kPrimDialogue, { &_n478720, 0x7d6, 0x7db, 0, 0, 2 }),
				seqCall(kPrimDialogue, { &_n478720, -0x7dc, 0x7f0, 0, 0, 2 }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
			return 0;
		}
		return 0;

	// Leave: stop the ambient sound
	case kEvLeave:
		game()->stopSound(&_h47a614);
		return 0;

	// Tick: item 0x2c swings, reflections shimmer, the floating objects and their reflections bob
	case kEvTick: {
		const int t = (int)ticks();
		int swing = (t / 4) % 0x1c;
		if (swing > 14)
			swing = 0x1c - swing;
		if (game()->flagTest(0x2c))
			_s47ac30.setFrame(swing + 2);
		if (t % 3 == 0) {
			_e47a1a8.step();
			_e479c88.step();
		}
		const int phaseY = (int)(ticks() * 11);
		const int phaseX = (int)(ticks() * 4);
		if ((t & 1) == 0) {
			_s47aa98.x = Game::sinScaled(6, phaseX) + 0x96;
			_s47aa98.y = Game::sinScaled(0xc, phaseY) + 0x82;
			_s47aa98.priority = -2;
			_s47aa98.subPriority = 0;
			_e47a1a8.x = Game::sinScaled(6, phaseX) + 0x96;
			_e47a1a8.y = Game::sinScaled(0xc, phaseY) + 0xd2;
			_e47a1a8.priority = -1;
			_e47a1a8.subPriority = 0;
			return 0;
		}
		_s47a5d0.x = Game::sinScaled(6, phaseX) + 0x14a;
		_s47a5d0.y = Game::sinScaled(0xc, phaseY) + 0x82;
		_s47a5d0.priority = -2;
		_s47a5d0.subPriority = 0;
		_e479c88.x = Game::sinScaled(6, phaseX) + 0x14a;
		_e479c88.y = Game::sinScaled(0xc, phaseY) + 0xd2;
		_e479c88.priority = -1;
		_e479c88.subPriority = 0;
		return 0;
	}

	// Hotspot clicked (the original returns 0 in every case)
	case kEvHotspot:
		switch (param) {
		// Hotspot 0x198: the NPC talks (lines 2033-2034), then off to room 3243
		case 0x198:
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x40, 0x5a), 0x22 }),
				seqCall(kPrimDialogue, { &_n478720, 0x7f1, 0x7f2, 0, 0, 2 }),
				seqCall(kPrimWait, { 0x32 }),
				seqCall(kPrimChangeRoom, { 0xcab }),
				seqCall(kPrimSound, { 0x7f3 }),
				seqEnd()
			});
			return 0;
		// Hotspot 0x2c: item 44 hangs out of reach - comment (line 2001)
		case 0x2c:
			if (game()->flagTest(0x2c)) {
				game()->enqueue({
					seqCall(kPrimWalk, { xy(0x5c, 0x5c), 0x60 }),
					seqCall(kPrimWait, { 0xa }),
					seqCall(kPrimFace, { 0 }),
					seqCall(kPrimDialogue, { 0, 0x7d1, 0x7d1, 0, 0, 0 }),
					seqEnd()
				});
				return 0;
			}
			break;
		// Hotspot 0x2e: take item 46 (once), comment (line 2004)
		case 0x2e:
			if (game()->flagTestAndClear(0x2e)) {
				game()->enqueue({
					seqCall(kPrimWalk, { xy(0x4b, 0x57), 0x90 }),
					seqCall(kPrimSoundWait, { 0xefd }),
					seqCall(kPrimSetFrame, { &_s47ad78, 0x15 }),
					seqCall(kPrimAddItem, { 0x2e }),
					seqCall(kPrimFace, { 0 }),
					seqCall(kPrimDialogue, { 0, 0x7d4, 0x7d4, 0, 0, 0 }),
					seqEnd()
				});
				return 0;
			}
			break;
		// Hotspot 0x7f5 (room 2037): walk to the edge and comment (line 2002)
		case 0x7f5:
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x16, 0x63), -1 }),
				seqCall(kPrimWalk, { xy(0x13, 0x63), 0x5c }),
				seqCall(kPrimWait, { 0x1e }),
				seqCall(kPrimFace, { 0 }),
				seqCall(kPrimDialogue, { 0, 0x7d2, 0x7d2, 0, 0, 0 }),
				seqEnd()
			});
			break;
		default:
			break;
		}
		return 0;

	// Held item over a hotspot: item 0xd or 0x29 on item 44 = action 0x7d5 while it hangs there
	case kEvItemOnHotspot:
		if (param == 0xd002c || param == 0x29002c)
			return game()->flagTest(0x2c) ? 0x7d5 : 0;
		return 0;

	// Action 0x7d5: knock item 44 down and catch it
	case kEvAction:
		if (param == 0x7d5) {
			const int frame = _s47ac30.frame;
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x5c, 0x5c), 0x60 }),
				seqCall(kPrimShowRelative, { &_s47abe8, &hero(), 5, -0x14, 0, -1 }),
				seqSpawn(),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimClearFlag, { 0x2c }),
				seqCall(kPrimShowRelative, { &_s47ac30, &_s47ac30, -5, 0, 0, 0 }),
				seqCall(kPrimSound, { 0xefc }),
				seqCall(kPrimMoveAccel, { &_s47ac30, 0x96, 0, 2, frame, frame, 1 }),
				seqCall(kPrimHide, { &_s47ac30 }),
				seqCall(kPrimAddItem, { 0x2c }),
				seqJoin(),
				seqCall(kPrimWait, { 0xc }),
				seqCall(kPrimHide, { &_s47abe8 }),
				seqCall(kPrimFace, { 0x5d }),
				seqWaitChildren(),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
			return 0;
		}
		return 0;

	default:
		break;
	}
	return 0;
}

/** Creates the handler (registered in rooms/ported.cpp). */
Room *createRoom2000() {
	return new Room2000();
}

} // End of namespace Gooka
