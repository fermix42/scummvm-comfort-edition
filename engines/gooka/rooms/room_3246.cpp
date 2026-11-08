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
 * Room3246: port of the room handler FUN_004246b0.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/** Knot hotspots in the order that opens the way to room 2410 (table 0x442f28). */
static const int kRoom3246Melody[8] = { 0xee8, 0xeeb, 0xeec, 0xeed, 0xee8, 0xeeb, 0xeec, 0xeeb };

/**
 * Room 3246 (original handler FUN_004246b0): the wall with seven singing
 * knots (hotspots 0xee8..0xeee, each plays the WAV of its own id). Playing
 * the melody kRoom3246Melody teleports the hero to room 2410. Entered from
 * room 2619 this is also the last room of the game: event 0x18 starts the
 * outro.
 */
class Room3246 : public Room {
public:
	Room3246();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;

private:
	int _melodyPos;   ///< DAT_0047b840: knots of the melody played correctly so far
	int _quietTicks;  ///< DAT_0047b548: idle ticks since the last knot (music fades back in)
};

/** Constructs the room statics. */
Room3246::Room3246() : Room(3246), _melodyPos(0), _quietTicks(0) {
}

/** Static descriptor 0x442ef8. */
const RoomDescriptor *Room3246::getDescriptor() {
	return findRoomData(_id);
}

/** Room events (FUN_004246b0, `what` >= 2). */
int Room3246::handleEvent(int event, int param) {
	switch (event) {
	// Enter: teleported in from 2410, or (from 2619) the end of the game
	case kEvEnter:
		_melodyPos = 0;
		if (param == 0x96a) {
			// Arrival from room 2410: the hero materialises
			hero().place(0x65, 0x60, 0x9c);
			hero().hide();
			game()->enqueue({
				seqCall(kPrimWait, { 1 }),
				seqCall(kPrimSound, { 2096 }),
				seqCall(kPrimAnimate, { &hero(), 149, 2 }),
				seqCall(kPrimSetFrame, { &hero(), 39 }),
				seqCall(kPrimFace, { 0 }),
				seqEnd()
			});
		}
		if (param == 0xa3b) {
			// Arrival from room 2619: materialise, then event 0x18 plays the outro
			game()->setMusicVolume(0);
			hero().place(0x65, 0x60, 0x9c);
			hero().hide();
			game()->enqueue({
				seqCall(kPrimWait, { 1 }),
				seqCall(kPrimSound, { 2096 }),
				seqCall(kPrimAnimate, { &hero(), 149, 2 }),
				seqCall(kPrimSetFrame, { &hero(), 39 }),
				seqCall(kPrimFace, { 0 }),
				seqCall(kPrimWait, { 30 }),
				seqCall(kPrimRaiseEvent, { 24, 0 }),
				seqEnd()
			});
			return 0;
		}
		break;

	// Tick: after 100 idle ticks the music volume creeps back up by 1 per tick
	case kEvTick:
		if (!game()->sequenceBusy()) {
			_quietTicks++;
			if (game()->getMusicVolume() < 0x100 && _quietTicks > 100) {
				game()->setMusicVolume(game()->getMusicVolume() + 1);
				return 0;
			}
		}
		break;

	// Hotspots 0xee8..0xeee: the knots
	case kEvHotspot:
		if (param >= 0xee8 && param <= 0xeee) {
			if (game()->flagTest(0x967)) {
				// First knot: comment (0x967), fade the music out, play, comment (0x968)
				game()->enqueue({
					seqSpawn(),
					seqCall(kPrimMusicFade, { 50, -2, 1 }),
					seqJoin(),
					seqCall(kPrimDialogue, { 0, 2407, 2407, 0, 0, 0 }),
					seqCall(kPrimWalk, { xy(101, 96), 144 }),
					seqWaitChildren(),
					seqCall(kPrimMusicVolume, { 0 }),
					seqCall(kPrimWait, { 20 }),
					seqCall(kPrimSound, { param }),
					seqCall(kPrimWait, { 10 }),
					seqCall(kPrimFace, { 0 }),
					seqCall(kPrimDialogue, { 0, 2408, 2408, 0, 0, 0 }),
					seqCall(kPrimRaiseEvent, { 23, param }),
					seqEnd()
				});
			} else {
				// Later knots: fade the music out and play the knot's tone
				game()->enqueue({
					seqSpawn(),
					seqCall(kPrimMusicFade, { 50, -4, 1 }),
					seqJoin(),
					seqCall(kPrimWalk, { xy(101, 96), 36 }),
					seqWaitChildren(),
					seqCall(kPrimMusicVolume, { 0 }),
					seqCall(kPrimSound, { param }),
					seqCall(kPrimRaiseEvent, { 23, param }),
					seqEnd()
				});
			}
			_quietTicks = 0;
		}
		break;

	// Event 0x17 (raised by the knot sequences): check the melody, the full one teleports to 2410
	case kEvRoomPrivate1:
		// (Past the end the original reads the next data word, 2, which never matches)
		if (_melodyPos >= 8 || kRoom3246Melody[_melodyPos] != param) {
			_melodyPos = 0;
			return 0;
		}
		_melodyPos++;
		if (_melodyPos == 8) {
			game()->enqueue({
				seqCall(kPrimWait, { 20 }),
				seqSpawn(),
				seqCall(kPrimFace, { 39 }),
				seqCall(kPrimDialogue, { 0, 2409, 2409, 0, 0, 0 }),
				seqJoin(),
				seqCall(kPrimSoundWait, { 3811 }),
				seqCall(kPrimWait, { 1 }),
				seqCall(kPrimSoundWait, { 3811 }),
				seqCall(kPrimWait, { 1 }),
				seqCall(kPrimSoundWait, { 3811 }),
				seqCall(kPrimWait, { 1 }),
				seqCall(kPrimSoundWait, { 3811 }),
				seqCall(kPrimWait, { 1 }),
				seqCall(kPrimSoundWait, { 3811 }),
				seqCall(kPrimWait, { 3 }),
				seqCall(kPrimSoundWait, { 3812 }),
				seqWaitChildren(),
				seqCall(kPrimWait, { 20 }),
				seqCall(kPrimSound, { 2097 }),
				seqCall(kPrimSetFrame, { &hero(), 149 }),
				seqCall(kPrimWait, { 2 }),
				seqCall(kPrimAnimate, { &hero(), 156, 2 }),
				seqCall(kPrimHide, { &hero() }),
				seqCall(kPrimWait, { 1 }),
				seqCall(kPrimChangeRoom, { 2410 }),
				seqCall(kPrimShow, { &hero() }),
				seqEnd()
			});
			return 0;
		}
		break;

	// Event 0x18 (end of the game): delete the autosave and play the outro
	case kEvRoomPrivate2:
		// DeleteFileA("<savedir>\GAME.SAVE"): the title must start a new game instead of
		// offering the finished one (the ScummVM saves themselves are kept)
		g_engine->_ui->forceNewGameAtTitle();
		g_engine->setState(kStateOutro);
		return 0;

	default:
		break;
	}
	return 0;
}

/** Factory used by the room registry. */
Room *createRoom3246() {
	return new Room3246();
}

} // End of namespace Gooka
