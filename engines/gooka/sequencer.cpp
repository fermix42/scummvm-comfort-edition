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
 * Action-sequence interpreter, a faithful port of FUN_0040d390 and helpers
 * (see sequencer.h).
 */

#include "common/textconsole.h"

#include "gooka/actor.h"
#include "gooka/sequencer.h"

namespace Gooka {

SeqPrimitiveRunner Sequencer::_runner = nullptr;

SeqArg::SeqArg(Actor *a) : value(0), ptr(static_cast<Sprite *>(a)) {
}

Actor *SeqArg::actor() const {
	return static_cast<Actor *>(sprite());
}

// ---------------------------------------------------------------------------
// Program builders
// ---------------------------------------------------------------------------

/** Builds an instruction from its parts. */
static SeqInstr makeInstr(SeqOp op, SeqPrim prim, std::initializer_list<SeqArg> args) {
	SeqInstr instr;
	instr.op = op;
	instr.prim = prim;
	for (const SeqArg &a : args)
		instr.args.push_back(a);
	return instr;
}

SeqInstr seqCall(SeqPrim prim, std::initializer_list<SeqArg> args) { return makeInstr(kOpCall, prim, args); }
SeqInstr seqIf(SeqPrim prim, std::initializer_list<SeqArg> args) { return makeInstr(kOpIf, prim, args); }
SeqInstr seqElse() { return makeInstr(kOpElse, kPrimNone, {}); }
SeqInstr seqEndIf() { return makeInstr(kOpEndIf, kPrimNone, {}); }
SeqInstr seqWhile(SeqPrim prim, std::initializer_list<SeqArg> args) { return makeInstr(kOpWhile, prim, args); }
SeqInstr seqEndWhile() { return makeInstr(kOpEndWhile, kPrimNone, {}); }
SeqInstr seqBreak(SeqPrim prim, std::initializer_list<SeqArg> args) { return makeInstr(kOpBreak, prim, args); }
SeqInstr seqYield() { return makeInstr(kOpYield, kPrimNone, {}); }
SeqInstr seqSpawn() { return makeInstr(kOpSpawn, kPrimNone, {}); }
SeqInstr seqJoin() { return makeInstr(kOpJoin, kPrimNone, {}); }
SeqInstr seqKillChildren() { return makeInstr(kOpKillChildren, kPrimNone, {}); }
SeqInstr seqWaitChildren() { return makeInstr(kOpWaitChildren, kPrimNone, {}); }
SeqInstr seqEnd() { return makeInstr(kOpEnd, kPrimNone, {}); }

// ---------------------------------------------------------------------------
// Sequencer
// ---------------------------------------------------------------------------

Sequencer::Sequencer() : _ip(-1), _cur(-1), _op((SeqOp)0), _counter(0) {
}

Sequencer::~Sequencer() {
	killChildren();
}

// Copies the program and appends an END so a missing terminator cannot run off the end.
void Sequencer::load(const SeqProgram &program) {
	Common::SharedPtr<SeqProgram> copy(new SeqProgram(program));
	if (copy->empty() || copy->back().op != kOpEnd)
		copy->push_back(seqEnd());
	_stack.clear();
	_scratch.clear();
	startAt(copy, 0);
}

// FUN_0040d700: new IP, no current opcode, children dropped.
void Sequencer::startAt(const Common::SharedPtr<SeqProgram> &program, int ip) {
	_program = program;
	_ip = ip;
	_cur = -1;
	_op = (SeqOp)0;
	_counter = 0;
	killChildren();
}

void Sequencer::stop() {
	_ip = -1;
	_op = (SeqOp)0;
	_stack.clear();
	killChildren();
}

const SeqArg &Sequencer::arg(uint i) const {
	static const SeqArg zero(0);
	if (_cur < 0 || !_program || i >= (*_program)[_cur].args.size())
		return zero;
	return (*_program)[_cur].args[i];
}

SeqPrim Sequencer::currentPrim() const {
	if (_cur < 0 || !_program)
		return kPrimNone;
	return (*_program)[_cur].prim;
}

int &Sequencer::scratch(uint i) {
	if (i >= _scratch.size())
		_scratch.resize(i + 1, 0);
	return _scratch[i];
}

// FUN_0040d2b0: the instruction at the IP becomes current and the IP moves on.
void Sequencer::fetch() {
	if (!_program || _ip < 0 || _ip >= (int)_program->size()) {
		_cur = -1;
		_op = kOpEnd;
		_counter = 0;
		return;
	}
	_cur = _ip;
	_op = (*_program)[_cur].op;
	_counter = 0;
	_ip++;
}

int Sequencer::callPrimitive() {
	if (_cur < 0 || (*_program)[_cur].prim == kPrimNone || !_runner)
		return 1;
	return _runner(*this);
}

// Nesting-aware skip: `open` increases the depth, `close1`/`close2` decrease it.
void Sequencer::skipTo(SeqOp open, SeqOp close1, SeqOp close2) {
	int depth = 1;
	do {
		fetch();
		if (_op == open)
			depth++;
		if (_op == close1 || _op == close2)
			depth--;
		if (_cur < 0)
			return; // ran off the program
	} while (depth != 0);
}

void Sequencer::killChildren() {
	for (uint i = 0; i < _children.size(); i++)
		delete _children[i];
	_children.clear();
}

// Finished children are removed on the next pass, like the original.
void Sequencer::runChildren() {
	uint i = 0;
	while (i < _children.size()) {
		Sequencer *child = _children[i];
		if (!child->isRunning()) {
			delete child;
			_children.remove_at(i);
		} else {
			i++;
			child->run();
		}
	}
}

// One tick of FUN_0040d390.
void Sequencer::run() {
	runChildren();
	if (_ip < 0)
		return;

	while (_op != kOpWaitChildren || _children.empty()) {
		if (_op == kOpCall && callPrimitive() != 0) {
			_counter++;
			return;
		}

		fetch();
		bool leave = false;

		switch (_op) {
		case kOpRet:
		case kOpJoin:
		case kOpEnd:
			leave = true;
			break;

		case kOpYield:
			return;

		case kOpIf:
			if (callPrimitive() == 0)
				skipTo(kOpIf, kOpElse, kOpEndIf);
			break;

		case kOpElse:
			skipTo(kOpIf, kOpEndIf, (SeqOp)-1);
			break;

		case kOpWhile:
			if (callPrimitive() == 0) {
				skipTo(kOpWhile, kOpEndWhile, (SeqOp)-1);
			} else {
				Frame f = { kOpWhile, _cur };
				_stack.push_back(f);
			}
			break;

		case kOpEndWhile:
			if (callPrimitive() != 0 && !_stack.empty()) {
				_ip = _stack.back().value;
				_stack.pop_back();
			}
			break;

		case kOpBreak:
			if (callPrimitive() != 0)
				skipTo(kOpWhile, kOpEndWhile, (SeqOp)-1);
			break;

		case kOpGosub:
			warning("Gooka: GOSUB is not supported by the sequencer");
			break;

		case kOpSpawn: {
			// The child runs the block after SPAWN until its JOIN; the parent skips it
			Sequencer *child = new Sequencer();
			_children.push_back(child);
			child->startAt(_program, _ip);
			child->run();
			skipTo(kOpSpawn, kOpJoin, (SeqOp)-1);
			break;
		}

		case kOpKillChildren:
			killChildren();
			break;

		default:
			break;
		}

		if (leave) {
			// Return to the last GOSUB frame; with none left the sequence ends
			for (;;) {
				if (_stack.empty()) {
					_ip = -1;
					killChildren();
					return;
				}
				const Frame f = _stack.back();
				_stack.pop_back();
				if (f.op == kOpGosub) {
					_ip = f.value;
					break;
				}
			}
		}

		if (_ip < 0)
			return;
	}
}

} // End of namespace Gooka
