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

#include "common/debug.h"

#include "gooka/actor.h"
#include "gooka/detection.h"
#include "gooka/game.h"
#include "gooka/gooka.h"
#include "gooka/image.h"
#include "gooka/resource.h"
#include "gooka/room.h"
#include "gooka/sound.h"
#include "gooka/walk.h"

namespace Gooka {

/** Hero stand frames around the circle, clockwise from the front (0x43ebc0). */
static const int kStandFrames[8] = { 0, 33, 34, 37, 39, 38, 36, 35 };
/** Stand frame after the straight walk cycles 1-8 left, 9-16 right, 17-24 up, 25-32 down (0x43ebe0). */
static const int kWalkStand[4] = { 34, 36, 39, 0 };
/** Stand frame after the diagonal walk cycles 0xae.., 0xb6.., 0xbe.., 0xc6.. (0x43ebf0). */
static const int kDiagStand[4] = { 33, 35, 37, 38 };
/**
 * Diagonal walk cycles {first, last, turn frame} (0x43eaf0), indexed by
 * (target left of pos) * 2 + (target above pos).
 */
static const int kDiagWalk[4][3] = {
	{ 182, 189, 184 }, // right, down
	{ 198, 205, 200 }, // right, up
	{ 174, 181, 176 }, // left, down
	{ 190, 197, 192 }  // left, up
};

/** A transition animation between stand frames (0x43eb20). */
struct TurnAnim {
	int target; ///< stand frame the animation leads to
	int first;
	int last;
	int flag;   ///< always 0 in the game data
};

/** Transition animations (0x43eb20, 10 records). */
static const TurnAnim kTurnAnims[10] = {
	{ 34, 90, 96, 0 }, { 36, 97, 103, 0 }, { 33, 40, 42, 0 }, { 0, 104, 108, 0 },
	{ 34, 109, 111, 0 }, { 36, 112, 114, 0 }, { 42, 115, 116, 0 }, { 37, 117, 119, 0 },
	{ 39, 164, 173, 0 }, { 38, 144, 146, 0 }
};

/** Hero talk frames {standFrame, talkStart, talkCount} (0x4451e8). */
static const int kHeroTalk[5][3] = {
	{ 0, 104, 4 }, { 34, 109, 3 }, { 36, 112, 3 }, { 42, 115, 2 }, { 39, 39, 1 }
};

/** Index of a stand frame in kStandFrames, -1 if it is none (FUN_004140f0). */
static int standIndex(int frame) {
	for (int i = 0; i < 8; i++) {
		if (kStandFrames[i] == frame)
			return i;
	}
	return -1;
}

/** Transition animation containing `frame`, nullptr if none (FUN_00413bd0). */
static const TurnAnim *findTurnAnim(int frame) {
	for (int i = 0; i < 10; i++) {
		if (kTurnAnims[i].first <= frame && frame <= kTurnAnims[i].last)
			return &kTurnAnims[i];
	}
	return nullptr;
}

/** -1, 0 or 1 like the original `((-1 < d) - 1 & ~1) + 1` (0 for d == 0). */
static int signOf(int d) {
	return d == 0 ? 0 : (d > 0 ? 1 : -1);
}

/** Shrinks an anchor coordinate like FUN_00409440 (scale < 0x100 only). */
static int scaleCoord(int v, int scale) {
	return (v * scale + 0x80) / 256 + 1;
}

/**
 * Left-button press edge of the current tick (DAT_004781d0, computed at the
 * start of FUN_0041d280 from the WM_LBUTTONDOWN state). Sampled from the
 * engine input on every say() call; a sample from a tick other than the
 * previous one counts as "held", so a click is never seen twice or late.
 */
static bool leftClickEdge() {
	static bool valid = false;
	static uint32 lastTick = 0;
	static bool lastDown = true;
	static bool edge = false;

	const uint32 tick = g_engine->_tickCounter;
	if (valid && tick == lastTick)
		return edge;
	const bool down = g_engine->_input.leftDown;
	edge = down && valid && !lastDown && tick == lastTick + 1;
	lastDown = down;
	lastTick = tick;
	valid = true;
	return edge;
}

// FUN_0040dc30. Fields the original leaves to the zeroed static storage are cleared here.
Actor::Actor() : flags(0), speechTimer(0), textColor(0xFFFFFF), voiceHandle(kNoSound),
		anchorX(0), anchorY(0), walkState(0), posX(0), posY(0), targetX(0), targetY(0),
		waypointX(0), waypointY(0), stepX(0), stepY(0), walkDir(0), field_d4(0), field_d8(0),
		idleFrameMin(0), idleFrameMax(0) {
	for (int i = 0; i < 5; i++)
		npcVars[i] = 0;
}

// FUN_0040dcd0 / FUN_0040dcf0: the subtitle hides itself; the voice is stopped
// (the original only detaches the handle, which SoundManager cannot do).
Actor::~Actor() {
	if (g_engine && g_engine->_soundMan && voiceHandle != kNoSound)
		g_engine->_soundMan->stop(&voiceHandle);
}

// Hero = Game::_hero (the only object of the walking class 0x437990).
bool Actor::isHero() const {
	return g_engine && g_engine->_game && this == &g_engine->_game->_hero;
}

// FUN_0040dd80: the default anchor uses the image header size (Image fields
// [1]/[2]), not the frame record. Deviation: no sheet gives (0, 0).
Common::Point Actor::frameAnchor() const {
	if (!sheet)
		return Common::Point(0, 0);
	int ax = -1, ay = -1;
	if (frame >= 0) {
		const Common::Point hot = sheet->getHotspot(frame);
		ax = hot.x;
		ay = hot.y;
	}
	if (ax == -1 || (ax == 0 && ay == 0)) {
		ax = sheet->getWidth() / 2;
		ay = sheet->getHeight();
	}
	if ((flags & kActorScalable) && scale < 0x100) {
		ax = scaleCoord(ax, scale);
		ay = scaleCoord(ay, scale);
	}
	return Common::Point(ax, ay);
}

// FUN_0040de10: scale and priority from the zone under (px, py).
void Actor::setPixelPos(int px, int py) {
	WalkMap &walk = g_engine->_game->_walk;
	const ZoneRecord *zone = walk.zoneAt(px >> 2, py >> 2);
	const int layer = zone ? zone->layer : 0;
	setScale((flags & kActorScalable) ? walk.scaleAt(px, py) : kScaleFull);
	const Common::Point a = frameAnchor();
	x = anchorX + (px - a.x);
	y = anchorY + (py - a.y);
	priority = layer;
	subPriority = py;
}

// FUN_0040df00: the centre pixel of the cell.
void Actor::setGridPos(int gx, int gy) {
	setPixelPos(gx * 4 + 2, gy * 4 + 2);
}

// FUN_0040df20: position first (anchor of the old frame), then the frame.
void Actor::place(int gx, int gy, int f) {
	setGridPos(gx, gy);
	setFrame(f);
}

// FUN_0040deb0: note that the anchor offset (anchorX/Y) is not subtracted.
Common::Point Actor::getGridPos() const {
	const Common::Point a = frameAnchor();
	return Common::Point((a.x + x) >> 2, (y + a.y) >> 2);
}

// FUN_0040e290.
void Actor::setAnchor(int ax, int ay) {
	anchorY = ay;
	anchorX = ax;
	const Common::Point g = getGridPos();
	setGridPos(g.x, g.y);
}

// FUN_00414110: steps every 3rd tick (every tick, twice, in zone type 5);
// type 6 ends the walk without the final turn.
int Actor::walkTo(int tx, int ty, int dir) {
	const Common::Point g = getGridPos();
	const ZoneRecord *zone = g_engine->_game->_walk.zoneAt(g.x, g.y);
	const bool fast = zone && zone->type == 5;

	if ((int32)g_engine->_tickCounter % 3 != 0 && !fast)
		return 1;
	if (walkStep(tx, ty) != 0 && (!fast || walkStep(tx, ty) != 0))
		return 1;
	if (zone && zone->type == 6)
		return 0;
	return turnTo(dir);
}

// FUN_00413b70: the walk ends at the centre of the current cell.
void Actor::stopWalking() {
	if (walkState != 1)
		return;
	const Common::Point g = getGridPos();
	waypointX = g.x;
	posX = targetX = g.x * 0x400 + 0x200;
	waypointY = g.y;
	posY = targetY = g.y * 0x400 + 0x200;
}

// FUN_00413410. Walk states: 0 idle, 1 walking, 2/3 stairs (zone type 1),
// 4 ladder (types 3/4), 6 ladder (types 7/8).
int Actor::walkStep(int tx, int ty) {
	WalkMap &walk = g_engine->_game->_walk;

	switch (walkState) {
	case 2:
		walkStairsUp();
		return 1;
	case 3:
		walkStairsSide();
		return 1;
	case 4:
		climbLadder();
		return 1;
	case 6:
		climbLadder2();
		return 1;
	default:
		break;
	}

	if (walkState != 1) {
		// Standing: start the next leg of the path
		const Common::Point g = getGridPos();
		if ((g.x == tx || tx == -1) && (g.y == ty || ty == -1))
			return 0;
		posX = g.x * 0x400 + 0x200;
		posY = g.y * 0x400 + 0x200;
		if (!walk.findNextWaypoint(g.x, g.y, tx, ty, waypointX, waypointY)) {
			setGridPos(tx, ty);
			return 0;
		}
		stepY = 0;
		stepX = 0;
		targetX = waypointX * 0x400 + 0x200;
		targetY = waypointY * 0x400 + 0x200;
		// The original reads the zone record without a null check
		const ZoneRecord *zone = walk.zoneAt(g.x, g.y);
		const int type = zone ? zone->type : 0;
		int nx, ny;

		if (g.y != waypointY) {
			if (g.x == waypointX) {
				// Vertical leg
				if (type == 3 || type == 4) {
					// Ladder: merge a following vertical ladder leg
					if (walk.zoneType(waypointX, waypointY) == 3 &&
							walk.findNextWaypoint(waypointX, waypointY, tx, ty, nx, ny) && waypointX == nx) {
						waypointY = ny;
						targetY = ny * 0x400 + 0x200;
					}
					field_d8 = targetY - posY;
					climbLadder();
					return 1;
				}
				if (type == 7 || type == 8) {
					if (walk.zoneType(waypointX, waypointY) == 7 &&
							walk.findNextWaypoint(waypointX, waypointY, tx, ty, nx, ny) && waypointX == nx) {
						waypointY = ny;
						targetY = ny * 0x400 + 0x200;
					}
					field_d8 = targetY - posY;
					climbLadder2();
					return 1;
				}
				if (type == 1 && walk.zoneType(waypointX, waypointY) == 1) {
					// Stairs: going up jumps to the waypoint, going down turns into a sideways stair move
					if (targetY <= posY) {
						setGridPos(waypointX, waypointY);
						return 1;
					}
					if (walk.findNextWaypoint(waypointX, waypointY, tx, ty, nx, ny) && waypointY == ny) {
						waypointX = nx;
						targetX = nx * 0x400 + 0x200;
						field_d4 = targetX - posX;
						field_d8 = targetY - posY;
						walkStairsSide();
						return 1;
					}
				}
				bool up = targetY < posY;
				if (type == 2)
					up = !up;
				if (up) {
					if (!setWalkFrames(0x11, 0x18) && turnTo(0x14))
						return 1;
					stepY = -5;
				} else {
					if (!setWalkFrames(0x19, 0x20) && turnTo(0x1c))
						return 1;
					stepY = 5;
				}
				walkState = 1;
			} else {
				// Diagonal leg
				const int d = (targetY < posY) + (targetX < posX) * 2;
				if (!setWalkFrames(kDiagWalk[d][0], kDiagWalk[d][1]) && turnTo(kDiagWalk[d][2]))
					return 1;
				stepX = signOf(targetX - posX) * 6;
				stepY = signOf(targetY - posY) * 6;
				walkState = 1;
			}
		} else {
			// Horizontal leg; on stairs followed by an upward leg it becomes a stair move
			if (type == 1 && walk.zoneType(waypointX, waypointY) == 1 &&
					walk.findNextWaypoint(waypointX, waypointY, tx, ty, nx, ny) &&
					waypointX == nx && ny < g.y) {
				waypointY = ny;
				field_d4 = targetX - posX;
				targetY = ny * 0x400 + 0x200;
				field_d8 = targetY - posY;
				walkStairsUp();
				return 1;
			}
			if (posX <= targetX) {
				if (!setWalkFrames(9, 0x10)) {
					if (frame == 0x35 || frame == 0x45)
						setFrame(0xb);
					else if (frame == 0x39 || frame == 0x49)
						setFrame(0xf);
					else if (turnTo(0xb))
						return 1;
				}
				stepX = 0x10;
			} else {
				if (!setWalkFrames(1, 8)) {
					if (frame == 0x2d || frame == 0x3d)
						setFrame(3);
					else if (frame == 0x31 || frame == 0x41)
						setFrame(7);
					else if (turnTo(3))
						return 1;
				}
				stepX = -0x10;
			}
			walkState = 1;
		}
	}

	if (walkState == 1) {
		// Move by step * zone scale (one pixel per axis in zone type 6)
		const int s = walk.scaleAt(posX >> 8, posY >> 8);
		const Common::Point g = getGridPos();
		const ZoneRecord *zone = walk.zoneAt(g.x, g.y);
		if (!zone || zone->type != 6) {
			posX += s * stepX;
			posY += s * stepY;
		} else {
			posX += signOf(stepX) * 0x100;
			posY += signOf(stepY) * 0x100;
		}
		if (stepX * targetX <= posX * stepX && stepY * targetY <= posY * stepY) {
			posX = targetX;
			posY = targetY;
			walkState = 0;
		}
		stepFrame();
		setPixelPos(posX >> 8, posY >> 8);
	}
	return 1;
}

// FUN_00413c00. Deviation: -1000 outside any zone does nothing (the original
// would set frame -1000).
int Actor::turnTo(int target) {
	const Common::Point g = getGridPos();
	const ZoneRecord *zone = g_engine->_game->_walk.zoneAt(g.x, g.y);
	const int cur = frame;
	if (cur == target || target == -1 || (zone && zone->type == 6))
		return 0;

	if (target == -1000) {
		if (!zone)
			return 0;
		// Automatic final direction after a floor click
		switch (zone->type) {
		case 1:
			if ((cur >= 0x2b && cur <= 0x32) || (cur >= 0x3b && cur <= 0x42) || cur == 0x22)
				target = 0x22;
			else
				target = 0x24;
			break;
		case 3:
		case 4:
		case 7:
		case 8:
			target = cur;
			break;
		default:
			target = 0;
			break;
		}
	}

	const int targetIndex = standIndex(target);
	if (targetIndex != -1) {
		// Map walk, stair and diagonal frames to the stand frame they belong to
		int f = frame;
		if (f >= 1 && f <= 0x20)
			f = kWalkStand[(f - 1) / 8];
		if (f >= 0xae && f <= 0xcd)
			f = kDiagStand[(f - 0xae) / 8];
		if ((f >= 0x2b && f <= 0x32) || (f >= 0x3b && f <= 0x42))
			f = 0x22;
		if ((f >= 0x33 && f <= 0x3a) || (f >= 0x43 && f <= 0x4a))
			f = 0x24;
		if (f == target) {
			setFrame(target);
			return 1;
		}
		if (f >= 0x4c && f <= 0x59) {
			setFrame(0x27);
			return 1;
		}
		const int curIndex = standIndex(f);
		if (curIndex != -1) {
			// One step around the circle, the shorter way (ties: see npcVars[0])
			const int upDist = (targetIndex - curIndex) & 7;
			const int downDist = (curIndex - targetIndex) & 7;
			const bool goDown = npcVars[0] == 0 ? upDist >= downDist : upDist > downDist;
			setFrame(kStandFrames[(curIndex + (goDown ? -1 : 1)) & 7]);
			return 1;
		}
		const TurnAnim *anim = findTurnAnim(f);
		if (anim) {
			// Play a transition animation back to its start, then jump to its stand frame
			if (f != anim->first && anim->flag == 0)
				setFrame(f - 1);
			else
				setFrame(anim->target);
			return 1;
		}
	}

	switch (target) {
	case 3:
		if (turnTo(0x22))
			return 1;
		break;
	case 0xb:
		if (turnTo(0x24))
			return 1;
		break;
	case 0x14:
		if (turnTo(0x27))
			return 1;
		break;
	case 0x1c:
		if (turnTo(0))
			return 1;
		break;
	case 0xb0:
		if (turnTo(0x21))
			return 1;
		break;
	case 0xb8:
		if (turnTo(0x23))
			return 1;
		break;
	case 0xc0:
		if (turnTo(0x25))
			return 1;
		break;
	case 0xc8:
		if (turnTo(0x26))
			return 1;
		break;
	default: {
		// Target inside a transition animation: reach its stand frame, then play it
		const TurnAnim *anim = findTurnAnim(target);
		if (anim) {
			const int f = frame;
			if (anim->first <= f && f <= anim->last)
				setFrame(f < target ? f + 1 : f - 1);
			else if (f == anim->target)
				setFrame(anim->first);
			else
				turnTo(anim->target);
			return 1;
		}
		break;
	}
	}
	setFrame(target);
	return 1;
}

// FUN_00413010.
bool Actor::setWalkFrames(int first, int last) {
	idleFrameMin = first;
	idleFrameMax = last;
	return first <= frame && frame <= last;
}

// FUN_00413040: stair cycle base..base+7 with the stand frames base+2 / base+6.
int Actor::setStairFrames(int base, int walkBase, int turnFrame) {
	idleFrameMin = base;
	idleFrameMax = base + 7;
	const int f = frame;
	if (f == base + 2 || f == base + 6)
		return 0;
	if (walkBase <= f && f <= walkBase + 3) {
		setFrame(base + 2);
		return 0;
	}
	if (walkBase + 4 <= f && f <= walkBase + 7) {
		setFrame(base + 6);
		return 0;
	}
	if (turnTo(turnFrame))
		return 1;
	setFrame(base + 2);
	return 1;
}

// Footstep of FUN_00412ed0 / FUN_00412f70: zone sound, 0 = default 0xd49, -1 = none.
void Actor::playFootstep() {
	const Common::Point g = getGridPos();
	const ZoneRecord *zone = g_engine->_game->_walk.zoneAt(g.x, g.y);
	if (!zone || zone->footstepSfx == -1)
		return;
	g_engine->_soundMan->play(nullptr, zone->footstepSfx ? zone->footstepSfx : 0xd49);
}

// FUN_00412ed0: C remainder, as in the original (negative differences never give 3).
void Actor::stepFrame() {
	if ((frame - idleFrameMin) % 4 == 3)
		playFootstep();
	if (idleFrameMax <= frame)
		setFrame(idleFrameMin);
	else
		setFrame(frame + 1);
}

// FUN_00412f70.
void Actor::stepFrameBack() {
	if ((frame - idleFrameMin) % 4 == 3)
		playFootstep();
	if (frame <= idleFrameMin)
		setFrame(idleFrameMax);
	else
		setFrame(frame - 1);
}

// FUN_00413190: stairs upwards, left (0x2b..) or right (0x33..) cycle.
void Actor::walkStairsUp() {
	if (walkState != 2) {
		int turning;
		if (field_d4 < 1) {
			npcVars[0] = 1;
			turning = setStairFrames(0x2b, 1, 0x22);
		} else {
			npcVars[0] = 0;
			turning = setStairFrames(0x33, 9, 0x24);
		}
		if (turning)
			return;
		walkState = 2;
	}
	stairStep(4);
}

// FUN_00413200: stairs sideways, left (0x3b..) or right (0x43..) cycle.
void Actor::walkStairsSide() {
	if (walkState != 3) {
		int turning;
		if (field_d4 < 1) {
			npcVars[0] = 1;
			turning = setStairFrames(0x3b, 1, 0x22);
		} else {
			npcVars[0] = 0;
			turning = setStairFrames(0x43, 9, 0x24);
		}
		if (turning)
			return;
		walkState = 3;
	}
	stairStep(1);
}

// FUN_004130e0: x follows the frame phase 1..4 of the cycle; y jumps at phase `yPhase`.
void Actor::stairStep(int yPhase) {
	stepFrame();
	int phase = (frame - 0x29) % 4;
	if (phase == 0)
		phase = 4;
	posX = targetX - field_d4 * (4 - phase) / 4;
	if (yPhase <= phase)
		posY = targetY;
	if (phase == 4) {
		posX = targetX;
		posY = targetY;
		walkState = 0;
	}
	setPixelPos(posX >> 8, posY >> 8);
}

// FUN_00413270: back view (0x27), then the climbing cycle 0x54..0x57; up moves
// after the cycle, down moves first and plays the cycle backwards.
void Actor::climbLadder() {
	if (walkState != 4) {
		idleFrameMin = 0x54;
		idleFrameMax = 0x57;
		if (frame != 0x54) {
			if (turnTo(0x27))
				return;
			setFrame(0x54);
			return;
		}
		walkState = 4;
	}
	if (targetY < posY) {
		stepFrame();
	} else {
		stepFrameBack();
		posY = targetY;
		setPixelPos(posX >> 8, targetY >> 8);
	}
	if (frame == 0x54) {
		posY = targetY;
		walkState = 0;
		setPixelPos(posX >> 8, targetY >> 8);
	}
}

// FUN_00413340: front view (0), then the cycle 0x19..; ends on frame 0x1c.
// Going up, the original adds 300 to the target every tick; kept as is.
void Actor::climbLadder2() {
	if (walkState != 6) {
		idleFrameMin = 0x19;
		idleFrameMax = 0x20;
		if (frame != 0x19) {
			if (turnTo(0))
				return;
			setFrame(0x19);
			return;
		}
		walkState = 6;
	}
	if (targetY < posY) {
		targetY += 300;
		stepFrame();
	} else {
		stepFrameBack();
		posY = targetY;
		setPixelPos(posX >> 8, targetY >> 8);
	}
	if (frame == 0x1c) {
		posY = targetY;
		walkState = 0;
		setPixelPos(posX >> 8, targetY >> 8);
	}
}

// FUN_0040df50 + the layout part of FUN_0040dfb0 + clamp FUN_00408af0 to (0, 0, 640, 400).
void Actor::placeSubtitle() {
	const Common::Point size = getSize();
	const int ax = size.x / 2 + x;
	int ay = y - 16;
	const Common::Point text = subtitle.getSize();
	if (ay + text.y >= kPlayAreaHeight)
		ay = kPlayAreaHeight - text.y;
	if (ay < 0)
		ay = 0;
	subtitle.y = ay - text.y;
	subtitle.x = ax - text.x / 2;
	subtitle.priority = 1000;
	subtitle.subPriority = 0;

	if (subtitle.x + text.x >= kScreenWidth)
		subtitle.x = kScreenWidth - text.x;
	if (subtitle.x < 0)
		subtitle.x = 0;
	if (subtitle.y + text.y >= kPlayAreaHeight)
		subtitle.y = kPlayAreaHeight - text.y;
	if (subtitle.y < 0)
		subtitle.y = 0;
}

// FUN_0040dfb0. A line starts when kActorTalking is set: the hero first turns to
// `restFrame` (vtable 0x10 = walkTo(-1, -1, restFrame)), an NPC just takes it
// (FUN_0040df90). The subtitle is always white (the +0x50 field is not read).
// A left-button press ends the line on the next call.
int Actor::say(int lineId, int restFrame, int talkBase, int talkCount) {
	if (lineId == 0)
		return 0;
	Game *game = g_engine->_game;
	SoundManager *sound = g_engine->_soundMan;
	const bool clicked = leftClickEdge();
	game->flagClear(lineId);

	if (flags & kActorTalking) {
		if (frame != restFrame && isVisible()) {
			if (isHero())
				walkTo(-1, -1, restFrame);
			else
				setFrame(restFrame);
			return 1;
		}
		flags &= ~kActorTalking;
		const Common::String text = g_engine->_res->getText(lineId);
		// FUN_00409e70(text, 0, 0xffffff) + FUN_00409e30(0x10000000): black text in a white box
		subtitle.setText(text, kFontBold, 0x000000, 0xFFFFFF, kTextBoxed);
		placeSubtitle();
		if (game->_subtitles)
			subtitle.show();
		speechTimer = 0;
		if (g_engine->_res->has(kResWave, lineId))
			sound->play(&voiceHandle, lineId, 0, false, 0, 0, Audio::Mixer::kSpeechSoundType);
		else
			speechTimer = text.size() * 2 + 12;
		debugC(2, kDebugScript, "Actor: says line %d (timer %d)", lineId, speechTimer);
	}

	// Talk animation every 4th tick
	if ((g_engine->_tickCounter & 3) == 0) {
		int f;
		if (talkCount < 2) {
			f = (talkCount != 0 && (g_engine->_tickCounter & 4)) ? talkBase : restFrame;
		} else {
			const int r = game->rnd(talkCount + 1);
			f = r == talkCount ? restFrame : r + talkBase;
			if (f == frame)
				f = game->rnd(talkCount) + talkBase;
		}
		setFrame(f);
	}

	if (speechTimer != 0)
		speechTimer--;
	if (!sound->isPlaying(&voiceHandle) && speechTimer == 0) {
		subtitle.hide();
		setFrame(restFrame);
		return 0;
	}
	if (clicked) {
		if (sound->isPlaying(&voiceHandle))
			sound->stop(&voiceHandle);
		speechTimer = 0;
	}
	return 1;
}

// Vtable slot 0xc: FUN_0040e280, plus FUN_00412eb0 for the hero.
void Actor::stopTalking() {
	flags &= ~kActorTalking;
	if (isHero()) {
		walkState = 0;
		npcVars[0] = 0;
	}
}

// FUN_0040e250.
bool Actor::isTalking() const {
	if (flags & kActorTalking)
		return true;
	return g_engine->_soundMan->isPlaying(&voiceHandle) || speechTimer != 0;
}

// No per-tick actor work outside walkTo()/say() in the original (see actor.h).
void Actor::update() {
}

// The table scan of FUN_0042d190 (5 entries).
bool Actor::heroTalkFrames(int frame, int &restFrame, int &talkBase, int &talkCount) {
	for (int i = 0; i < 5; i++) {
		if (frame == kHeroTalk[i][0] ||
				(kHeroTalk[i][1] <= frame && frame < kHeroTalk[i][1] + kHeroTalk[i][2])) {
			restFrame = kHeroTalk[i][0];
			talkBase = kHeroTalk[i][1];
			talkCount = kHeroTalk[i][2];
			return true;
		}
	}
	restFrame = frame;
	talkBase = frame;
	talkCount = 1;
	return false;
}

} // End of namespace Gooka
