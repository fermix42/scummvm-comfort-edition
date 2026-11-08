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
 * Flags, variables and the random table (see state.h).
 */

#include "gooka/state.h"

namespace Gooka {

// ---------------------------------------------------------------------------
// FlagSet
// ---------------------------------------------------------------------------

bool FlagSet::get(uint32 index) const {
	const uint32 word = index >> 5;
	if (word >= _bits.size())
		return _default;
	return (_bits[word] >> (index & 31)) & 1;
}

// New words are filled with the default so untouched bits keep reading it.
void FlagSet::set(uint32 index, bool value) {
	const uint32 word = index >> 5;
	if (word >= _bits.size())
		_bits.resize(word + 1, _default ? 0xFFFFFFFF : 0);
	if (value)
		_bits[word] |= 1u << (index & 31);
	else
		_bits[word] &= ~(1u << (index & 31));
}

void FlagSet::sync(Common::Serializer &s) {
	uint32 count = _bits.size();
	s.syncAsUint32LE(count);
	if (s.isLoading())
		_bits.resize(count);
	for (uint32 i = 0; i < count; i++)
		s.syncAsUint32LE(_bits[i]);
}

// ---------------------------------------------------------------------------
// GameState
// ---------------------------------------------------------------------------

GameState::GameState() : _positive(true), _negative(false) {
	reset();
}

bool GameState::flagTest(int id) const {
	if (id > 0)
		return _positive.get(id);
	return _negative.get(-id);
}

void GameState::flagSet(int id, bool value) {
	if (id > 0)
		_positive.set(id, value);
	else
		_negative.set(-id, value);
}

int &GameState::var(int index) {
	assert(index >= 0 && index < kNumVars);
	return _vars[index];
}

void GameState::reset() {
	_positive.clear();
	_negative.clear();
	memset(_vars, 0, sizeof(_vars));
	memset(randomTable, 0, sizeof(randomTable));
}

void GameState::sync(Common::Serializer &s) {
	_positive.sync(s);
	_negative.sync(s);
	for (int i = 0; i < kNumVars; i++)
		s.syncAsSint32LE(_vars[i]);
	for (int i = 0; i < kNumRandomEntries; i++) {
		s.syncAsSint16LE(randomTable[i].x);
		s.syncAsSint16LE(randomTable[i].y);
		s.syncAsByte(randomTable[i].kind);
	}
}

} // End of namespace Gooka
