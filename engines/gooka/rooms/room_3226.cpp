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
 * Room3226: port of the room handler FUN_00429cc0.
 */

#include "gooka/rooms/room_common.h"

namespace Gooka {

/**
 * Solved positions of the 80 pieces (table 0x444c40, x/y pairs). The room
 * only uses them modulo 6, as the phase of the 6-pixel snapping grid, and
 * reduces each entry in place the first time its piece is dropped.
 */
static const int kRoom3226Targets[80][2] = {
	{ 397, 154 }, { 381, 308 }, { 430, 274 }, { 394, 232 }, { 426, 244 }, { 396, 277 }, { 347, 303 }, { 451, 210 },
	{ 366, 240 }, { 352, 269 }, { 411, 127 }, { 298, 339 }, { 419, 81 }, { 350, 284 }, { 188, 236 }, { 165, 263 },
	{ 354, 134 }, { 324, 344 }, { 315, 314 }, { 422, 225 }, { 330, 215 }, { 325, 120 }, { 437, 135 }, { 365, 190 },
	{ 252, 208 }, { 311, 248 }, { 450, 162 }, { 211, 237 }, { 413, 186 }, { 436, 98 }, { 226, 198 }, { 162, 102 },
	{ 175, 76 }, { 199, 113 }, { 376, 112 }, { 159, 235 }, { 200, 145 }, { 247, 137 }, { 280, 158 }, { 252, 30 },
	{ 145, 181 }, { 181, 183 }, { 294, 226 }, { 232, 170 }, { 402, 63 }, { 190, 215 }, { 148, 234 }, { 152, 126 },
	{ 289, 50 }, { 299, 274 }, { 268, 124 }, { 146, 153 }, { 273, 305 }, { 299, 187 }, { 344, 171 }, { 269, 336 },
	{ 210, 317 }, { 250, 318 }, { 280, 179 }, { 373, 86 }, { 354, 108 }, { 262, 267 }, { 232, 278 }, { 298, 97 },
	{ 238, 309 }, { 365, 43 }, { 307, 135 }, { 280, 25 }, { 249, 246 }, { 179, 276 }, { 259, 219 }, { 324, 60 },
	{ 195, 289 }, { 217, 39 }, { 202, 242 }, { 323, 26 }, { 251, 60 }, { 197, 59 }, { 255, 88 }, { 220, 88 }
};

/**
 * Room 3226 (original handler FUN_00429cc0): the jigsaw of the shards
 * (item 0x31), opened by looking at them (fallback: changeRoom(0xc9a)). 80 pieces (sheet
 * frames piece * 4 + rotation) are dragged with the left button, turned
 * with the right button or by dropping them on the "turn" corner (top
 * right), and snap to a 6-pixel grid. Positions and rotations are kept in
 * the saved random table (GameState::randomTable, filled by FUN_00429c10
 * on a new game); the room to return to is kept in var 0xc9a. A left click
 * in the bottom-left corner closes the puzzle.
 */
class Room3226 : public Room {
public:
	Room3226();
	const RoomDescriptor *getDescriptor() override;
	int handleEvent(int event, int param) override;

private:
	/** Index of the topmost piece under (x, y), -1 if none (loops of events 0xa / 0xd). */
	int pieceAt(int x, int y);
	/** Turns the selected piece a quarter back around its centre (code at 0x429fd8). */
	void rotateSelected();

	Sprite _a47ce10[80]; ///< array at 0x47ce10 (FUN_0040ff60): the pieces
	int _targets[80][2]; ///< table 0x444c40 (reduced modulo 6 in place when used)
	int _dragging;       ///< DAT_0047cb44: a piece is being dragged
	int _selected;       ///< DAT_0047c590: dragged / last touched piece, -1 = none
	int _dragOffX;       ///< DAT_0047ce04: piece x - mouse x when the drag started
	int _dragOffY;       ///< DAT_0047c84c: piece y - mouse y when the drag started
};

/** Constructs the room statics. */
Room3226::Room3226() : Room(3226), _dragging(0), _selected(0), _dragOffX(0), _dragOffY(0) {
	for (int i = 0; i < 80; i++) {
		_targets[i][0] = kRoom3226Targets[i][0];
		_targets[i][1] = kRoom3226Targets[i][1];
	}
}

/** Static descriptor 0x444f10. */
const RoomDescriptor *Room3226::getDescriptor() {
	return findRoomData(_id);
}

/** Topmost piece whose frame box contains (x, y); -1 if none. */
int Room3226::pieceAt(int x, int y) {
	for (int i = 79; i >= 0; i--) {
		const Sprite &piece = _a47ce10[i];
		const Common::Point size = piece.getSize();
		if (piece.x <= x && piece.y <= y && x < piece.x + size.x && y < piece.y + size.y)
			return i;
	}
	return -1;
}

/** Previous rotation frame of the selected piece, re-centred; ends a drag. */
void Room3226::rotateSelected() {
	Sprite &piece = _a47ce10[_selected];
	const Common::Point before = piece.getSize();
	const int frame = piece.frame;
	piece.setFrame((frame % 4 + 3) % 4 + frame / 4 * 4);
	const Common::Point after = piece.getSize();
	piece.x += (before.x - after.x) / 2;
	piece.y += (before.y - after.y) / 2;
	piece.priority = _selected;
	piece.subPriority = 0;
	_dragging = 0;
}

/** Room events (FUN_00429cc0, `what` >= 2). */
int Room3226::handleEvent(int event, int param) {
	// Mouse events carry x | y << 16
	const int mouseX = param & 0xFFFF;
	const int mouseY = (uint32)param >> 16;

	switch (event) {
	// Enter: lay out the pieces from the saved table (clamped to the screen), hide the hero
	case kEvEnter:
		for (int i = 0; i < 80; i++) {
			RandomEntry &entry = game()->_state.randomTable[i];
			// Unsigned 16-bit compares: negative positions end up at the right / bottom edge
			if ((uint16)entry.x > 0x27b)
				entry.x = 0x27b;
			if ((uint16)entry.y > 0x168)
				entry.y = 0x168;
			int y = (uint16)entry.y;
			if (y > 0x168)
				y = 0x168;
			game()->placeObject(&_a47ce10[i], (uint16)entry.x, y, i, entry.kind + i * 4, 0, -1);
		}
		hero().hide();
		if (param != 0xc9a && param != -1)
			game()->var(0xc9a) = param;
		_dragging = 0;
		_selected = -1;
		return 0;

	// Leave: save positions and rotations of the pieces, show the hero again
	case kEvLeave:
		for (int i = 0; i < 80; i++) {
			RandomEntry &entry = game()->_state.randomTable[i];
			entry.x = (int16)_a47ce10[i].x;
			entry.y = (int16)_a47ce10[i].y;
			entry.kind = (byte)(_a47ce10[i].frame % 4);
		}
		hero().show();
		return 0;

	// Looking at item 0x31 again does not reopen the puzzle
	case kEvBeforeItemRoom:
		return 1;

	// Left button down: pick up the topmost piece under the mouse
	case kEvLeftDown: {
		const int i = pieceAt(mouseX, mouseY);
		if (i >= 0) {
			_dragging = 1;
			_dragOffX = _a47ce10[i].x - mouseX;
			_selected = i;
			_dragOffY = _a47ce10[i].y - mouseY;
			return 0;
		}
		break;
	}

	// Every tick with the mouse position: the dragged piece follows the mouse (bottom clamped)
	case kEvClick:
		if (_dragging) {
			int y = mouseY + _dragOffY;
			if (y > 0x186)
				y = 0x186;
			Sprite &piece = _a47ce10[_selected];
			piece.x = mouseX + _dragOffX;
			piece.y = y;
			piece.priority = _selected;
			piece.subPriority = 0;
			return 0;
		}
		break;

	// Left button up: drop on the "turn" corner = rotate, else snap to the grid; no drag: close button
	case kEvLeftUp: {
		// DAT_00481448 / DAT_0048144c: the current mouse position
		const int curX = g_engine->_input.mouseX;
		const int curY = g_engine->_input.mouseY;
		if (_dragging && curX > 0x23a && curY < 0x46) {
			rotateSelected();
			return 0;
		}
		if (_dragging) {
			int *target = _targets[_selected];
			const int phaseX = target[0] % 6;
			const int phaseY = target[1] % 6;
			target[0] = phaseX;
			target[1] = phaseY;
			int y = phaseY + (mouseY - phaseY / 2 + _dragOffY) / 6 * 6;
			if (y > 0x186)
				y = 0x186;
			Sprite &piece = _a47ce10[_selected];
			piece.x = phaseX + (mouseX - phaseX / 2 + _dragOffX) / 6 * 6;
			piece.y = y;
			piece.priority = _selected;
			piece.subPriority = 0;
			_dragging = 0;
			return 0;
		}
		if (curX < 0x3c && curY > 0x154 && curY < 400)
			game()->changeRoom(game()->var(0xc9a));
		_dragging = 0;
		break;
	}

	// Right button down: turn the topmost piece under the mouse
	case kEvRightDown: {
		const int i = pieceAt(mouseX, mouseY);
		if (i >= 0) {
			_selected = i;
			rotateSelected();
			return 0;
		}
		break;
	}

	default:
		break;
	}
	return 0;
}

/** Factory used by the room registry. */
Room *createRoom3226() {
	return new Room3226();
}

} // End of namespace Gooka
