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
 * The sequencer primitives (original functions 0x42c680..0x42d4a0).
 * Each returns non-zero while it is still busy; `seq.counter()` is 0 on
 * the first call and counts the ticks the instruction has been running.
 */

#include "common/textconsole.h"

#include "gooka/game.h"
#include "gooka/gooka.h"
#include "gooka/music.h"
#include "gooka/sound.h"

namespace Gooka {

/** Next frame of a cycle between fA and fB (shared tail of the move primitives). */
static int cycleFrame(int frame, int fA, int fB) {
	if (fA < fB) {
		if (frame + 1 <= fB)
			return frame + 1;
	} else {
		if (fB <= frame - 1)
			return frame - 1;
	}
	return fA;
}

/** Position after one step of `delta` towards `target`, clamped when it passes it. */
static int stepTowards(int pos, int delta, int target) {
	const int next = pos + delta;
	const int t = next * delta;
	if (t - delta * target != 0 && delta * target <= t)
		return target;
	return next;
}

// 0x42c7e0: move along one axis, cycling frames fA..fB.
static int primMoveAxis(Sequencer &seq) {
	Sprite *spr = seq.arg(0).sprite();
	const int axis = seq.arg(1).value;
	const int target = seq.arg(2).value;
	const int delta = seq.arg(3).value;
	const int fA = seq.arg(4).value;
	int fB = seq.arg(5).value;
	int frame = spr->frame;
	int x = spr->x;
	int y = spr->y;

	if (seq.counter() == 0) {
		spr->show();
		if (frame < fA || fB < frame) {
			frame = fA;
			fB = fA;
		}
	}
	if ((uint)seq.counter() % (uint)seq.arg(6).value != 0)
		return 1;

	if (axis == 0) {
		if (y == target)
			return 0;
		y = stepTowards(y, delta, target);
	} else {
		if (x == target)
			return 0;
		x = stepTowards(x, delta, target);
	}
	spr->x = x;
	spr->y = y;
	if (fA == -1)
		return 1;
	spr->setFrame(cycleFrame(frame, fA, fB));
	return 1;
}

// 0x42c910: vertical move with acceleration (falls, jumps); velocity in scratch 0.
static int primMoveAccel(Sequencer &seq) {
	Sprite *spr = seq.arg(0).sprite();
	const int targetY = seq.arg(1).value;
	const int accel = seq.arg(3).value;
	const int fA = seq.arg(4).value;
	int fB = seq.arg(5).value;
	int frame = spr->frame;
	int y = spr->y;

	if (seq.counter() == 0) {
		spr->show();
		if (frame < fA || fB < frame) {
			frame = fA;
			fB = fA;
		}
		seq.scratch(0) = seq.arg(2).value;
	}
	if ((uint)seq.counter() % (uint)seq.arg(6).value != 0)
		return 1;
	if (y == targetY)
		return 0;

	y += seq.scratch(0);
	seq.scratch(0) += accel;
	const int v = seq.scratch(0);
	if (y * v - targetY * v != 0 && targetY * v <= y * v)
		y = targetY;

	if (fA != -1)
		frame = cycleFrame(frame, fA, fB);
	spr->y = y;
	spr->setFrame(frame);
	return 1;
}

// 0x42cab0: velocity + acceleration on both axes for `ticks` ticks (velocities in scratch 0/1).
static int primMoveVelocity(Sequencer &seq) {
	Sprite *spr = seq.arg(0).sprite();
	const int x = spr->x;
	const int y = spr->y;

	if (seq.counter() == 0) {
		spr->show();
		seq.scratch(0) = seq.arg(2).value;
		seq.scratch(1) = seq.arg(4).value;
	}
	if (seq.arg(1).value <= seq.counter())
		return 0;
	if ((uint)seq.counter() % (uint)seq.arg(6).value == 0) {
		const int vx = seq.scratch(0);
		seq.scratch(0) += seq.arg(3).value;
		const int vy = seq.scratch(1);
		seq.scratch(1) += seq.arg(5).value;
		spr->x = x + vx;
		spr->y = y + vy;
	}
	return 1;
}

// 0x42cc00: linear move, x every `everyX` ticks, y every `everyY` ticks.
static int primMoveLinear(Sequencer &seq) {
	Sprite *spr = seq.arg(0).sprite();
	if (seq.counter() == 0)
		spr->show();
	if (seq.arg(1).value <= seq.counter())
		return 0;
	if (seq.counter() % seq.arg(3).value == 0)
		spr->x += seq.arg(2).value;
	if (seq.counter() % seq.arg(5).value == 0)
		spr->y += seq.arg(4).value;
	return 1;
}

// 0x42d010: fade the music volume by `step` every `every` ticks until `target`.
static int primMusicFade(Sequencer &seq) {
	Game *game = g_engine->_game;
	const int current = game->getMusicVolume();
	const int target = seq.arg(0).value;
	const int step = seq.arg(1).value;

	if (seq.counter() == 0 && step * current - step * target != 0 && step * target <= step * current)
		return 0;
	if (seq.counter() % seq.arg(2).value == 0) {
		const int next = step * (current + step);
		if (next - step * target != 0 && step * target <= next) {
			game->setMusicVolume(target);
			return 0;
		}
		game->setMusicVolume(current + step);
	}
	return 1;
}

// 0x42d190: consecutive lines alternating between the NPC and the hero; the sign of
// the first line says who starts (+ = hero). Scratch 0 = current line, 1 = hero's turn.
static int primDialogue(Sequencer &seq) {
	Game *game = g_engine->_game;
	Actor &hero = game->_hero;
	Actor *npc = seq.arg(0).actor();
	const int first = seq.arg(1).value;
	const int last = seq.arg(2).value;
	int restFrame = seq.arg(3).value;
	int talkBase = seq.arg(4).value;
	int talkCount = seq.arg(5).value;

	if (first == 0)
		return 0;
	if (seq.counter() == 0) {
		seq.scratch(0) = ABS(first);
		seq.scratch(1) = first > 0 ? 1 : 0;
		if (first < 1) {
			if (npc)
				npc->flags |= kActorTalking;
		} else {
			hero.flags |= kActorTalking;
		}
	}

	// Hero talk frames matching the current stand or talk frame (table 0x4451e8)
	int heroRest, heroTalk, heroCount;
	Actor::heroTalkFrames(hero.frame, heroRest, heroTalk, heroCount);

	if (restFrame == -1 && npc) {
		restFrame = npc->frame;
		talkCount = 1;
		talkBase = restFrame;
	}

	if (seq.scratch(1) == 0) {
		if (npc && npc->say(seq.scratch(0), restFrame, talkBase, talkCount))
			return 1;
		if (seq.scratch(0) == last)
			return 0;
		hero.flags |= kActorTalking;
	} else {
		if (hero.say(seq.scratch(0), heroRest, heroTalk, heroCount))
			return 1;
		if (seq.scratch(0) == last)
			return 0;
		if (npc)
			npc->flags |= kActorTalking;
	}
	seq.scratch(0)++;
	seq.scratch(1) = !seq.scratch(1);
	return 1;
}

// Dispatches the primitive of the current instruction.
int runPrimitive(Sequencer &seq) {
	Game *game = g_engine->_game;
	Actor &hero = game->_hero;
	const SeqPrim prim = seq.currentPrim();

	switch (prim) {
	case kPrimIfFlag:
		return game->flagTest(seq.arg(0).value);
	case kPrimIfNotFlag:
		return !game->flagTest(seq.arg(0).value);
	case kPrimIfItem:
		return game->hasItem(seq.arg(0).value);
	case kPrimIfNotItem:
		return !game->hasItem(seq.arg(0).value);

	case kPrimWalk: {
		const int v = seq.arg(0).value;
		return hero.walkTo((int16)(v & 0xFFFF), (int16)(v >> 16), seq.arg(1).value);
	}
	case kPrimFace:
		if (seq.counter() == 0)
			hero.show();
		return hero.walkTo(-1, -1, seq.arg(0).value);

	case kPrimAnimate: {
		Sprite *spr = seq.arg(0).sprite();
		if (seq.counter() == 0)
			spr->show();
		if ((uint)seq.counter() % (uint)seq.arg(2).value != 0)
			return 1;
		const int target = seq.arg(1).value;
		if (spr->frame < target) {
			spr->setFrame(spr->frame + 1);
			return 1;
		}
		if (target < spr->frame) {
			spr->setFrame(spr->frame - 1);
			return 1;
		}
		return 0;
	}
	case kPrimMoveAxis:
		return primMoveAxis(seq);
	case kPrimMoveAccel:
		return primMoveAccel(seq);
	case kPrimMoveVelocity:
		return primMoveVelocity(seq);
	case kPrimMoveLinear:
		return primMoveLinear(seq);

	case kPrimSay: {
		Actor *actor = seq.arg(0).actor();
		if (seq.counter() == 0)
			actor->flags |= kActorTalking;
		return actor->say(seq.arg(1).value, seq.arg(2).value, seq.arg(3).value, seq.arg(4).value);
	}
	case kPrimDialogue:
		return primDialogue(seq);
	case kPrimWait:
		return seq.counter() < seq.arg(0).value;

	case kPrimShow:
		seq.arg(0).sprite()->show();
		return 0;
	case kPrimHide:
		seq.arg(0).sprite()->hide();
		return 0;
	case kPrimSetFrame:
		seq.arg(0).sprite()->setFrame(seq.arg(1).value);
		return 0;
	case kPrimShowAt: {
		Sprite *spr = seq.arg(0).sprite();
		const uint pos = seq.arg(1).value;
		const uint prio = seq.arg(2).value;
		spr->show();
		spr->y = pos >> 16;
		spr->x = pos & 0xFFFF;
		spr->priority = prio & 0xFFFF;
		spr->subPriority = prio >> 16;
		return 0;
	}
	case kPrimShowRelative: {
		Sprite *spr = seq.arg(0).sprite();
		const Sprite *ref = seq.arg(1).sprite();
		spr->show();
		spr->x = ref->x + seq.arg(2).value;
		spr->y = ref->y + seq.arg(3).value;
		spr->priority = ref->priority + seq.arg(4).value;
		spr->subPriority = ref->subPriority + seq.arg(5).value;
		return 0;
	}
	case kPrimShowPriority: {
		Sprite *spr = seq.arg(0).sprite();
		const uint prio = seq.arg(1).value;
		spr->show();
		spr->priority = prio & 0xFFFF;
		spr->subPriority = prio >> 16;
		return 0;
	}
	case kPrimPlaceActor: {
		Actor *actor = seq.arg(0).actor();
		const uint pos = seq.arg(1).value;
		const uint prio = seq.arg(2).value;
		actor->show();
		actor->place(pos & 0xFFFF, pos >> 16, prio >> 16);
		actor->priority = prio & 0xFFFF;
		actor->subPriority = pos >> 16;
		return 0;
	}
	case kPrimPlaceHero:
		hero.show();
		hero.place(seq.arg(0).value, seq.arg(1).value, seq.arg(2).value);
		return 0;

	case kPrimSound:
		game->playSound(nullptr, seq.arg(0).value);
		return 0;
	case kPrimSoundWait: {
		const int subtitleId = seq.arg(1).value;
		if (seq.counter() == 0) {
			game->playSound(&game->_ambientHandle, seq.arg(0).value);
			if (subtitleId)
				game->showOffscreenSubtitle(subtitleId);
		}
		const bool playing = game->isSoundPlaying(&game->_ambientHandle);
		if (subtitleId && (!playing || !game->_subtitles))
			game->hideOffscreenSubtitle();
		return playing;
	}
	case kPrimWaitHandle:
		return game->isSoundPlaying(seq.arg(0).handle());
	case kPrimAmbient:
		game->playSound(&game->_ambientHandle, seq.arg(0).value, 0, true);
		return 0;
	case kPrimAmbientStop:
		game->stopSound(&game->_ambientHandle);
		return 0;

	case kPrimMusicPause:
		g_engine->_music->pause();
		return 0;
	case kPrimMusicResume:
		g_engine->_music->resume();
		return 0;
	case kPrimMusicFade:
		return primMusicFade(seq);
	case kPrimMusicVolume:
		game->setMusicVolume(seq.arg(0).value);
		return 0;

	case kPrimHideExit:
		game->hideExitSprite(seq.arg(0).value);
		return 0;
	case kPrimShowExit:
		game->showExitSprite(seq.arg(0).value);
		return 0;

	case kPrimAddItem:
		game->addItem(seq.arg(0).value);
		game->flagClear(seq.arg(0).value);
		return 0;
	case kPrimRemoveItem:
		game->removeItem(seq.arg(0).value);
		return 0;
	case kPrimReplaceItem:
		game->replaceItem(seq.arg(0).value, seq.arg(1).value, true);
		return 0;
	case kPrimReplaceItemQuiet:
		game->replaceItem(seq.arg(0).value, seq.arg(1).value, false);
		return 0;
	case kPrimDropHeld:
		game->dropHeldItem();
		return 0;

	case kPrimChangeRoom:
		game->changeRoom(seq.arg(0).value);
		return 0;
	case kPrimClearBusy:
		game->clearBusy();
		return 0;
	case kPrimActOnHotspot:
		game->actOnHotspot(seq.arg(0).value, 0);
		return 0;
	case kPrimRaiseEvent:
		game->raiseEvent(seq.arg(0).value, seq.arg(1).value);
		return 0;
	case kPrimClearFlag:
		game->flagClear(seq.arg(0).value);
		return 0;
	case kPrimSetFlag:
		game->flagSet(seq.arg(0).value, true);
		return 0;

	case kPrimNone:
	default:
		return 1;
	}
}

} // End of namespace Gooka
