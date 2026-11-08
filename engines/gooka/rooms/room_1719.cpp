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
 * Room 1719: the torture chamber of Gurha's temple. Gooka is chained to
 * the altar and has to file through the chain; without the file (item
 * 0x37) the game ends (port of the room handler FUN_0041eca0).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Room 1719 (original handler FUN_0041eca0): two animated objects, the
 * chained hero (flag 0x6bb), the chain 0x36, the talking robot guard with
 * the mace 3, Albert's body with the needle 1, and the locked grate 0x6ac
 * back to 1708 that the lockpick (item 2) opens.
 */
class Room1719 : public Room {
public:
	/** Constructs the room objects. */
	Room1719();
	/** Static descriptor 0x441ef0. */
	const RoomDescriptor *getDescriptor() override;
	/** Enter/tick, hotspots, item actions and the game-over blinds (0x17). */
	int handleEvent(int event, int param) override;

private:
	Sprite _s479118; ///< 0x479118: animated object, frames 0x21..0x28
	Sprite _s479508; ///< 0x479508: animated object, frames 0x19..0x20
	Sprite _s479b68; ///< 0x479b68: the chain (object 0x36)
	Sprite _s479838; ///< 0x479838: the mace (object 3)
	Sprite _s479a70; ///< 0x479a70: Albert's body (hotspot 0x6ca), frame 1 while the needle is in it
	Sprite _s4790d0; ///< 0x4790d0: the grate to 1708 (hotspot 0x6ac)
	Actor _n479550;  ///< 0x479550: Gooka chained to the altar
	Actor _n479160;  ///< 0x479160: the robot guard

	/**
	 * Game-over blinds. The original uses one global object (DAT_00473d30,
	 * FUN_00412d30 / FUN_00412d80) for all rooms; every user resets it on
	 * entry, so a per-room instance behaves the same.
	 */
	EndTextScroller _blinds;
};

Room1719::Room1719() : Room(1719) {
}

const RoomDescriptor *Room1719::getDescriptor() {
	return findRoomData(_id);
}

int Room1719::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: objects, the chained hero (flag 0x6bb) and the open grate (flag 0x6cd);
	// without the file (item 0x37) the blinds close after 100 ticks: game over
	case kEvEnter:
		game()->placeObject(&_s479118, 0xfa, 0x5e, 0, 0x21, 0, -1);
		game()->placeObject(&_s479508, 0x72, 0x6c, 0, 0x19, 0, -1);
		game()->placeObject(&_n479160, 0x156, 0x86, 0, 0xd, 0, -1);
		if (game()->flagTest(0x6bb))
			game()->placeObjectIfFlag(0x36, &_s479b68, 0xd0, 0xfd, 2, 9, 1);
		else
			game()->placeObjectIfFlag(0x36, &_s479b68, 0xd0, 0x148, 2, 10, 1);
		game()->placeObjectIfFlag(3, &_s479838, 0x120, 0xa2, 0, 0xc, 1);
		game()->placeObjectIfFlag(0x6ca, &_s479a70, 0x1f0, 0x10e, 2, game()->flagTest(1), 0);
		if (game()->flagTest(0x6bb)) {
			game()->placeObject(&_n479550, 200, 0xba, 2, 2, 0, -1);
			hero().hide();
		}
		if (game()->flagTest(0x6cd))
			game()->placeObjectIfFlag(0x6ac, &_s4790d0, 0, 100, 0, 0x2d, 0);
		if (!game()->hasItem(0x37)) {
			game()->enqueue({
				seqCall(kPrimWait, { 100 }),
				seqWhile(kPrimNone),
				seqCall(kPrimRaiseEvent, { 0x17, 0 }),
				seqYield(),
				seqEndWhile(),
				seqEnd()
			});
		}
		_blinds.reset();
		return 0;

	// Every tick: the two animations advance a frame per 4 ticks
	case kEvTick: {
		const int phase = (int)ticks() / 4 % 8;
		_s479118.setFrame(phase + 0x21);
		_s479508.setFrame(phase + 0x19);
		return 0;
	}

	// Held item over a hotspot: lockpick (2) on the grate while locked, file (0x37) on the chain while chained
	case kEvItemOnHotspot:
		if (param == 0x206ac)
			return game()->flagTest(0x6cd) * 0x6cf;
		if (param == 0x370036)
			return game()->flagTest(0x6bb) * 0x6ba;
		return 0;

	// Action executed: file through the chain (0x6ba) or pick the lock of the grate (0x6cf);
	// the original then falls through into the hotspot case, which ignores these ids
	case kEvAction:
		if (param == 0x6ba) {
			game()->enqueue({
				seqCall(kPrimSoundWait, { 0xf00 }),
				seqCall(kPrimSound, { 0xf11 }),
				seqCall(kPrimSetFrame, { &_s479b68, 10 }),
				seqCall(kPrimShowAt, { &_s479b68, xy(0xd0, 0x148), 0x10002 }),
				seqCall(kPrimDropHeld),
				seqCall(kPrimAnimate, { &_n479550, 8, 4 }),
				seqCall(kPrimHide, { &_n479550 }),
				seqCall(kPrimPlaceHero, { 0x4b, 0x5e, 0 }),
				seqCall(kPrimClearFlag, { 0x6bb }),
				seqEnd()
			});
		} else if (param == 0x6cf) {
			game()->enqueue({
				seqCall(kPrimClearFlag, { 0x6cd }),
				seqCall(kPrimWalk, { xy(0x1b, 0x55), 0x22 }),
				seqCall(kPrimDialogue, { 0, 0x6cd, 0x6cd, 0, 0, 0 }),
				seqCall(kPrimFace, { 0x5c }),
				seqCall(kPrimSoundWait, { 0x324 }),
				seqCall(kPrimSound, { 0xf01 }),
				seqCall(kPrimAnimate, { &_s4790d0, 0x29, 3 }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
		}
		// fall through

	// Hotspot clicked (also reached from the action case above)
	case kEvHotspot:
		switch (param) {
		// Hotspot 3: the mace - the robot guard talks about it and lets Gooka take it
		case 3:
			game()->enqueue({
				seqCall(kPrimWalk, { xy(0x42, 0x45), 0x63 }),
				seqCall(kPrimSound, { 0xf26 }),
				seqCall(kPrimAnimate, { &_n479160, 0x10, 4 }),
				seqCall(kPrimSay, { &_n479160, 0x6bc, 0x10, 0x10, 2 }),
				seqCall(kPrimFace, { 0x24 }),
				seqCall(kPrimDialogue, { &_n479160, 0x6bd, 0x6c9, 0x10, 0x10, 2 }),
				seqCall(kPrimAnimate, { &_n479160, 0xd, 5 }),
				seqCall(kPrimFace, { 0x64 }),
				seqCall(kPrimWait, { 5 }),
				seqCall(kPrimHide, { &_s479838 }),
				seqCall(kPrimHideExit, { 3 }),
				seqCall(kPrimAddItem, { 3 }),
				seqCall(kPrimClearFlag, { 3 }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
			return 0;

		// Hotspot 0x36: the chain - take it once Gooka is free (flag 0x6bb clear)
		case 0x36:
			if (!game()->flagTest(0x6bb)) {
				game()->enqueue({
					seqCall(kPrimWalk, { xy(0x4b, 0x5e), 0x77 }),
					seqCall(kPrimWait, { 5 }),
					seqCall(kPrimHideExit, { 0x36 }),
					seqCall(kPrimAddItem, { 0x36 }),
					seqCall(kPrimClearFlag, { 0x36 }),
					seqCall(kPrimHide, { &_s479b68 }),
					seqCall(kPrimFace, { 0 }),
					seqEnd()
				});
			}
			return 0;

		// Hotspot 0x6ac: the grate to 1708 - rattle it while locked, else use it as exit
		case 0x6ac:
			if (game()->flagTest(0x6cd)) {
				game()->enqueue({
					seqCall(kPrimWalk, { xy(0x1b, 0x55), 0x5c }),
					seqCall(kPrimSoundWait, { 0x323 }),
					seqCall(kPrimFace, { 0 }),
					seqCall(kPrimDialogue, { 0, 0x6ce, 0x6ce, 0, 0, 0 }),
					seqEnd()
				});
				return -1;
			}
			return 0;

		// Hotspot 0x6ca: Albert's body - comment and take the needle (item 1) while it is there
		case 0x6ca:
			if (game()->flagTest(1)) {
				game()->enqueue({
					seqCall(kPrimWalk, { xy(0x91, 0x63), 0x27 }),
					seqCall(kPrimDialogue, { 0, 0x6cb, 0x6cb, 0, 0, 0 }),
					seqCall(kPrimFace, { 0xa5 }),
					seqCall(kPrimSoundWait, { 0xf02 }),
					seqCall(kPrimSetFrame, { &_s479a70, 0 }),
					seqCall(kPrimClearFlag, { 1 }),
					seqCall(kPrimAddItem, { 1 }),
					seqCall(kPrimFace, { 0 }),
					seqEnd()
				});
			}
			return 0;

		default:
			return 0;
		}

	// Hotspot lookup: while Gooka is chained (flag 0x6bb) only the chain 0x36 can be used
	case kEvHotspotOverride:
		if (game()->flagTest(0x6bb) && param != 0x36)
			return -1;
		return 0;

	// Room-private event 0x17 (raised every tick by the game-over sequence): close the blinds a step
	case kEvRoomPrivate1:
		_blinds.addLine();
		return 0;

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom1719() {
	return new Room1719();
}

} // End of namespace Gooka
