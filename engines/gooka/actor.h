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
 * Actors: the hero and the talking/walking NPCs (original class with
 * vtable 0x437918, constructor FUN_0040dc30).
 */

#ifndef GOOKA_ACTOR_H
#define GOOKA_ACTOR_H

#include "gooka/sprite.h"
#include "gooka/text.h"

namespace Gooka {

/** Actor flag bits (field +0x44). */
enum ActorFlags {
	kActorScalable = 1, ///< scale follows the walk zone
	kActorTalking = 2   ///< "start a new line": set by the say primitives, cleared by say() when the line starts
};

/**
 * A sprite that can stand on the walk grid, walk along paths and speak
 * lines with subtitles and voice. Public fields mirror the original
 * offsets because room code accesses them directly.
 */
class Actor : public Sprite {
public:
	Actor();
	~Actor() override;

	uint32 flags;     ///< +0x44 ActorFlags
	int speechTimer;  ///< +0x4c remaining ticks of the current line
	uint32 textColor; ///< +0x50 0xFFFFFF; say() does not read it (the original draws subtitles white)
	int voiceHandle;  ///< +0x54 sound handle of the current voice line
	TextSprite subtitle; ///< +0x58 subtitle shown above the actor
	int anchorX;      ///< +0xa4 anchor offset added to the frame anchor
	int anchorY;      ///< +0xa8
	int walkState;    ///< +0xac 0 standing, 1 walking, 2/3 stairs, 4/6 ladders (FUN_00413410)
	int posX, posY;   ///< +0xb0/+0xb4 position, fixed point: cell * 0x400 + 0x200
	int targetX, targetY;     ///< +0xb8/+0xbc walk target (same fixed point)
	int waypointX, waypointY; ///< +0xc0/+0xc4 next waypoint (grid cells)
	int stepX, stepY;         ///< +0xc8/+0xcc step per tick
	int walkDir;      ///< +0xd0 (never read by the original walk code)
	int field_d4;     ///< +0xd4 x distance of a stair move (fixed point)
	int field_d8;     ///< +0xd8 y distance of a stair/ladder move
	int idleFrameMin; ///< +0xdc current animation cycle (walk/stair/ladder frames), first frame
	int idleFrameMax; ///< +0xe0 last frame of the cycle
	int npcVars[5];   ///< +0xe4..+0xf4 written by some rooms for their NPCs; hero: [0] = turn preference set on stairs

	/**
	 * Puts the anchor point on pixel (px, py): updates scale (if scalable),
	 * x/y, priority = zone layer and sub-priority = py (FUN_0040de10).
	 */
	void setPixelPos(int px, int py);
	/** setPixelPos() on the centre of grid cell (gx, gy) (FUN_0040df00). */
	void setGridPos(int gx, int gy);
	/** Places the actor on a grid cell with a frame (Actor_place, FUN_0040df20). */
	void place(int gx, int gy, int frame);
	/** Current grid cell of the anchor point (FUN_0040deb0). */
	Common::Point getGridPos() const;
	/** Sets the anchor offset and re-places the actor (FUN_0040e290). */
	void setAnchor(int ax, int ay);

	/**
	 * Walks towards grid cell (x, y) and turns to frame `dir` at the end
	 * (FUN_00414110); called every tick while waiting. x = y = -1 only turns;
	 * dir -1 keeps the direction, -1000 picks it from the zone type (floor
	 * clicks). Steps on every 3rd tick (twice per tick in zone type 5).
	 * @return non-zero while still walking/turning
	 */
	int walkTo(int x, int y, int dir);
	/** Stops a walk in progress on the current cell (FUN_00413b70). */
	void stopWalking();

	/**
	 * Says a line: subtitle (if enabled), voice WAV `lineId` (else a timer of
	 * strlen * 2 + 12 ticks), talk animation every 4th tick (FUN_0040dfb0).
	 * Called every tick while waiting. The line starts only when
	 * kActorTalking is set (the hero first turns to `restFrame`); a
	 * left-button press ends it on the next call, like the original.
	 * @return non-zero while the line is still being said
	 */
	int say(int lineId, int restFrame, int talkBase, int talkCount);
	/**
	 * Vtable slot 0xc: cancels a pending line start (clears kActorTalking,
	 * FUN_0040e280); for the hero (vtable 0x437990, FUN_00412eb0) it also
	 * resets the walk state and the turn preference. The new-game reset
	 * FUN_0041de10 calls it on the hero. It does not cut a line that is
	 * already being said: say() itself ends the line on a left-button press
	 * (see say()).
	 */
	void stopTalking();
	/** True while a line is pending, the voice plays or the timer runs (FUN_0040e250). */
	bool isTalking() const;

	/**
	 * Does nothing. The original has no per-tick actor work besides walkTo()
	 * and say(): the frame stepper and footsteps (FUN_00412ed0) run only
	 * inside the walk state machine, and FUN_0040f8c0 only calls walkTo()
	 * for the hero's walk target plus the side-effect free isTalking().
	 * Kept for API compatibility; nobody needs to call it.
	 */
	void update();

	/**
	 * Hero talk frames for the dialogue primitive (table 0x4451e8 of
	 * {standFrame, talkStart, talkCount}, scanned like FUN_0042d190): an
	 * entry matches when `frame` is its stand frame or lies in
	 * [talkStart, talkStart + talkCount). Without a match the outputs are
	 * (frame, frame, 1).
	 * @return true if an entry matched
	 */
	static bool heroTalkFrames(int frame, int &restFrame, int &talkBase, int &talkCount);

private:
	/** True for the hero (Game::_hero), whose class has the walking vtable 0x437990. */
	bool isHero() const;
	/** Anchor of the current frame: prefix hot spot or bottom-centre, scaled if scalable (FUN_0040dd80). */
	Common::Point frameAnchor() const;
	/** Lays out and places the subtitle above the actor (FUN_0040df50 + FUN_0040dfb0 + FUN_00408af0). */
	void placeSubtitle();

	/** One walk step (FUN_00413410); returns 0 when there is nothing (left) to do. */
	int walkStep(int x, int y);
	/** Turns one frame towards stand frame `target` (-1000 = by zone type); 0 = already there (FUN_00413c00). */
	int turnTo(int target);
	/** Sets the walk cycle; true if the frame is already in it (FUN_00413010). */
	bool setWalkFrames(int first, int last);
	/** Sets a stair cycle and its entry frame; non-zero while turning first (FUN_00413040). */
	int setStairFrames(int base, int walkBase, int turnFrame);
	/** Next frame of the cycle, footstep every 4th frame (FUN_00412ed0). */
	void stepFrame();
	/** Previous frame of the cycle, footstep every 4th frame (FUN_00412f70). */
	void stepFrameBack();
	/** Plays the zone's footstep sound under the actor (shared by the frame steppers). */
	void playFootstep();
	/** Diagonal stair step, zone type 1 upwards (walk state 2, FUN_00413190). */
	void walkStairsUp();
	/** Diagonal stair step, zone type 1 sideways (walk state 3, FUN_00413200). */
	void walkStairsSide();
	/** Moves along the stair diagonal by the frame phase (FUN_004130e0). */
	void stairStep(int yPhase);
	/** Ladder of zone types 3/4 (walk state 4, FUN_00413270). */
	void climbLadder();
	/** Ladder of zone types 7/8 (walk state 6, FUN_00413340). */
	void climbLadder2();
};

} // End of namespace Gooka

#endif // GOOKA_ACTOR_H
