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
 * Shared room templates FUN_00419b70 ... FUN_00419dd0: the corridor maze
 * inside the crashed spaceship (rooms 2626-2658). Each template owns one
 * static descriptor (background 2603 ... 2608) that the original rewrites
 * with the hotspot/exit ids of the calling room on every call; here every
 * room gets its own copy of the descriptor arrays instead.
 */

#ifndef GOOKA_ROOMS_ROOM_TEMPLATE_419_H
#define GOOKA_ROOMS_ROOM_TEMPLATE_419_H

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Common base of the six corridor templates: a private copy of the
 * template descriptor whose hotspot slots and exit ids are filled in by
 * the derived template class (the writes at the start of every template).
 */
class RoomTemplate419 : public Room {
public:
	/** The room's own copy of the template descriptor (`what == 1`). */
	const RoomDescriptor *getDescriptor() override;
	/** Templates FUN_00419b70 ... FUN_00419d70 handle no events. */
	int handleEvent(int event, int param) override;

protected:
	enum {
		kNumHotspotSlots = 12, ///< dwords between the hotspot table and the exit records of every template
		kMaxExits = 5          ///< at most four exits plus the terminating record
	};

	/**
	 * Copies the static template descriptor.
	 * @param id       room id (`what == 0`)
	 * @param layout   template descriptor; hotspots/exits hold the placeholders of GAME.EXE
	 */
	RoomTemplate419(int id, const RoomDescriptor &layout);

	/** Writes hotspot slot `slot` (0-based) as `id & 0xffff | cursor << 16`. */
	void setHotspot(int slot, int id, int cursor);
	/** Sets the id (= destination room) of exit record `index`. */
	void setExitId(int index, int id);

	RoomDescriptor _desc;                ///< descriptor returned for `what == 1`
	int _hotspots[kNumHotspotSlots];     ///< hotspot table, indexed by hotspot-map slot - 1
	ExitRecord _exits[kMaxExits];        ///< exit records, terminated by id 0
};

/**
 * FUN_00419b70 (descriptor 0x440dc8, background 2603): crossing with four
 * exits (bottom, left, far end, right) and the cursor types 4, 1, 2, 3.
 */
class RoomTemplate419b70 : public RoomTemplate419 {
protected:
	/** Hotspot slots 1-4 = exit records 0-3 = back, left, ahead, right. */
	RoomTemplate419b70(int id, int back, int left, int ahead, int right);
};

/**
 * FUN_00419c00 (descriptor 0x440fe8, background 2604): crossing with four
 * exits (bottom, left, far end, right) and the cursor types 4, 1, 2, 3.
 */
class RoomTemplate419c00 : public RoomTemplate419 {
protected:
	/** Hotspot slots 1-4 = exit records 0-3 = back, left, ahead, right. */
	RoomTemplate419c00(int id, int back, int left, int ahead, int right);
};

/**
 * FUN_00419c90 (descriptor 0x4411a0, background 2605): T-junction with
 * three exits (bottom, left, right) and the cursor types 4, 1, 3.
 */
class RoomTemplate419c90 : public RoomTemplate419 {
protected:
	/** Hotspot slots 1-3 = exit records 0-2 = back, left, right. */
	RoomTemplate419c90(int id, int back, int left, int right);
};

/**
 * FUN_00419d10 (descriptor 0x4412f8, background 2606): corridor bending
 * left, `back` (cursor 4, exit record 1) and `left` (cursor 1, exit record 0).
 */
class RoomTemplate419d10 : public RoomTemplate419 {
protected:
	/** Hotspot slot 1 = `back`, slot 2 = `left`. */
	RoomTemplate419d10(int id, int back, int left);
};

/**
 * FUN_00419d70 (descriptor 0x441450, background 2607): corridor bending
 * right, `back` (cursor 4, exit record 1) and `right` (cursor 3, exit record 0).
 */
class RoomTemplate419d70 : public RoomTemplate419 {
protected:
	/** Hotspot slot 1 = `back`, slot 2 = `right`. */
	RoomTemplate419d70(int id, int back, int right);
};

/**
 * FUN_00419dd0 (descriptor 0x4415c0, background 2608, sheet 2609): lift
 * landing with two exits (cursor types 1 and 3) and the lift button
 * (hotspot 0xa32 = room 2610, slot 3). The sliding lift door closes behind
 * the hero when he arrives from the lift panel (room 2610) and opens when
 * the button is used, taking him back to the panel.
 */
class RoomTemplate419dd0 : public RoomTemplate419 {
public:
	/** Enter: place the lift door (arriving from 2610: it closes); click on 0xa32: ride the lift. */
	int handleEvent(int event, int param) override;

protected:
	/** Hotspot slot 1 = `left` (exit record 0), slot 2 = `right` (exit record 1). */
	RoomTemplate419dd0(int id, int left, int right);

private:
	Sprite _s476d48; ///< lift door (static 0x476d48, shared by all rooms of the template in the original)
};

} // End of namespace Gooka

#endif // GOOKA_ROOMS_ROOM_TEMPLATE_419_H
