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
 * Walk map, hotspot map and path finding data of the current room
 * (original globals DAT_00449638, DAT_0044d4d0, DAT_00451358).
 */

#ifndef GOOKA_WALK_H
#define GOOKA_WALK_H

#include "common/rect.h"
#include "common/stream.h"

namespace Gooka {

struct ZoneRecord;

/** Size of the walk grid; one cell covers 4x4 pixels of the 640x400 scene. */
enum {
	kGridWidth = 160,
	kGridHeight = 100,
	kWalkStripBit = 0x80 ///< walk cell flag: waypoint strip used by the path finder
};

/**
 * Walk and hotspot maps of the current room plus the path finder tables.
 * Coordinates are grid cells unless noted otherwise.
 *
 * Path finding (object DAT_00451358): cells with bit 0x80 form "strips"
 * (corridors); strip cells with a strip neighbour on both axes are
 * junctions. Per cell, four tables give the nearest strip/junction/walkable
 * cell in each direction (FUN_00410da0). A path is tried as an L shape
 * first (both orders); otherwise a branch-and-bound search runs from the
 * start over the junction graph towards the junctions reachable from the
 * target. The path is cached per target cell.
 */
class WalkMap {
public:
	WalkMap();
	~WalkMap();

	/**
	 * Loads the zone resource (32000 bytes: two 160x100 planes, row-major in
	 * the file) and remembers the room's zone table. Builds the path tables
	 * (FUN_00410da0).
	 */
	void load(Common::SeekableReadStream *zoneResource, const ZoneRecord *zones, int numZones);
	/** Forgets the current maps (no room loaded). */
	void clear();

	/** Raw walk cell: 0 = blocked, bit 7 = waypoint strip, low 7 bits = zone index (1-based). */
	byte walkCell(int gx, int gy) const;
	/** Hotspot slot (1-based index into the descriptor's hotspot list), 0 = none. */
	byte hotspotCell(int gx, int gy) const;

	/** Zone record of a cell, nullptr for blocked cells or index 0 (FUN_0040fab0). */
	const ZoneRecord *zoneAt(int gx, int gy) const;
	/** Actor scale at a pixel position (FUN_0040fb30). */
	int scaleAt(int px, int py) const;
	/** Zone type of a cell, -1 if it has no zone or is off the grid (FUN_0040fb00 / FUN_00411820). */
	int zoneType(int gx, int gy) const;

	/** @name Path finder */
	///@{
	/**
	 * Nearest walkable cell for a floor click on (gx, gy), moved to the top
	 * end of stair (type 1) and ladder (types 3/4, 7/8) zones (FUN_0040fbd0).
	 * @return false if no walkable cell is within 30 cells
	 */
	bool findWalkTarget(int gx, int gy, Common::Point &target) const;
	/**
	 * Ring search for the nearest walkable cell (FUN_00411c90): the four
	 * cells at distance r = 1..29 on the axes, plus the per-cell nearest
	 * walkable tables perpendicular to them.
	 * @return false if nothing was found (result is then (gx, gy))
	 */
	bool findNearestWalkable(int gx, int gy, Common::Point &result) const;
	/**
	 * Next waypoint on the way from cell (sx, sy) to (tx, ty) (FUN_004119b0,
	 * called through FUN_0040fba0): follows the cached path along one axis,
	 * stops where the zone type changes and, in zone types 0/5/6, cuts the
	 * corner with a 45 degree diagonal when it is free.
	 * @return false if there is no path (then (wx, wy) = (sx, sy))
	 */
	bool findNextWaypoint(int sx, int sy, int tx, int ty, int &wx, int &wy);
	///@}

private:
	/**
	 * Per-cell path finder record (5 bytes at DAT_00451358 + 0x3e84).
	 * Meaning of up/down/left/right by cell kind: blocked cell -> nearest
	 * walkable cell; walkable non-strip cell -> nearest strip cell of the
	 * same walkable run; strip cell -> nearest junction of the same strip run.
	 */
	struct PathCell {
		byte junction; ///< 1 = junction strip cell, 2 while visited by the search
		byte up;       ///< y of the nearest cell above (smaller y), 0xff = none
		byte down;     ///< y of the nearest cell below (larger y)
		byte left;     ///< x of the nearest cell to the left
		byte right;    ///< x of the nearest cell to the right
	};

	enum {
		kNone = 0xff,            ///< "no cell" in the PathCell tables
		kMaxPath = 256 + 8,      ///< path arrays (256 in the original, margin for the terminator)
		kStackSize = 4,          ///< target-side search stack (DAT_00451358 + 0x17f04)
		kNoCost = 9999999        ///< "no path found yet" cost
	};

	/** True if (gx, gy) is on the grid. */
	static bool inGrid(int gx, int gy) { return gx >= 0 && gx < kGridWidth && gy >= 0 && gy < kGridHeight; }
	/** Builds the junction flags and the four nearest-cell tables (FUN_00410da0). */
	void buildPathTables();
	/** Walkable along row sy from sx to tx, then along column tx to ty (FUN_00411450). */
	bool rowThenColumnFree(int sx, int sy, int tx, int ty) const;
	/** Walkable along column sx from sy to ty, then along row ty to tx (FUN_00411510). */
	bool columnThenRowFree(int sx, int sy, int tx, int ty) const;
	/** Computes and caches the path from (sx, sy) to (tx, ty) (FUN_004115c0). */
	bool computePath(int sx, int sy, int tx, int ty);
	/** Target side of the search: walks from (cx, cy) to a junction, then searches from the start (FUN_00411080). */
	bool searchFromTarget(int sx, int sy, int cx, int cy, int cost);
	/** Recursive branch-and-bound search from (cx, cy) over junctions to goal (gx, gy) (FUN_00411220). */
	bool searchStrips(int cx, int cy, int gx, int gy, int cost);
	/** Index of the path point after (sx, sy), computing the path if needed; -1 = no path (FUN_00411750). */
	int pathIndex(int sx, int sy, int tx, int ty);
	/** Straight run along the next path segment, clipped at a zone type change (FUN_00411850). */
	bool nextPathPoint(int sx, int sy, int tx, int ty, int &wx, int &wy);

	byte _walk[kGridWidth * kGridHeight];    ///< [x * 100 + y] like the original
	byte _hotspot[kGridWidth * kGridHeight]; ///< [x * 100 + y]
	const ZoneRecord *_zones;
	int _numZones;

	PathCell _cells[kGridWidth * kGridHeight]; ///< [x * 100 + y]
	int _pathX[kMaxPath];  ///< +0x17704 path points, _pathX[_pathLen] = -1
	int _pathY[kMaxPath];  ///< +0x17b04
	int _stackX[kStackSize]; ///< +0x17f04 target-side cells of the best path
	int _stackY[kStackSize]; ///< +0x17f14
	int _bestCost;         ///< +0x17f24
	int _level;            ///< +0x17f28 recursion level of searchStrips()
	int _pathLen;          ///< +0x17f2c number of path points
	int _depth;            ///< +0x17f30 recursion level of searchFromTarget()
	int _maxDepth;         ///< +0x17f34 used entries of the stack
	int _cacheX, _cacheY;  ///< +0x17f38/+0x17f3c target of the cached path
};

} // End of namespace Gooka

#endif // GOOKA_WALK_H
