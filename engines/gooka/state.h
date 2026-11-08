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
 * Persistent game state that is written to save games: flags,
 * room-private variables and the random table.
 */

#ifndef GOOKA_STATE_H
#define GOOKA_STATE_H

#include "common/array.h"
#include "common/serializer.h"

namespace Gooka {

/**
 * Growable bit set with a default value for bits that were never written
 * (original helpers FUN_00403930 test / FUN_004037e0 set).
 */
class FlagSet {
public:
	explicit FlagSet(bool defaultValue) : _default(defaultValue) {}

	/** Value of bit `index`; the default for bits beyond the stored range. */
	bool get(uint32 index) const;
	/** Sets bit `index`, growing the storage as needed. */
	void set(uint32 index, bool value);
	/** Forgets all bits (every bit reads as the default again). */
	void clear() { _bits.clear(); }

	/** Saves or loads the bits. */
	void sync(Common::Serializer &s);

private:
	Common::Array<uint32> _bits;
	bool _default;
};

/** Number of ints in the room-private variable block (0x8000 bytes). */
static const int kNumVars = 0x2000;
/** Number of entries in the saved random table. */
static const int kNumRandomEntries = 80;

/** One entry of the saved random table (DAT_0047c5e8, filled by FUN_00429c10). */
struct RandomEntry {
	int16 x, y;
	byte kind;
};

/**
 * All state that survives room changes and is saved.
 * Flags are keyed by object/hotspot/room/line IDs: positive IDs default to 1
 * ("object present, line not said yet"), negative IDs default to 0.
 */
class GameState {
public:
	GameState();

	/** flagTest (FUN_0040e770). */
	bool flagTest(int id) const;
	/** flagSet (FUN_0040e7c0). */
	void flagSet(int id, bool value);

	/** Room-private persistent int `index` (FUN_0040e820). */
	int &var(int index);

	/** Resets everything for a new game. */
	void reset();

	/** Saves or loads flags, variables and the random table. */
	void sync(Common::Serializer &s);

	RandomEntry randomTable[kNumRandomEntries];

private:
	FlagSet _positive; ///< IDs > 0 (DAT_004692a0)
	FlagSet _negative; ///< IDs <= 0, keyed by -id (DAT_0044d4b8)
	int _vars[kNumVars]; ///< DAT_004696f8
};

} // End of namespace Gooka

#endif // GOOKA_STATE_H
