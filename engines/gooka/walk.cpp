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
#include "common/textconsole.h"

#include "gooka/detection.h"
#include "gooka/room.h"
#include "gooka/walk.h"

namespace Gooka {

/** -1, 0 or 1 like the original `((-1 < d) - 1 & ~1) + 1` (0 for d == 0). */
static int signOf(int d) {
	return d == 0 ? 0 : (d > 0 ? 1 : -1);
}

/** True for the zone types the corner cutting may cross (no zone, floor, 5, 6). */
static bool isFlatType(int type) {
	return type == 0 || type == 5 || type == 6;
}

// Empty maps until a room is loaded.
WalkMap::WalkMap() {
	clear();
}

WalkMap::~WalkMap() {
}

// Transpose like FUN_0040efb0 (file [y * 160 + x] -> [x * 100 + y]), then FUN_00410da0.
void WalkMap::load(Common::SeekableReadStream *zoneResource, const ZoneRecord *zones, int numZones) {
	_zones = zones;
	_numZones = numZones;

	byte plane[kGridWidth * kGridHeight];
	for (int p = 0; p < 2; p++) {
		memset(plane, 0, sizeof(plane));
		if (zoneResource->read(plane, sizeof(plane)) != sizeof(plane))
			warning("Gooka: short walk map resource");
		byte *dst = p == 0 ? _walk : _hotspot;
		for (int y = 0; y < kGridHeight; y++) {
			for (int x = 0; x < kGridWidth; x++)
				dst[x * kGridHeight + y] = plane[y * kGridWidth + x];
		}
	}

	buildPathTables();
	debugC(1, kDebugScript, "WalkMap: loaded, %d zones", numZones);
}

// No room: everything blocked, no zones.
void WalkMap::clear() {
	memset(_walk, 0, sizeof(_walk));
	memset(_hotspot, 0, sizeof(_hotspot));
	_zones = nullptr;
	_numZones = 0;
	memset(_pathX, 0, sizeof(_pathX));
	memset(_pathY, 0, sizeof(_pathY));
	memset(_stackX, 0, sizeof(_stackX));
	memset(_stackY, 0, sizeof(_stackY));
	_pathX[0] = -1;
	_bestCost = kNoCost;
	_level = 0;
	_pathLen = 0;
	_depth = 0;
	_maxDepth = 0;
	buildPathTables();
}

// Plain accessor (DAT_00449638).
byte WalkMap::walkCell(int gx, int gy) const {
	return inGrid(gx, gy) ? _walk[gx * kGridHeight + gy] : 0;
}

// Plain accessor (DAT_0044d4d0).
byte WalkMap::hotspotCell(int gx, int gy) const {
	return inGrid(gx, gy) ? _hotspot[gx * kGridHeight + gy] : 0;
}

// FUN_0040fab0. Deviation: indexes past the zone table give nullptr instead of garbage.
const ZoneRecord *WalkMap::zoneAt(int gx, int gy) const {
	if (!inGrid(gx, gy))
		return nullptr;
	const int index = _walk[gx * kGridHeight + gy] & 0x7f;
	if (index == 0 || !_zones || index > _numZones)
		return nullptr;
	return &_zones[index - 1];
}

// FUN_0040fb30: linear ramp from 0 at row yStart to `scale` at row yEnd, clamped to 0..0x100.
int WalkMap::scaleAt(int px, int py) const {
	const ZoneRecord *zone = zoneAt(px >> 2, py >> 2);
	if (!zone)
		return 0x100;
	int s;
	if (zone->yEnd == -1)
		s = zone->scale;
	else
		s = ((py - zone->yStart * 4 - 2) * zone->scale) / ((zone->yEnd - zone->yStart) * 4 + 2);
	if (s < 0)
		s = 0;
	if (s > 0x100)
		s = 0x100;
	return s;
}

// FUN_0040fb00 / FUN_00411820.
int WalkMap::zoneType(int gx, int gy) const {
	const ZoneRecord *zone = zoneAt(gx, gy);
	return zone ? zone->type : -1;
}

// FUN_0040fbd0: nearest walkable cell, then climb to the top end of stairs and ladders.
bool WalkMap::findWalkTarget(int gx, int gy, Common::Point &target) const {
	if (!findNearestWalkable(gx, gy, target))
		return false;

	int x = target.x, y = target.y;
	int t;
	switch (zoneType(x, y)) {
	case 1:
		// Stairs: go to the top of a vertical run of type-1 cells ...
		if (zoneType(x, y - 1) == 1) {
			while (zoneType(x, y) == 1)
				y--;
			target.y = y + 1;
			return true;
		}
		// ... or to the first cell to the right, then to the left, that has type 1 below it
		for (int i = 0; zoneType(x + i, y) == 1; i++) {
			if (zoneType(x + i, y + 1) == 1) {
				target.x = x + i;
				return true;
			}
		}
		if (zoneType(x, y) == 1) {
			for (int i = 0;;) {
				if (zoneType(x + i, y + 1) == 1) {
					target.x = x + i;
					return true;
				}
				i--;
				if (zoneType(x + i, y) != 1)
					return true;
			}
		}
		return true;

	case 3:
		// Ladder 3/4: up to the first type-4 cell (or the top of the zone) ...
		t = zoneType(x, y);
		if (t != -1) {
			while (t != 4) {
				y--;
				t = zoneType(x, y);
				if (t == -1)
					break;
			}
		}
		if (t == -1)
			y++;
		// fall through
	case 7:
		// Ladder 7/8: up to the first type-8 cell, else the top of the zone
		t = zoneType(x, y);
		while (t != -1) {
			if (t == 8) {
				target.y = y;
				return true;
			}
			y--;
			t = zoneType(x, y);
		}
		target.y = y + 1;
		return true;

	default:
		return true;
	}
}

// FUN_00411c90. Deviation: a start cell off the grid is treated as blocked instead of read out of bounds.
bool WalkMap::findNearestWalkable(int gx, int gy, Common::Point &result) const {
	result = Common::Point(gx, gy);
	if (inGrid(gx, gy) && _walk[gx * kGridHeight + gy] != 0)
		return true;

	int best = kNoCost;
	for (int r = 1; r < 30; r++) {
		if (best != kNoCost)
			break;
		for (int d = 0; d < 4; d++) {
			const int nx = ((d == 0) - (d == 1)) * r + gx;
			const int ny = ((d == 2) - (d == 3)) * r + gy;
			if (!inGrid(nx, ny))
				continue;
			const int index = nx * kGridHeight + ny;
			if (_walk[index] != 0) {
				result = Common::Point(nx, ny);
				return true;
			}
			const PathCell &cell = _cells[index];
			if (d < 2) {
				// Horizontal offset: look up and down from the probe
				int dist = ny - cell.up;
				if (cell.up != kNone && dist < best && dist < r) {
					result = Common::Point(nx, cell.up);
					best = dist;
				}
				dist = cell.down - gy;
				if (cell.down != kNone && dist < best && dist < r) {
					result = Common::Point(nx, cell.down);
					best = dist;
				}
			} else {
				// Vertical offset: look left and right from the probe
				int dist = gx - cell.left;
				if (cell.left != kNone && dist < best && dist < r) {
					result = Common::Point(cell.left, ny);
					best = dist;
				}
				dist = cell.right - gx;
				if (cell.right != kNone && dist < best && dist < r) {
					result = Common::Point(cell.right, ny);
					best = dist;
				}
			}
		}
	}
	return best < 30;
}

// FUN_004119b0: next waypoint with 45 degree corner cutting (see walk.h).
bool WalkMap::findNextWaypoint(int sx, int sy, int tx, int ty, int &wx, int &wy) {
	const int startType = zoneType(sx, sy);
	if (!nextPathPoint(sx, sy, tx, ty, wx, wy))
		return false;
	if ((wx == tx && wy == ty) || !isFlatType(startType))
		return true;

	// The segment after the waypoint
	int x2, y2;
	if (!nextPathPoint(wx, wy, tx, ty, x2, y2))
		return true;

	if (sy == wy) {
		// First leg along the row, second along the column: find the first point
		// of the first leg from which a diagonal reaches the column (wx) without
		// passing y2 and crossing only flat zones.
		const int dx = signOf(wx - sx);
		const int dy = signOf(y2 - wy);
		for (int i = sx; i != wx; i += dx) {
			const int rest = wx - i;
			if ((dy * dx * rest + sy) * dy > dy * y2)
				continue;
			const int len = ABS(rest);
			int k = 0;
			for (int cx = i, cy = sy; k < len; k++, cx += dx, cy += dy) {
				if (!isFlatType(zoneType(cx, cy)))
					break;
			}
			if (k < len)
				continue;
			if (i == sx)
				wy = rest * dy * dx + sy; // go diagonally right away
			else
				wx = i; // shorten the first leg
			return true;
		}
		return true;
	}

	// First leg along the column, second along the row (mirror image of the above)
	const int dy = signOf(wy - sy);
	const int dx = signOf(x2 - wx);
	for (int i = sy; i != wy; i += dy) {
		const int rest = wy - i;
		if ((dx * dy * rest + sx) * dx > dx * x2)
			continue;
		const int len = ABS(rest);
		int k = 0;
		for (int cx = sx, cy = i; k < len; k++, cx += dx, cy += dy) {
			if (!isFlatType(zoneType(cx, cy)))
				break;
		}
		if (k < len)
			continue;
		if (i == sy)
			wx = rest * dx * dy + sx;
		else
			wy = i;
		return true;
	}
	return true;
}

// FUN_00410da0: junction flags, then the nearest-cell tables in the four directions.
void WalkMap::buildPathTables() {
	// Junctions: strip cells with a strip neighbour horizontally and vertically
	for (int x = 0; x < kGridWidth; x++) {
		for (int y = 0; y < kGridHeight; y++) {
			const int i = x * kGridHeight + y;
			_cells[i].junction = 0;
			if (!(_walk[i] & kWalkStripBit))
				continue;
			const bool horiz = (x > 0 && (_walk[i - kGridHeight] & kWalkStripBit)) ||
				(x < kGridWidth - 1 && (_walk[i + kGridHeight] & kWalkStripBit));
			const bool vert = (y > 0 && (_walk[i - 1] & kWalkStripBit)) ||
				(y < kGridHeight - 1 && (_walk[i + 1] & kWalkStripBit));
			if (horiz && vert)
				_cells[i].junction = 1;
		}
	}

	// One scan line of the tables. `junction` = last junction of the current strip
	// run, `strip` = last strip cell of the current walkable run, `walkable` = last
	// walkable cell; each value is stored before the current cell updates them.
	for (int x = 0; x < kGridWidth; x++) {
		int junction = kNone, strip = kNone, walkable = kNone;
		for (int y = 0; y < kGridHeight; y++) {
			const byte c = _walk[x * kGridHeight + y];
			PathCell &cell = _cells[x * kGridHeight + y];
			cell.up = (c & kWalkStripBit) ? junction : (c ? strip : walkable);
			if (cell.junction)
				junction = y;
			if (c & kWalkStripBit)
				strip = y;
			else
				junction = kNone;
			if (c)
				walkable = y;
			else
				strip = kNone;
		}
		junction = strip = walkable = kNone;
		for (int y = kGridHeight - 1; y >= 0; y--) {
			const byte c = _walk[x * kGridHeight + y];
			PathCell &cell = _cells[x * kGridHeight + y];
			cell.down = (c & kWalkStripBit) ? junction : (c ? strip : walkable);
			if (cell.junction)
				junction = y;
			if (c & kWalkStripBit)
				strip = y;
			else
				junction = kNone;
			if (c)
				walkable = y;
			else
				strip = kNone;
		}
	}
	for (int y = 0; y < kGridHeight; y++) {
		int junction = kNone, strip = kNone, walkable = kNone;
		for (int x = 0; x < kGridWidth; x++) {
			const byte c = _walk[x * kGridHeight + y];
			PathCell &cell = _cells[x * kGridHeight + y];
			cell.left = (c & kWalkStripBit) ? junction : (c ? strip : walkable);
			if (cell.junction)
				junction = x;
			if (c & kWalkStripBit)
				strip = x;
			else
				junction = kNone;
			if (c)
				walkable = x;
			else
				strip = kNone;
		}
		junction = strip = walkable = kNone;
		for (int x = kGridWidth - 1; x >= 0; x--) {
			const byte c = _walk[x * kGridHeight + y];
			PathCell &cell = _cells[x * kGridHeight + y];
			cell.right = (c & kWalkStripBit) ? junction : (c ? strip : walkable);
			if (cell.junction)
				junction = x;
			if (c & kWalkStripBit)
				strip = x;
			else
				junction = kNone;
			if (c)
				walkable = x;
			else
				strip = kNone;
		}
	}

	_cacheX = _cacheY = 0xff;
}

// FUN_00411450.
bool WalkMap::rowThenColumnFree(int sx, int sy, int tx, int ty) const {
	const int x0 = MIN(sx, tx), x1 = MAX(sx, tx);
	for (int x = x0; x <= x1; x++) {
		if (!_walk[x * kGridHeight + sy])
			return false;
	}
	const int y0 = MIN(sy, ty), y1 = MAX(sy, ty);
	for (int y = y0; y <= y1; y++) {
		if (!_walk[tx * kGridHeight + y])
			return false;
	}
	return true;
}

// FUN_00411510.
bool WalkMap::columnThenRowFree(int sx, int sy, int tx, int ty) const {
	const int y0 = MIN(sy, ty), y1 = MAX(sy, ty);
	for (int y = y0; y <= y1; y++) {
		if (!_walk[sx * kGridHeight + y])
			return false;
	}
	const int x0 = MIN(sx, tx), x1 = MAX(sx, tx);
	for (int x = x0; x <= x1; x++) {
		if (!_walk[x * kGridHeight + ty])
			return false;
	}
	return true;
}

// FUN_004115c0. Deviation: endpoints off the grid fail instead of reading out of bounds.
bool WalkMap::computePath(int sx, int sy, int tx, int ty) {
	_cacheX = -1;
	if (!inGrid(sx, sy) || !inGrid(tx, ty) ||
			_walk[sx * kGridHeight + sy] == 0 || _walk[tx * kGridHeight + ty] == 0)
		return false;

	// Default: start, corner, target
	_pathX[0] = _pathX[1] = sx;
	_pathY[0] = _pathY[1] = sy;
	_pathX[2] = tx;
	_pathY[2] = ty;
	_pathX[3] = -1;
	_cacheX = tx;
	_cacheY = ty;
	_pathLen = 3;

	if (rowThenColumnFree(sx, sy, tx, ty)) {
		_pathX[1] = tx;
		return true;
	}
	if (columnThenRowFree(sx, sy, tx, ty)) {
		_pathY[1] = ty;
		return true;
	}

	// Strip search: start ... junction (searchStrips), then the target-side cells in reverse
	_bestCost = kNoCost;
	_maxDepth = 0;
	_pathLen = 0;
	searchFromTarget(sx, sy, tx, ty, 0);
	while (_maxDepth != 0) {
		_maxDepth--;
		_pathX[_pathLen] = _stackX[_maxDepth];
		_pathY[_pathLen] = _stackY[_maxDepth];
		_pathLen++;
	}
	_pathX[_pathLen] = -1;
	debugC(3, kDebugScript, "WalkMap: path (%d,%d)->(%d,%d): %d points, cost %d", sx, sy, tx, ty, _pathLen, _bestCost);
	return _bestCost != kNoCost;
}

// FUN_00411080. Note: the original charges `start - next` instead of
// `current - next` for the target-side moves; kept as is (it only
// affects which of several paths wins).
bool WalkMap::searchFromTarget(int sx, int sy, int cx, int cy, int cost) {
	const int index = cx * kGridHeight + cy;
	const PathCell &cell = _cells[index];
	if ((_walk[index] & kWalkStripBit) && cell.junction != 0) {
		_level = 0;
		return searchStrips(sx, sy, cx, cy, cost);
	}

	_depth++;
	bool found = false;
	if (cell.left != kNone && searchFromTarget(sx, sy, cell.left, cy, cost + sx - cell.left))
		found = true;
	if (cell.right != kNone && searchFromTarget(sx, sy, cell.right, cy, cost - sx + cell.right))
		found = true;
	if (cell.up != kNone && searchFromTarget(sx, sy, cx, cell.up, cost + sy - cell.up))
		found = true;
	if (cell.down != kNone && searchFromTarget(sx, sy, cx, cell.down, cost - sy + cell.down))
		found = true;
	_depth--;

	// The stack never exceeds 2 entries with valid tables; the check only guards memory
	if (found && _depth < kStackSize) {
		_stackX[_depth] = cx;
		_stackY[_depth] = cy;
		if (_maxDepth <= _depth)
			_maxDepth = _depth + 1;
	}
	return found;
}

// FUN_00411220: depth-first search with a cost bound; junctions are marked 2 while on the stack.
bool WalkMap::searchStrips(int cx, int cy, int gx, int gy, int cost) {
	if (cost >= _bestCost || _level >= 0xfc)
		return false;

	if (cx == gx && cy == gy) {
		_bestCost = cost;
		_pathX[_level] = gx;
		_pathY[_level] = gy;
		_pathLen = _level + 1;
		return true;
	}

	PathCell &cell = _cells[cx * kGridHeight + cy];
	if (cell.junction == 1)
		cell.junction = 2;
	bool found = false;
	_level++;
	if (cell.left != kNone && _cells[cell.left * kGridHeight + cy].junction != 2 &&
			searchStrips(cell.left, cy, gx, gy, cost + cx - cell.left))
		found = true;
	if (cell.right != kNone && _cells[cell.right * kGridHeight + cy].junction != 2 &&
			searchStrips(cell.right, cy, gx, gy, cost - cx + cell.right))
		found = true;
	if (cell.up != kNone && _cells[cx * kGridHeight + cell.up].junction != 2 &&
			searchStrips(cx, cell.up, gx, gy, cost + cy - cell.up))
		found = true;
	if (cell.down != kNone && _cells[cx * kGridHeight + cell.down].junction != 2 &&
			searchStrips(cx, cell.down, gx, gy, cost - cy + cell.down))
		found = true;
	_level--;
	if (cell.junction == 2)
		cell.junction = 1;

	if (!found)
		return false;
	_pathX[_level] = cx;
	_pathY[_level] = cy;
	return true;
}

// FUN_00411750: reuses the cached path when the target is the same and the start is on it.
int WalkMap::pathIndex(int sx, int sy, int tx, int ty) {
	if (tx == _cacheX && ty == _cacheY) {
		for (int i = 0; i < _pathLen; i++) {
			if (_pathX[i] == sx && _pathY[i] == sy) {
				while (_pathX[i] == sx && _pathY[i] == sy)
					i++;
				return i;
			}
		}
	}

	if (!computePath(sx, sy, tx, ty))
		return -1;
	int i = 1;
	while (_pathX[i] == sx && _pathY[i] == sy)
		i++;
	return i;
}

// FUN_00411850: follows the path points that share the start's column (or row),
// then stops at the first cell whose zone type differs from the start's.
bool WalkMap::nextPathPoint(int sx, int sy, int tx, int ty, int &wx, int &wy) {
	const int index = pathIndex(sx, sy, tx, ty);
	wx = sx;
	wy = sy;
	if (index == -1 || _pathX[index] == -1)
		return false;

	const int startType = zoneType(sx, sy);
	if (_pathX[index] == sx) {
		// Vertical segment
		for (int i = index; _pathY[i] != -1 && _pathX[i] == sx; i++)
			wy = _pathY[i];
		const int dir = wy >= sy ? 1 : -1;
		for (int y = sy; y != wy; y += dir) {
			if (zoneType(sx, y) != startType) {
				wy = y;
				return true;
			}
		}
		return true;
	}

	// Horizontal segment
	for (int i = index; _pathX[i] != -1 && _pathY[i] == sy; i++)
		wx = _pathX[i];
	const int dir = wx >= sx ? 1 : -1;
	for (int x = sx; x != wx; x += dir) {
		if (zoneType(x, sy) != startType) {
			wx = x;
			return true;
		}
	}
	return true;
}

} // End of namespace Gooka
