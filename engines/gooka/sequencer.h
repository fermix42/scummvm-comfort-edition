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
 * The action-sequence interpreter (original Sequencer object DAT_00449598,
 * interpreter FUN_0040d390). Room code builds short programs of primitive
 * calls (walk, say, animate, ...) that run over several ticks.
 */

#ifndef GOOKA_SEQUENCER_H
#define GOOKA_SEQUENCER_H

#include <initializer_list>

#include "common/array.h"
#include "common/ptr.h"

namespace Gooka {

class Actor;
class Sprite;

/** Instruction opcodes (upper 16 bits of the original tag dword). */
enum SeqOp {
	kOpRet = 1,          ///< return from GOSUB; ends the sequence at top level
	kOpYield = 2,        ///< stop for this tick
	kOpCall = 3,         ///< run a primitive until it returns 0
	kOpIf = 4,           ///< skip to ELSE/ENDIF when the predicate returns 0
	kOpElse = 5,
	kOpEndIf = 6,
	kOpWhile = 7,        ///< loop while the predicate returns non-zero
	kOpEndWhile = 8,
	kOpBreak = 9,        ///< leave the loop when the predicate returns non-zero
	kOpGosub = 0xa,      ///< unused by the game
	kOpSpawn = 0xb,      ///< run the block up to the matching JOIN in parallel
	kOpJoin = 0xc,       ///< end of a spawned block
	kOpKillChildren = 0xd,
	kOpWaitChildren = 0xe, ///< wait until all spawned blocks finished
	kOpEnd = 0xf
};

/** Primitives (original C functions 0x42c680..0x42d4a0, see GAMEEXE.md 6.3). */
enum SeqPrim {
	kPrimNone = 0,          ///< no function: predicates count as true
	kPrimIfFlag,            ///< 0x42c680 (flag): flag set
	kPrimIfNotFlag,         ///< 0x42c6a0 (flag): flag clear
	kPrimIfItem,            ///< 0x42c6c0 (item): item in inventory
	kPrimIfNotItem,         ///< 0x42c6e0 (item): item not in inventory
	kPrimWalk,              ///< 0x42c700 (x | y << 16, dir): hero walks, then faces dir
	kPrimFace,              ///< 0x42c730 (dir): hero turns
	kPrimAnimate,           ///< 0x42c770 (spr, frame, step): step frames towards `frame`
	kPrimMoveAxis,          ///< 0x42c7e0 (spr, axis, target, delta, fA, fB, step)
	kPrimMoveAccel,         ///< 0x42c910 (spr, targetY, v0, accel, fA, fB, step)
	kPrimMoveVelocity,      ///< 0x42cab0 (spr, ticks, vx, ax, vy, ay, step)
	kPrimMoveLinear,        ///< 0x42cc00 (spr, ticks, dx, everyX, dy, everyY)
	kPrimSay,               ///< 0x42cc90 (actor, line, restFrame, talkBase, talkCount)
	kPrimWait,              ///< 0x42ccc0 (ticks)
	kPrimShow,              ///< 0x42cce0 (spr)
	kPrimHide,              ///< 0x42cd00 (spr)
	kPrimSetFrame,          ///< 0x42cd20 (spr, frame)
	kPrimShowAt,            ///< 0x42cd40 (spr, x | y << 16, prio | sub << 16)
	kPrimShowRelative,      ///< 0x42cd90 (spr, refSpr, dx, dy, dprio, dsub)
	kPrimShowPriority,      ///< 0x42ce00 (spr, prio | sub << 16)
	kPrimPlaceActor,        ///< 0x42ce40 (actor, gx | gy << 16, prio | frame << 16)
	kPrimPlaceHero,         ///< 0x42cea0 (gx, gy, frame)
	kPrimSound,             ///< 0x42ced0 (wav)
	kPrimSoundWait,         ///< 0x42cf40 (wav, optional subtitle id): play on the shared handle and wait
	kPrimWaitHandle,        ///< 0x42cf80 (int *handle)
	kPrimAmbient,           ///< 0x42cfa0 (wav): looping sound on the shared handle
	kPrimAmbientStop,       ///< 0x42cfd0 ()
	kPrimMusicPause,        ///< 0x42cff0 ()
	kPrimMusicResume,       ///< 0x42d000 ()
	kPrimMusicFade,         ///< 0x42d010 (target, step, every)
	kPrimMusicVolume,       ///< 0x42d080 (volume)
	kPrimHideExit,          ///< 0x42d0a0 (exitId)
	kPrimShowExit,          ///< 0x42d0c0 (exitId)
	kPrimAddItem,           ///< 0x42d0e0 (item): add + clear its flag
	kPrimRemoveItem,        ///< 0x42d110 (item)
	kPrimReplaceItem,       ///< 0x42d130 (old, new): with sparkle
	kPrimReplaceItemQuiet,  ///< 0x42d150 (old, new)
	kPrimChangeRoom,        ///< 0x42d170 (room)
	kPrimDialogue,          ///< 0x42d190 (npc, +-firstLine, lastLine, restFrame, talkBase, talkCount)
	kPrimClearBusy,         ///< 0x42d420 ()
	kPrimActOnHotspot,      ///< 0x42d430 (hotspot)
	kPrimRaiseEvent,        ///< 0x42d450 (event, param)
	kPrimDropHeld,          ///< 0x42d470 ()
	kPrimClearFlag,         ///< 0x42d480 (flag)
	kPrimSetFlag            ///< 0x42d4a0 (flag)
};

/** One argument: an integer or a pointer to a sprite, actor or sound handle. */
struct SeqArg {
	int32 value;
	void *ptr;

	SeqArg(int v) : value(v), ptr(nullptr) {}
	SeqArg(Sprite *s) : value(0), ptr(s) {}
	SeqArg(Actor *a);
	SeqArg(int *handle) : value(0), ptr(handle) {}

	/** The argument as sprite (actors are stored as their Sprite base). */
	Sprite *sprite() const { return (Sprite *)ptr; }
	/** The argument as actor; only valid when an Actor was passed. */
	Actor *actor() const;
	/** The argument as pointer to a sound handle. */
	int *handle() const { return (int *)ptr; }
};

/** One instruction of a sequence program. */
struct SeqInstr {
	SeqOp op;
	SeqPrim prim;
	Common::Array<SeqArg> args;
};

/** A complete program; always ends with kOpEnd. */
typedef Common::Array<SeqInstr> SeqProgram;

/** @name Program builders used by the room code */
///@{
/** Packs a coordinate pair like the original (x | y << 16). */
inline int xy(int x, int y) { return (x & 0xFFFF) | (y << 16); }
SeqInstr seqCall(SeqPrim prim, std::initializer_list<SeqArg> args = {});
SeqInstr seqIf(SeqPrim prim, std::initializer_list<SeqArg> args = {});
SeqInstr seqElse();
SeqInstr seqEndIf();
SeqInstr seqWhile(SeqPrim prim, std::initializer_list<SeqArg> args = {});
SeqInstr seqEndWhile();
SeqInstr seqBreak(SeqPrim prim, std::initializer_list<SeqArg> args = {});
SeqInstr seqYield();
SeqInstr seqSpawn();
SeqInstr seqJoin();
SeqInstr seqKillChildren();
SeqInstr seqWaitChildren();
SeqInstr seqEnd();
///@}

class Sequencer;

/** Runs one primitive for a sequencer; returns non-zero while it is still busy. */
typedef int (*SeqPrimitiveRunner)(Sequencer &seq);

/**
 * Executes a program over several ticks. Spawned blocks run in child
 * sequencers that share the program and are stepped before their parent.
 */
class Sequencer {
public:
	Sequencer();
	~Sequencer();

	/** Starts a new program, dropping the old one and all children (FUN_0040d730). */
	void load(const SeqProgram &program);
	/** Stops immediately. */
	void stop();
	/** Runs one tick (FUN_0040d390). */
	void run();
	/** True while a program is loaded and not finished (original IP != 0). */
	bool isRunning() const { return _ip >= 0; }

	/** @name State available to primitives */
	///@{
	/** Ticks the current instruction has been running (0 on the first call). */
	int counter() const { return _counter; }
	/** Argument `i` of the current instruction. */
	const SeqArg &arg(uint i) const;
	/** Primitive of the current instruction. */
	SeqPrim currentPrim() const;
	/** Per-sequencer scratch variable (grown on demand, starts at 0). */
	int &scratch(uint i);
	///@}

	/** Executes the primitive of the current instruction (set by the engine). */
	static SeqPrimitiveRunner _runner;

private:
	/** Entry of the loop/GOSUB stack. */
	struct Frame {
		SeqOp op;
		int value; ///< instruction index
	};

	/** Shares the program with the child sequencer and starts it at `ip`. */
	void startAt(const Common::SharedPtr<SeqProgram> &program, int ip);
	/** Loads the instruction at the IP and advances it (FUN_0040d2b0). */
	void fetch();
	/** Calls the current primitive; kPrimNone counts as 1 (FUN_0040d2f0). */
	int callPrimitive();
	/** Skips forward until the matching end opcode (FUN_0040d310). */
	void skipTo(SeqOp open, SeqOp close1, SeqOp close2);
	/** Deletes all children (FUN_0040d350). */
	void killChildren();
	/** Steps the children and removes the finished ones. */
	void runChildren();

	Common::SharedPtr<SeqProgram> _program;
	int _ip;        ///< next instruction, -1 = not running
	int _cur;       ///< current instruction
	SeqOp _op;      ///< opcode of the current instruction (0 before the first fetch)
	int _counter;   ///< +0x40
	Common::Array<Frame> _stack;
	Common::Array<int> _scratch;
	Common::Array<Sequencer *> _children;
};

} // End of namespace Gooka

#endif // GOOKA_SEQUENCER_H
