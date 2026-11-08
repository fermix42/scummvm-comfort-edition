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
 * Room2037: port of the room handler FUN_0041ff60.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** ENDWHILE whose predicate decides whether to loop again (the builder seqEndWhile() has none). */
static SeqInstr seqEndWhileIf(SeqPrim prim, int arg) {
	SeqInstr instr = seqEndWhile();
	instr.prim = prim;
	instr.args.push_back(SeqArg(arg));
	return instr;
}

/** Room 2037 (original handler FUN_0041ff60). */
class Room2037 : public Room {
public:
	/** Room 2037; the members are the original statics. */
	Room2037();
	/** Static descriptor DAT_004420a0. */
	const RoomDescriptor *getDescriptor() override;
	/** The room body (what >= 2). */
	int handleEvent(int event, int param) override;

private:
	/** Puts the NPC DAT_00478720 on the screen as hotspot 0x198 (event 3). */
	void placeNpc();
	/** Tick part while flag 0x7fa is set: everything bobs, object 0x7f8 drifts away once released. */
	void bob(int t);

	ShimmerSprite _e47a8a0; ///< water (FUN_00412690(0x10, 0xffffffff))
	int _h47ab50 = kNoSound; ///< ambient sound 0xd48
	Sprite _s47adc0; ///< object of hotspot 0x7f5 (frames 1..3 / 5..7 / 9..11)
	Sprite _s47ae08; ///< object of hotspot 0x7f8 (frames 13..15), drifts away when released
	Sprite _s479bf8; ///< frame 0x11, thrown by the hero in action 0x7f9
	Sprite _s47a810; ///< frame 0x12, drifts away when arriving from room 3243
	/**
	 * TODO(port): DAT_00478720 is a global NPC actor of the original (ctor
	 * FUN_0041bc40, sprite sheet = image 0x198 set by the new-game reset
	 * FUN_0041de10), used by rooms 1200, 2000, 2037, 2049 and the fallback
	 * (action 0x80f). The core has no such object yet, so the room keeps its
	 * own copy and sets it up completely on entry.
	 */
	Actor _n478720;
	int _drift = 0; ///< DAT_0047a1a4: how far object 0x7f8 has drifted away (hidden at 0x50)
	int _bobY = 0;  ///< DAT_0047aa90: vertical bob offset (0/1, toggled every 17 ticks)
	int _bobX = 0;  ///< DAT_00479ebc: horizontal bob offset (0/1, toggled every 19 ticks)
};

Room2037::Room2037() : Room(2037), _e47a8a0(16, -1) {
}

const RoomDescriptor *Room2037::getDescriptor() {
	return findRoomData(_id);
}

void Room2037::placeNpc() {
	_n478720.sheet = g_engine->getImage(0x198);
	game()->registerHotspotIfFlag(0x198, &_n478720);
	_n478720.x = 0x154;
	_n478720.y = 0x140;
	_n478720.priority = 0;
	_n478720.subPriority = 0;
	_n478720.setFrame(0);
	_n478720.setScale(0x3c);
}

void Room2037::bob(int t) {
	_s47adc0.x = _bobX + 0xf0;
	_s47adc0.y = _bobY + 0xe6;
	_s47adc0.priority = 0;
	_s47adc0.subPriority = 0;
	// (sic: x takes the drift and the vertical bob, y the horizontal bob)
	_s47ae08.x = _drift + _bobY;
	_s47ae08.y = _bobX + 0x120;
	_s47ae08.priority = 0;
	_s47ae08.subPriority = 0;
	if (!game()->flagTest(0x7f8) && t % 3 == 0)
		_drift++;
	if (_drift >= 0x50) {
		_s47ae08.hide();
		game()->flagClear(0x7f9);
	}
	hero().setAnchor(_bobX, _bobY);
	_n478720.x = _bobX + 0x140;
	_n478720.priority = 1;
	_n478720.subPriority = 500;
	_n478720.y = _bobY + 0x154;
	if (t % 0x11 == 0)
		_bobY = (_bobY == 0);
	if (t % 0x13 == 0)
		_bobX = (_bobX == 0);
}

int Room2037::handleEvent(int event, int param) {
	switch (event) {
	// Enter: from room 3243 object 0x12 drifts away while the NPC talks; water, sound, objects, NPC
	case kEvEnter:
		if (param == 0xcab) {
			hero().place(0x4f, 0x5f, 0);
			game()->placeObject(&_s47a810, 0x1ef, 0xdc, 10, 0x12, 0, -1);
			game()->enqueue({
				seqYield(),
				seqSpawn(),
				seqCall(kPrimMoveLinear, { &_s47a810, 0x96, 1, 1, -1, 0x10 }),
				seqCall(kPrimHide, { &_s47a810 }),
				seqJoin(),
				seqCall(kPrimSay, { &_n478720, 0x7fc, 0, 0, 2 }),
				seqCall(kPrimFace, { 0x27 }),
				seqCall(kPrimDialogue, { 0, 0x7fd, 0x7fd, 0, 0, 0 }),
				seqCall(kPrimSay, { &_n478720, 0x7fe, 0, 0, 2 }),
				seqCall(kPrimFace, { 0 }),
				seqCall(kPrimDialogue, { 0, 0x7ff, 0x7ff, 0, 0, 0 }),
				seqWaitChildren(),
				seqEnd()
			});
		}
		game()->placeShimmer(&_e47a8a0, -8, 0xf3, -1, 0x294, 0x9d, 0, 0x10, -1);
		game()->playSound(&_h47ab50, 0xd48, 0, true, -1500, 0);
		game()->placeObjectIfFlag(0x7f5, &_s47adc0, 0xfa, 200, 0, 1, 0);
		game()->placeObjectIfFlag(0x7f8, &_s47ae08, 0, 0x118, 0, 4, 0);
		game()->placeObjectHidden(&_s479bf8, 0, 0, 0, 0x11, 0);
		placeNpc();
		_drift = 0;
		return 0;

	// Leave: stop the ambient sound, reset the hero's anchor offset
	case kEvLeave:
		game()->stopSound(&_h47ab50);
		hero().setAnchor(0, 0);
		return 0;

	// Tick: water, object animations, bobbing while flag 0x7fa is set
	case kEvTick: {
		const int t = (int)ticks();
		if (t % 3 == 0)
			_e47a8a0.step();
		int base;
		if (game()->flagTest(0x7f9))
			base = 1;
		else
			base = game()->flagTest(0x7fa) ? 5 : 9;
		_s47adc0.setFrame((t / 5) % 3 + base);
		_s47ae08.setFrame((t / 5) % 3 + 0xd);
		if (game()->flagTest(0x7fa))
			bob(t);
		return 0;
	}

	// Hotspot clicked: 0x801 (room 2049) changes the room at once
	case kEvHotspot:
		if (param == 0x801)
			game()->changeRoom(0x801);
		return 0;

	// Held item over a hotspot: item 0x2d on 0x7f8 = action 0x7f9, item 0x2e on 0x7f5 = action 0x7fa
	case kEvItemOnHotspot:
		if (param == 0x2d07f8)
			return game()->flagTest(0x7f8) ? 0x7f9 : 0;
		if (param == 0x2e07f5)
			return game()->flagTest(0x7f9) ? 0 : game()->flagTestId(0x7fa);
		return 0;

	// Actions
	case kEvAction:
		// Action 0x7f9: throw item 0x2d at object 0x7f8, then wave until it has drifted away (flag 0x7f9 cleared)
		if (param == 0x7f9) {
			_drift = 0;
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x4a, 0x61), 0x5c }),
				seqCall(kPrimShowRelative, { &_s479bf8, &hero(), 0, 0x10, 0, 0 }),
				seqSpawn(),
				seqCall(kPrimMoveAccel, { &_s479bf8, 0x17c, 0, 1, 0x11, 0x11, 2 }),
				seqJoin(),
				seqCall(kPrimMoveLinear, { &_s479bf8, 0x14, -6, 1, -1, 1 }),
				seqWaitChildren(),
				seqCall(kPrimHide, { &_s479bf8 }),
				seqCall(kPrimClearFlag, { 0x7f8 }),
				seqWhile(kPrimNone),
				seqCall(kPrimFace, { 0x5b }),
				seqCall(kPrimWait, { 5 }),
				seqCall(kPrimFace, { 0x5a }),
				seqCall(kPrimWait, { 5 }),
				seqEndWhileIf(kPrimIfFlag, 0x7f9),
				seqCall(kPrimFace, { 0 }),
				seqCall(kPrimDropHeld),
				seqCall(kPrimRemoveItem, { 0x2d }),
				seqEnd()
			});
			return 0;
		}
		// Action 0x7fa: use item 0x2e on object 0x7f5 - everything drifts off to room 2049
		if (param == 0x7fa) {
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x4f, 0x61), 0xa6 }),
				seqCall(kPrimDropHeld),
				seqCall(kPrimRemoveItem, { 0x2e }),
				seqCall(kPrimClearFlag, { 0x7fa }),
				seqSpawn(),
				seqCall(kPrimMoveLinear, { &_s47adc0, 0x96, 1, 9, -1, 6 }),
				seqJoin(),
				seqSpawn(),
				seqCall(kPrimMoveLinear, { &_n478720, 0x96, 1, 9, -1, 6 }),
				seqJoin(),
				seqSpawn(),
				seqCall(kPrimMoveLinear, { &hero(), 0x96, 1, 9, -1, 6 }),
				seqJoin(),
				seqCall(kPrimDialogue, { 0, 0x7fb, 0x7fb, 0, 0, 0 }),
				seqWaitChildren(),
				seqCall(kPrimChangeRoom, { 0x801 }),
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
Room *createRoom2037() {
	return new Room2037();
}

} // End of namespace Gooka
