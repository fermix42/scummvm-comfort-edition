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
 * Special sprites: shimmering water/heat, particle fields, the book pages,
 * sparkles, plain rectangles and the game-over blinds.
 * Colours are 0x00BBGGRR like in text.h.
 */

#ifndef GOOKA_EFFECTS_H
#define GOOKA_EFFECTS_H

#include "common/array.h"

#include "gooka/sprite.h"
#include "gooka/text.h"

namespace Gooka {

/**
 * Draws a frame of the room sheet with a per-row horizontal jitter
 * (vtable 0x437950; constructors FUN_00412690 / FUN_00417080, step
 * FUN_00412430, draw FUN_004121c0). Usually priority -1, seen through holes
 * in the background. Placed with Game::placeShimmer(), which fills the
 * public fields (sheet = room sheet) and calls init().
 * Source row = (phase + screenRow - y) % frameHeight, so rooms animate a
 * vertical scroll by writing `phase` (e.g. tickCounter >> 2).
 */
class ShimmerSprite : public Sprite {
public:
	/** FUN_00412690(amplitude, direction); amplitude 0 = FUN_00417080 (no init). */
	explicit ShimmerSprite(int amplitude = 0, int flag = -1);

	int width;      ///< +0x30 drawn area
	int height;     ///< +0x34
	int amplitude;  ///< +0x38 offsets are rnd(amplitude) - amplitude / 2
	int sheetFrame; ///< +0x3c frame of `sheet` that is distorted
	int phase;      ///< +0x40 vertical scroll of the source rows
	int direction;  ///< +0x44 rows the offset table scrolls per step (sign = direction, 0 = interlaced)

	/** Fills the offset table with random values (FUN_004125f0); also drops the decoded frame. */
	void init();
	/** Scrolls the offset table by `direction` rows, new random values enter (FUN_00412430); called from the tick event. */
	void step();
	/** Fills the table with the sine wave generator, restarting it (FUN_00412620). */
	void regenerate();
	/** Shifts the table by one row and adds a new wave value (FUN_00412590). */
	void scroll();

	/** (width, height) (FUN_00412c10). */
	Common::Point getSize() const override;
	/** FUN_004121c0, clipped to (x, y, width, height). */
	void draw(Graphics::Surface &dst) override;

private:
	/** Next wave value: sin(8, ++counter * 16) (FUN_00412570). */
	int8 nextWave();
	/** Random table entry rnd(amplitude) - amplitude / 2. */
	int8 randomOffset();
	/** Decodes sheetFrame of sheet into _pixels unless it is already there. */
	void decodeFrame();

	int8 _offsets[400]; ///< +0x4c per-row x offsets
	int _waveCounter;   ///< +0x1dc (+0x1e0..+0x1ec are written by FUN_00412620 but never read)

	Common::Array<uint16> _pixels; ///< decoded frame, raw RGB555, 0xFFFF = transparent
	const Image *_decodedSheet;    ///< sheet/frame in _pixels (nullptr = none)
	int _decodedFrame;
	int _frameWidth;               ///< image width of the decoded frame
	int _frameHeight;              ///< content height: last non-empty row + 1 (frame record +4)
};

/** Random particle field (vtable 0x437960, FUN_00412910 / FUN_004127a0, draw FUN_00412710). */
class ParticleSprite : public Sprite {
public:
	explicit ParticleSprite(int count);
	~ParticleSprite() override;

	int width;  ///< +0x34 box the particles live in
	int height; ///< +0x38

	/** Moves every particle; with probability 2/count, or when it left the box, it respawns (FUN_004127a0). */
	void step();

	/** (width, height) (FUN_004126f0). */
	Common::Point getSize() const override;
	/** One pixel per particle (FUN_00412710). */
	void draw(Graphics::Surface &dst) override;

private:
	/** One particle: position in the box, screen colour, velocity. */
	struct Particle {
		int x, y;     ///< +0x3c array
		uint32 color; ///< +0x40 array, screen format
		int dx, dy;   ///< +0x44 array
	};
	Particle *_particles;
	int _count; ///< +0x30
};

/**
 * The book (vtable 0x437a78, ctor FUN_0042a830): a 640x400 sprite showing
 * page `frame` (0..max), i.e. the strings `param + 2 * frame` (left column,
 * x = 20) and `param + 2 * frame + 1` (right column, x = 360), in black
 * font 403 starting at y + 28. Lines are aligned in 250 px; a line "@n"
 * shows frame n of the room sheet instead (FUN_0040ffe0).
 */
class CounterSprite : public Sprite {
public:
	CounterSprite(int a, int max);

	int param; ///< +0x34 string id of page 0, left column
	int max;   ///< +0x38 last page

	void increment(); ///< next page with a click sound (FUN_00410200)
	void decrement(); ///< previous page with a click sound (FUN_00410240)
	void reset();     ///< page 0 (FUN_004101f0)

	/** (640, 400) (FUN_0040ffc0). */
	Common::Point getSize() const override;
	/** Both columns of the current page (FUN_004101b0). */
	void draw(Graphics::Surface &dst) override;

private:
	/** One column: string `stringId` laid out at column x `columnX` (FUN_0040ffe0). */
	void drawColumn(Graphics::Surface &dst, const Common::Rect &clip, int columnX, int stringId);
};

/**
 * 16 short-lived "+"-shaped sparkles around a point (container FUN_00412160,
 * start FUN_00412080, step FUN_004120c0, reset FUN_00412050). As in the
 * original every spark is a sprite of its own (vtable 0x437940, 0x44 bytes)
 * that shows/hides itself in the scene with the burst's priority and
 * sub-priority = its index; the container itself draws nothing.
 * The inventory steps its sparkle once per tick (Game::tick FUN_0041d280).
 */
class SparkleSprite : public Sprite {
public:
	SparkleSprite();

	/**
	 * Starts a burst (FUN_00412080): for `duration` ticks (negative = forever)
	 * idle sparks respawn at random in (x ± count, y ± count).
	 * @param count  spread radius in pixels (+0x450)
	 * @param param  spark colour (the inventory uses white)
	 */
	void start(int x, int y, int priority, int count, int duration, int param);
	/** Hides and stops all sparks (FUN_00412050). */
	void reset();
	/** Advances the sparks by one tick and spawns new ones (FUN_004120c0). */
	void step();

	/** (0, 0): the sparks are separate scene sprites. */
	Common::Point getSize() const override;
	/** Nothing (see getSize()). */
	void draw(Graphics::Surface &dst) override;

private:
	/** One spark (vtable 0x437940). */
	class Spark : public Sprite {
	public:
		Spark();
		int cx, cy;   ///< +0x30/+0x34 centre
		int size;     ///< +0x38 arm length 0..5, -1 = idle
		uint32 color; ///< +0x3c base colour
		int delta;    ///< +0x40 +1 growing, -1 shrinking (every other tick)

		/** Hides the spark (FUN_00411fc0). */
		void stop();
		/** Shows a new spark at (x, y) (FUN_00411f70). */
		void spawn(int x, int y, int prio, int subPrio, uint32 rgb);
		/** Grows/shrinks it; hides it when it vanishes (FUN_00411f10). */
		void tick();
		/** (2 * size, 2 * size) (FUN_00411e70). */
		Common::Point getSize() const override;
		/** A "+" of two lines, brighter while larger (FUN_00411e90). */
		void draw(Graphics::Surface &dst) override;
	};

	Spark _sparks[16];
	int _remaining; ///< +0x440 ticks left in which sparks spawn
	int _x, _y;     ///< +0x444/+0x448 burst centre
	int _priority;  ///< +0x44c
	int _spread;    ///< +0x450
	uint32 _color;  ///< +0x454
};

/** Filled rectangle sprite (vtable 0x437980, ctor FUN_00412e10, draw FUN_00409590). */
class RectSprite : public Sprite {
public:
	RectSprite();

	int width;    ///< +0x30
	int height;   ///< +0x34
	uint32 color; ///< +0x38

	/** (width, height) (FUN_00412c10). */
	Common::Point getSize() const override;
	/** Fills (x, y, width, height) (FUN_00409590). */
	void draw(Graphics::Surface &dst) override;
};

/**
 * Double frame sprite (vtable 0x4379b8, draw FUN_00412c30): a black
 * roundFrame() around a yellow one inset by 1 px. The hover label of cursor
 * type 7 (FUN_0041be90) shows two of them, priority 0x3ea, growing from the
 * label bounds by (tickCounter / 3 + i * 5) % 10 px on every side (i = 0, 1).
 */
class FrameSprite : public Sprite {
public:
	FrameSprite();

	int width;  ///< +0x30
	int height; ///< +0x34

	/** (width, height) (FUN_00412c10). */
	Common::Point getSize() const override;
	/** FUN_00412c30. */
	void draw(Graphics::Surface &dst) override;
};

/**
 * Game-over blinds (static array DAT_00473d30 of 16 RectSprites): 16 black
 * bars of 640x30 at y = 0, 30, ..., 450 that close one after another in
 * 5 steps of 6 px; the 81st step returns to the title screen (FUN_0041ca30).
 * Rooms call reset() on entry (event 3) and addLine() from event 0x17
 * (raised by their sequences) or from the tick event. (The historical name
 * "EndTextScroller" is kept for API compatibility; nothing scrolls.)
 */
class EndTextScroller {
public:
	EndTextScroller();

	void reset();   ///< FUN_00412d30: empty bars, registered as dynamic hotspots (-1) = shown
	void addLine(); ///< FUN_00412d80: next 6-px step; after 80 steps back to the title screen
	void step();    ///< no per-tick movement exists in the original; does nothing

private:
	RectSprite _bars[16];
	int _next; ///< DAT_00473d28 steps done
};

} // End of namespace Gooka

#endif // GOOKA_EFFECTS_H
