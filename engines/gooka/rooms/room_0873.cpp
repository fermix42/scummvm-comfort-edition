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
 * Room 873: the kitchen with the cook Korvana and the fire (port of the
 * room handler FUN_00426f60).
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Walks the hero to grid (x, y) facing `dir`, runs the dialogue lines
 * first..last with `npc` and turns to the front (engine helper FUN_0040f760,
 * ported here because the core has no equivalent).
 */
static void walkAndTalk(int x, int y, int dir, Actor *npc, int restFrame, int talkBase, int talkCount,
		int first, int last) {
	game()->enqueue({
		seqCall(kPrimWalk, { xy(x, y), dir }),
		seqCall(kPrimDialogue, { npc, first, last, restFrame, talkBase, talkCount }),
		seqCall(kPrimFace, { 0 }),
		seqEnd()
	});
}

/**
 * Room 873 (original handler FUN_00426f60). The fire (hotspot 0x36b) goes
 * through the states burning (flag 0x373), coal added (0x36c cleared),
 * fanned (0x36d cleared) and the fragment heated (0x36e cleared); pouring
 * water (0x373 cleared) puts it out. Its picture and ambient sound follow
 * the state; the cook (0x371) has a conversation for each part of the story.
 */
class Room0873 : public Room {
public:
	/** Room id 873; the objects are set up when the room is entered. */
	Room0873();
	/** Static descriptor 0x443f18. */
	const RoomDescriptor *getDescriptor() override;
	/** Enter/leave/tick, the cook and item 0x25, fire actions and the fire-changed callback 0x17. */
	int handleEvent(int event, int param) override;

private:
	/** Starts the fire sound matching the fire state (shared by events 3 and 0x17). */
	void playFireSound();

	Actor _actKorvana;  ///< 0x47c118: the cook Korvana
	Sprite _sprFire; ///< 0x47bec8: the fire
	int _hSndFire = kNoSound; ///< 0x47bf6c: fire sound loop
};

Room0873::Room0873() : Room(873) {
}

const RoomDescriptor *Room0873::getDescriptor() {
	return findRoomData(_id);
}

void Room0873::playFireSound() {
	// 0xef0 without coal, 0xef1 with coal, 0xef2 fanned
	int wav;
	if (game()->flagTest(0x36c))
		wav = 3824;
	else
		wav = 3826 - game()->flagTest(0x36d);
	game()->playSound(&_hSndFire, wav, 0, true, 0, 0);
}

int Room0873::handleEvent(int event, int param) {
	switch (event) {
	// Room entered: fire sound while it burns, the cook and the fire
	case kEvEnter:
		if (game()->flagTest(0x373))
			playFireSound();
		game()->placeObject(&_actKorvana, 100, 185, 1, 0, 0, -1);
		game()->placeObject(&_sprFire, 542, 228, 0, 7, 0, -1);
		return 0;

	// Room left: stop the fire sound
	case kEvLeave:
		game()->stopSound(&_hSndFire);
		return 0;

	// Every tick: fire picture for the current state
	case kEvTick:
		if (!game()->flagTest(0x373)) {
			// Put out: frame 0x19 before the coal was added, nothing afterwards
			if (!game()->flagTest(0x36c))
				_sprFire.setFrame(0x19);
			else
				_sprFire.setFrame(-1);
			return 0;
		}
		if (game()->flagTest(0x36c)) {
			// Burning without coal: frames 7..14, one every 4 ticks
			_sprFire.setFrame((int)ticks() / 4 % 8 + 7);
			return 0;
		}
		// With coal: frames 0x10..0x13, fanned 0x14..0x17, one every 5 ticks
		_sprFire.setFrame((game()->flagTest(0x36d) ? 0x10 : 0x14) + (int)ticks() / 5 % 4);
		return 0;

	// Hotspot clicked
	case kEvHotspot:
		switch (param) {
		// Hotspot 0x25: the hook (item 0x25) in the fire - too hot to take while the fire burns
		case 0x25:
			if (game()->flagTest(0x373)) {
				game()->enqueue({
					seqCall(kPrimWalk, { xy(128, 73), 99 }),
					seqCall(kPrimFace, { 0 }),
					seqCall(kPrimDialogue, { 0, 882, 882, 0, 0, 0 }),
					seqEnd()
				});
				return -1;
			}
			return 0;

		// Hotspot 0x371: the cook - talk according to the progress of the story
		case 0x371:
			if (game()->flagTest(0x339) && game()->flagTest(0x374)) {
				// Before the interrogation: "good morning, what's for breakfast?"
				walkAndTalk(113, 90, 34, &_actKorvana, 0, 1, 1, 884, 885);
				return 0;
			}
			if ((!game()->flagTest(0x339) && game()->flagTest(0x376)) ||
					(!game()->flagTest(0x322) && game()->flagTest(0x385))) {
				// After the interrogation: the sister's cooking; after the locked door: the broken key (0x1c, 0x1d)
				game()->enqueue({
					seqCall(kPrimWalk, { xy(113, 90), 34 }),
					seqIf(kPrimIfNotFlag, { 0x339 }),
					seqIf(kPrimIfFlag, { 0x376 }),
					seqCall(kPrimDialogue, { &_actKorvana, 886, 899, 0, 1, 1 }),
					seqEndIf(),
					seqEndIf(),
					seqIf(kPrimIfNotFlag, { 0x322 }),
					seqIf(kPrimIfFlag, { 0x385 }),
					seqCall(kPrimDialogue, { &_actKorvana, 901, 902, 0, 1, 1 }),
					seqCall(kPrimAddItem, { 0x1c }),
					seqCall(kPrimWait, { 0x32 }),
					seqCall(kPrimAddItem, { 0x1d }),
					seqEndIf(),
					seqEndIf(),
					seqCall(kPrimDialogue, { 0, 900, 900, 0, 0, 0 }),
					seqCall(kPrimFace, { 0 }),
					seqEnd()
				});
				return 0;
			}
			// Nothing new while the sister is still in the house
			if (game()->flagTest(0x34f))
				return 0;
			if (!game()->flagTest(0x387))
				return 0;
			// After the sister was taken away: the tall stranger and Digor
			walkAndTalk(113, 90, 34, &_actKorvana, 0, 1, 1, 903, 921);
			return 0;

		default:
			return 0;
		}

	// Held item over the fire (0x36b): the next step of the fire while it burns
	case kEvItemOnHotspot:
		switch (param) {
		// Item 0x19 (full coal scuttle): add coal
		case 0x19036b:
			return game()->flagTestId(0x36c) * game()->flagTest(0x373);
		// Item 0x18 (bellows): fan the fire once the coal is in
		case 0x18036b:
			return game()->flagTestId(0x36d) * game()->flagTest(0x373) * !game()->flagTest(0x36c);
		// Item 0x1e (key fragment in tongs): heat it once the fire is fanned
		case 0x1e036b:
			return game()->flagTestId(0x36e) * game()->flagTest(0x373) * !game()->flagTest(0x36d);
		// Item 0x26 (bowl of water): put the fire out
		case 0x26036b:
			return game()->flagTestId(0x373);
		default:
			return 0;
		}

	// Action executed
	case kEvAction:
		switch (param) {
		// Add coal: the full scuttle 0x19 becomes the empty one 0x17
		case 0x36c:
			game()->enqueue({
				seqCall(kPrimWalk, { xy(127, 73), 38 }),
				seqCall(kPrimSetFrame, { &hero(), 0x93 }),
				seqCall(kPrimSoundWait, { 3808 }),
				seqCall(kPrimSetFrame, { &hero(), 0x27 }),
				seqCall(kPrimDropHeld),
				seqCall(kPrimReplaceItemQuiet, { 0x19, 0x17 }),
				seqCall(kPrimClearFlag, { 0x36c }),
				seqCall(kPrimRaiseEvent, { 0x17, 0 }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
			return 0;
		// Fan the fire
		case 0x36d:
			game()->enqueue({
				seqCall(kPrimWalk, { xy(120, 72), 38 }),
				seqCall(kPrimShowRelative, { &hero(), &hero(), 0x28, 0, 0, 0 }),
				seqCall(kPrimSetFrame, { &hero(), 0xa3 }),
				seqCall(kPrimSoundWait, { 24 }),
				seqCall(kPrimSetFrame, { &hero(), 0x76 }),
				seqCall(kPrimShowRelative, { &hero(), &hero(), -0x28, 0, 0, 0 }),
				seqCall(kPrimClearFlag, { 0x36d }),
				seqCall(kPrimRaiseEvent, { 0x17, 0 }),
				seqCall(kPrimFace, { 0 }),
				seqCall(kPrimDialogue, { 0, 879, 879, 0, 0, 0 }),
				seqEnd()
			});
			return 0;
		// Heat the key fragment: 0x1e becomes the red-hot fragment 0x1f
		case 0x36e:
			game()->enqueue({
				seqCall(kPrimWalk, { xy(159, 78), 37 }),
				seqCall(kPrimSetFrame, { &hero(), 0x94 }),
				seqCall(kPrimWait, { 20 }),
				seqCall(kPrimSetFrame, { &hero(), 0x25 }),
				seqCall(kPrimDropHeld),
				seqCall(kPrimReplaceItem, { 0x1e, 0x1f }),
				seqCall(kPrimWalk, { xy(145, 80), 0 }),
				seqCall(kPrimDialogue, { 0, 880, 880, 0, 0, 0 }),
				seqEnd()
			});
			return 0;
		// Put the fire out: the bowl of water 0x26 becomes the empty bowl 0x20
		case 0x373:
			game()->enqueue({
				seqCall(kPrimWalk, { xy(128, 73), 99 }),
				seqCall(kPrimSetFrame, { &hero(), 0x9f }),
				seqCall(kPrimSoundWait, { 3851 }),
				seqCall(kPrimClearFlag, { 0x373 }),
				seqCall(kPrimRaiseEvent, { 0x17, 0 }),
				seqCall(kPrimWait, { 10 }),
				seqCall(kPrimSetFrame, { &hero(), 0x63 }),
				seqCall(kPrimDropHeld),
				seqCall(kPrimReplaceItemQuiet, { 0x26, 0x20 }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
			return 0;
		default:
			return 0;
		}

	// Fire state changed (raised by the action sequences): restart the matching fire sound
	case kEvRoomPrivate1:
		game()->stopSound(&_hSndFire);
		if (game()->flagTest(0x373))
			playFireSound();
		return 0;

	default:
		return 0;
	}
}

/** Factory used by the room registry. */
Room *createRoom0873() {
	return new Room0873();
}

} // End of namespace Gooka
