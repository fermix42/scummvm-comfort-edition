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
 * Room2049: port of the room handler FUN_00420800.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** Room 2049 (original handler FUN_00420800). */
class Room2049 : public Room {
public:
	/** Room 2049; the members are the original statics. */
	Room2049();
	/** Static descriptor DAT_00442208. */
	const RoomDescriptor *getDescriptor() override;
	/** The room body (what >= 2). */
	int handleEvent(int event, int param) override;

private:
	/** Puts the NPC DAT_00478720 on the screen as hotspot 0x198 (event 3). */
	void placeNpc();
	/**
	 * Action 0x80a with item 0x12 (`hits` false, sequence at 0x420eb8) or 0x22
	 * (`hits` true, sequence at 0x421133): the hero throws the projectile
	 * _s47a398 at object 0x804. A miss ends with line 2059; a hit shakes the
	 * object, ends with line 2060 and clears flag 0x810.
	 */
	void throwItem(bool hits);

	Sprite _s47a160; ///< object of hotspot 0x804 (frame 0 while flag 0x810 is set, else 7/8)
	Sprite _s47a398; ///< the thrown projectile, frame 9
	ShimmerSprite _e47a3e0; ///< water (FUN_00412690(0x10, 0xffffffff))
	int _h479ec8 = kNoSound; ///< ambient sound 0xd48
	/**
	 * TODO(port): DAT_00478720 is a global NPC actor of the original (ctor
	 * FUN_0041bc40, sprite sheet = image 0x198 set by the new-game reset
	 * FUN_0041de10), used by rooms 1200, 2000, 2037, 2049 and the fallback
	 * (action 0x80f). The core has no such object yet, so the room keeps its
	 * own copy and sets it up completely on entry.
	 */
	Actor _n478720;
};

Room2049::Room2049() : Room(2049), _e47a3e0(16, -1) {
}

const RoomDescriptor *Room2049::getDescriptor() {
	return findRoomData(_id);
}

void Room2049::placeNpc() {
	_n478720.sheet = g_engine->getImage(0x198);
	game()->registerHotspotIfFlag(0x198, &_n478720);
	_n478720.x = 0x4b;
	_n478720.y = 0xcd;
	_n478720.priority = 0;
	_n478720.subPriority = 0;
	_n478720.setFrame(0);
	_n478720.setScale(0x50);
}

void Room2049::throwItem(bool hits) {
	SeqProgram program = {
		seqCall(kPrimWalk, { xy(0x3c, 0x50), 0x24 }),
		seqCall(kPrimSetFrame, { &hero(), 0x7e }),
		seqCall(kPrimWait, { 4 }),
		seqCall(kPrimAnimate, { &hero(), 0x82, 4 }),
		seqCall(kPrimShowRelative, { &_s47a398, &hero(), 0x1c, 0x28, 0, -1 }),
		seqCall(kPrimWait, { 0x14 }),
		seqCall(kPrimSetFrame, { &hero(), 0x83 }),
		seqSpawn(),
		seqCall(kPrimSound, { 0xf0e }),
		seqCall(kPrimMoveLinear, { &_s47a398, 6, 0x28, 1, 7, 1 }),
		seqCall(kPrimSound, { 7 }),
		seqCall(kPrimMoveLinear, { &_s47a398, 0xa, 0x28, 1, -7, 1 }),
		seqJoin(),
		seqCall(kPrimSetFrame, { &hero(), 0x7f }),
		seqCall(kPrimWait, { 4 }),
		seqCall(kPrimSetFrame, { &hero(), 0x7e }),
		seqCall(kPrimWait, { 4 }),
		seqCall(kPrimSetFrame, { &hero(), 0x24 }),
		seqWaitChildren(),
		seqCall(kPrimFace, { 0 })
	};
	if (!hits) {
		program.push_back(seqCall(kPrimDialogue, { 0, 0x80b, 0x80b, 0, 0, 0 }));
	} else {
		const SeqProgram shake = {
			seqCall(kPrimWait, { 0x32 }),
			seqCall(kPrimSound, { 0xf0f }),
			seqCall(kPrimAnimate, { &_s47a160, 5, 5 }),
			seqCall(kPrimAnimate, { &_s47a160, 4, 3 }),
			seqCall(kPrimAnimate, { &_s47a160, 5, 4 }),
			seqCall(kPrimAnimate, { &_s47a160, 3, 4 }),
			seqCall(kPrimAnimate, { &_s47a160, 0, 4 }),
			seqCall(kPrimAnimate, { &_s47a160, 4, 5 }),
			seqCall(kPrimAnimate, { &_s47a160, 1, 5 }),
			seqCall(kPrimAnimate, { &_s47a160, 3, 5 }),
			seqCall(kPrimAnimate, { &_s47a160, 0, 8 }),
			seqCall(kPrimWait, { 8 }),
			seqCall(kPrimSetFrame, { &_s47a160, 7 }),
			seqCall(kPrimDialogue, { 0, 0x80c, 0x80c, 0, 0, 0 }),
			seqCall(kPrimClearFlag, { 0x810 })
		};
		for (uint i = 0; i < shake.size(); i++)
			program.push_back(shake[i]);
	}
	program.push_back(seqEnd());
	game()->enqueue(program);
}

int Room2049::handleEvent(int event, int param) {
	switch (event) {
	// Enter: from room 2037 the NPC talks (lines 2065-2073); object 0x804, projectile, water, NPC, ambient sound panned left
	case kEvEnter: {
		if (param == 0x7f5) {
			hero().place(0x19, 0x4d, 0x27);
			game()->enqueue({
				seqYield(),
				seqCall(kPrimDialogue, { &_n478720, -0x811, 0x819, 0, 0, 2 }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
		}
		int frame;
		if (game()->flagTest(0x810))
			frame = 0;
		else
			frame = game()->flagTest(0x80d) ? 7 : 8;
		game()->placeObject(&_s47a160, 400, 0xfa, 0, frame, 0, -1);
		game()->placeObjectHidden(&_s47a398, 100, 0, 0, 9, 0);
		game()->placeShimmer(&_e47a3e0, -0x14, 0x139, -1, 0x2a8, 0x58, 6, 0x10, -1);
		placeNpc();
		game()->playSound(&_h479ec8, 0xd48, 0, true, -1000, 0);
		g_engine->_soundMan->setPan(&_h479ec8, -2000);
		return 0;
	}

	// Leave: stop the ambient sound
	case kEvLeave:
		game()->stopSound(&_h479ec8);
		return 0;

	// Tick: water
	case kEvTick:
		if ((int)ticks() % 3 == 0)
			_e47a3e0.step();
		return 0;

	// Floor click right of grid column 0x3c while flag 0x810 is set: object 0x804 snaps, the hero backs off (line 2050)
	case kEvFloorClick:
		if ((param & 0xFFFF) > 0x3c && game()->flagTest(0x810)) {
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x46, 0x50), -1 }),
				seqSpawn(),
				seqCall(kPrimSound, { 0xef3 }),
				seqCall(kPrimAnimate, { &_s47a160, 5, 4 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimAnimate, { &_s47a160, 4, 4 }),
				seqCall(kPrimWait, { 4 }),
				seqCall(kPrimAnimate, { &_s47a160, 5, 4 }),
				seqCall(kPrimSetFrame, { &_s47a160, 4 }),
				seqCall(kPrimWait, { 0x10 }),
				seqCall(kPrimAnimate, { &_s47a160, 0, 4 }),
				seqJoin(),
				seqCall(kPrimWait, { 0x14 }),
				seqCall(kPrimWalk, { xy(0x37, 0x50), 0 }),
				seqCall(kPrimDialogue, { 0, 0x802, 0x802, 0, 0, 0 }),
				seqWaitChildren(),
				seqEnd()
			});
			return -1;
		}
		return 0;

	// Held item over a hotspot
	case kEvItemOnHotspot:
		switch (param) {
		// Item 0x11 on 0x804: action 0x80d while flag 0x810 is clear and flag 0x80d set
		case 0x110804:
			return game()->flagTest(0x810) ? 0 : game()->flagTestId(0x80d);
		// Items 0x12 / 0x22 on 0x804: action 0x80a with item 0x14 in the inventory while flag 0x810 is set
		case 0x120804:
		case 0x220804:
			return (game()->hasItem(0x14) && game()->flagTest(0x810)) ? 0x80a : 0;
		// Item 0x14 on 0x804: action 0x80a while flag 0x810 is set
		case 0x140804:
			return game()->flagTest(0x810) ? 0x80a : 0;
		// Item 0x20 on 0x803: action 0x805
		case 0x200803:
			return 0x805;
		default:
			break;
		}
		return 0;

	// Actions
	case kEvAction:
		switch (param) {
		// Action 0x805: use item 0x20 at 0x803, it becomes item 0x23
		case 0x805:
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0xb, 0x4f), 0x29 }),
				seqCall(kPrimSetFrame, { &hero(), 0x9d }),
				seqCall(kPrimSoundWait, { 0xf07 }),
				seqCall(kPrimDropHeld),
				seqCall(kPrimReplaceItem, { 0x20, 0x23 }),
				seqCall(kPrimSetFrame, { &hero(), 0x29 }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
			return 0;
		// Action 0x80a: throw item 0x12 (miss) or 0x22 (hit) at 0x804; with neither only a comment (line 2057)
		case 0x80a:
			game()->dropHeldItem();
			if (game()->hasItem(0x12)) {
				game()->removeItem(0x12);
				throwItem(false);
				return 0;
			}
			if (game()->hasItem(0x22)) {
				game()->removeItem(0x22);
				throwItem(true);
				return 0;
			}
			game()->enqueue({
				seqCall(kPrimDialogue, { 0, 0x809, 0x809, 0, 0, 0 }),
				seqEnd()
			});
			return 0;
		// Action 0x80d: take item 0x21 at object 0x804
		case 0x80d:
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x70, 0x5a), 0x62 }),
				seqCall(kPrimSetFrame, { &hero(), 0x9e }),
				seqCall(kPrimSoundWait, { 0xee7 }),
				seqCall(kPrimSetFrame, { &hero(), 0x62 }),
				seqCall(kPrimSoundWait, { 0xf10 }),
				seqCall(kPrimSetFrame, { &_s47a160, 8 }),
				seqCall(kPrimFace, { 0 }),
				seqCall(kPrimAddItem, { 0x21 }),
				seqCall(kPrimClearFlag, { 0x80d }),
				seqEnd()
			});
			return 0;
		default:
			break;
		}
		return 0;

	default:
		break;
	}
	return 0;
}

/** Creates the handler (registered in rooms/ported.cpp). */
Room *createRoom2049() {
	return new Room2049();
}

} // End of namespace Gooka
