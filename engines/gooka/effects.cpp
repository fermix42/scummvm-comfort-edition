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
 * Special-effect sprites (see effects.h). Like the original renderer, every
 * draw() is clipped to the sprite's own rectangle (getSpriteClip()).
 */

#include "common/debug.h"

#include "graphics/surface.h"

#include "gooka/detection.h"
#include "gooka/effects.h"
#include "gooka/game.h"
#include "gooka/gooka.h"
#include "gooka/image.h"
#include "gooka/resource.h"

namespace Gooka {

/** Pixel value marking transparent pixels in a decoded frame (decoded pixels are <= 0x7FFF). */
static const uint16 kTransparent = 0xFFFF;

// FUN_00407e70 through the game; 0 while the game does not exist yet (sprites that are
// constructed early get their real values from Game::placeShimmer() / the first step()).
static int rnd(int n) {
	return (g_engine && g_engine->_game) ? g_engine->_game->rnd(n) : 0;
}

// Decodes a frame into `pixels` (image width x height, raw RGB555, kTransparent elsewhere).
static void decodeImageFrame(const Image &img, int frame, Common::Array<uint16> &pixels) {
	const int w = img.getWidth();
	const int h = img.getHeight();
	pixels.resize(w * h);
	for (uint i = 0; i < pixels.size(); i++)
		pixels[i] = kTransparent;
	if (w <= 0 || h <= 0)
		return;
	Graphics::Surface s;
	s.init(w, h, w * 2, pixels.data(), Graphics::PixelFormat(2, 5, 5, 5, 0, 10, 5, 0, 0));
	img.draw(s, frame, 0, 0, nullptr);
}

// Frame record of FUN_00407f00: width = widest row, height = last non-empty row + 1.
// Deviation: measured on opaque pixels, the original sums skip+count (a row ending in a
// bare skip would count as wider there).
static Common::Point contentSize(const Common::Array<uint16> &pixels, int w, int h) {
	Common::Point size(0, 0);
	for (int row = 0; row < h; row++) {
		for (int col = w - 1; col >= 0; col--) {
			if (pixels[row * w + col] != kTransparent) {
				size.x = MAX<int>(size.x, col + 1);
				size.y = row + 1;
				break;
			}
		}
	}
	return size;
}

// FUN_004053c0: the grey part (minimum channel) scaled by `grey` %, the colour part by `chroma` %.
static uint32 adjustColor(uint32 rgb, int grey, int chroma) {
	const int r = rgb & 0xFF;
	const int g = (rgb >> 8) & 0xFF;
	const int b = (rgb >> 16) & 0xFF;
	const int m = MIN<int>(r, MIN<int>(g, b));
	const int base = m * grey / 100;
	const int nb = MIN<int>((b - m) * chroma / 100 + base, 0xFF);
	const int ng = MIN<int>((g - m) * chroma / 100 + base, 0xFF);
	const int nr = MIN<int>((r - m) * chroma / 100 + base, 0xFF);
	return (nb << 16) | (ng << 8) | nr;
}

// Line terminators of the text layout (FUN_0040ffe0 uses the same set as FUN_00409b70).
static bool isLineEnd(char c) {
	return c == '\n' || c == 0 || c == '\r' || c == '\v';
}

// ---------------------------------------------------------------------------
// ShimmerSprite
// ---------------------------------------------------------------------------

// FUN_00412690 (amplitude, direction, phase 0, init) or FUN_00417080 (phase 0 only) for amplitude 0.
ShimmerSprite::ShimmerSprite(int amp, int flag) : width(0), height(0), amplitude(amp), sheetFrame(0),
		phase(0), direction(flag), _waveCounter(0), _decodedSheet(nullptr), _decodedFrame(-1),
		_frameWidth(0), _frameHeight(0) {
	memset(_offsets, 0, sizeof(_offsets));
	if (amp != 0)
		init();
}

// rnd(amplitude) - amplitude / 2, truncated to a byte like the original.
int8 ShimmerSprite::randomOffset() {
	return (int8)(rnd(amplitude) - amplitude / 2);
}

// FUN_00412570
int8 ShimmerSprite::nextWave() {
	_waveCounter++;
	return (int8)Game::sinScaled(8, _waveCounter * 16);
}

// FUN_004125f0. The room sheet may have been reloaded, so the decoded frame is dropped too.
void ShimmerSprite::init() {
	for (int i = 0; i < 400; i++)
		_offsets[i] = randomOffset();
	_decodedSheet = nullptr;
}

// FUN_00412430
void ShimmerSprite::step() {
	if (direction == 0) {
		// Interlaced: even rows move up, odd rows move down, one new value at each end
		for (int i = 0; i < 398; i += 2)
			_offsets[i] = _offsets[i + 2];
		for (int i = 399; i > 1; i -= 2)
			_offsets[i] = _offsets[i - 2];
		// (the original also copies the byte before the table into row 1; it is overwritten here)
		_offsets[1] = randomOffset();
		_offsets[398] = randomOffset();
		return;
	}

	// Deviation: |direction| >= 400 would make the original memmove run wild; clamped
	const int n = CLIP<int>(ABS(direction), 0, 400);
	if (direction < 0) {
		memmove(_offsets, _offsets + n, 400 - n);
		for (int i = 0; i < n; i++)
			_offsets[400 - n + i] = randomOffset();
	} else {
		memmove(_offsets + n, _offsets, 400 - n);
		for (int i = 0; i < n; i++)
			_offsets[i] = randomOffset();
	}
}

// FUN_00412620: filled from the end the table scrolls away from.
void ShimmerSprite::regenerate() {
	_waveCounter = 0;
	if (direction < 0) {
		for (int i = 0; i < 400; i++)
			_offsets[i] = nextWave();
	} else {
		for (int i = 399; i >= 0; i--)
			_offsets[i] = nextWave();
	}
}

// FUN_00412590
void ShimmerSprite::scroll() {
	if (direction > 0) {
		memmove(_offsets + 1, _offsets, 399);
		_offsets[0] = nextWave();
	} else {
		memmove(_offsets, _offsets + 1, 399);
		_offsets[399] = nextWave();
	}
}

// FUN_00412c10
Common::Point ShimmerSprite::getSize() const {
	return Common::Point(width, height);
}

// Replaces the original's per-row pointer table (FUN_00407f00) by a decoded copy of the frame.
void ShimmerSprite::decodeFrame() {
	if (_decodedSheet == sheet && _decodedFrame == sheetFrame && _frameWidth == sheet->getWidth() &&
			_pixels.size() == (uint)(sheet->getWidth() * sheet->getHeight()))
		return;
	_decodedSheet = sheet;
	_decodedFrame = sheetFrame;
	_frameWidth = sheet->getWidth();
	decodeImageFrame(*sheet, sheetFrame, _pixels);
	_frameHeight = contentSize(_pixels, _frameWidth, sheet->getHeight()).y;
	debugC(3, kDebugGraphics, "Shimmer: frame %d decoded, %dx%d", sheetFrame, _frameWidth, _frameHeight);
}

// FUN_004121c0: screen row r shows source row (phase + r - y) % frameHeight shifted by offsets[r - y].
void ShimmerSprite::draw(Graphics::Surface &dst) {
	if (!sheet)
		return;
	const Common::Rect clip = getSpriteClip(*this, dst);
	if (clip.isEmpty())
		return;
	decodeFrame();
	if (_frameHeight <= 0)
		return;

	const uint16 *lut = g_engine->_lut;
	for (int row = clip.top; row < clip.bottom; row++) {
		const int index = row - y;
		// Deviation: the original reads past the 400-entry table for taller frames
		if (index < 0 || index >= _frameHeight || index >= 400)
			continue;
		// Deviation: a negative phase would index before the row table in the original
		int srcRow = (phase + index) % _frameHeight;
		if (srcRow < 0)
			srcRow += _frameHeight;

		const uint16 *src = &_pixels[srcRow * _frameWidth];
		const int left = x + _offsets[index];
		const int from = MAX<int>(clip.left, left);
		const int to = MIN<int>(clip.right, left + _frameWidth);
		uint16 *line = (uint16 *)dst.getBasePtr(0, row);
		for (int dx = from; dx < to; dx++) {
			const uint16 c = src[dx - left];
			if (c != kTransparent)
				line[dx] = lut[c];
		}
	}
}

// ---------------------------------------------------------------------------
// ParticleSprite
// ---------------------------------------------------------------------------

// FUN_00412910: all particles start outside the box, so the first step spawns them.
ParticleSprite::ParticleSprite(int count) : width(0), height(0), _count(MAX(count, 0)) {
	_particles = new Particle[MAX(_count, 1)];
	for (int i = 0; i < _count; i++) {
		_particles[i].x = _particles[i].y = 999999;
		_particles[i].color = 0;
		_particles[i].dx = _particles[i].dy = 0;
	}
	step();
}

// FUN_00412a00
ParticleSprite::~ParticleSprite() {
	delete[] _particles;
}

// FUN_004127a0 (the order of the rnd() calls is the original's).
void ParticleSprite::step() {
	for (int i = 0; i < _count; i++) {
		Particle &p = _particles[i];
		const bool respawn = rnd(_count >> 1) == 0 || p.x < 0 || p.x >= width || p.y < 0 || p.y >= height;
		if (!respawn) {
			p.x += p.dx;
			p.y += p.dy;
			continue;
		}
		p.y = rnd(height);
		p.x = rnd(width);
		const int b = rnd(0x100);
		const int g = rnd(0x100);
		const int r = rnd(0x100);
		p.color = rgbToScreen((b << 16) | (g << 8) | r);
		const int vy = rnd(5);
		const int vx = rnd(5);
		p.dx = vx - 2;
		p.dy = vy - 2;
		if (p.dx == 0 && p.dy == 0)
			p.dx = 1;
	}
}

// FUN_004126f0
Common::Point ParticleSprite::getSize() const {
	return Common::Point(width, height);
}

// FUN_00412710
void ParticleSprite::draw(Graphics::Surface &dst) {
	const Common::Rect clip = getSpriteClip(*this, dst);
	if (clip.isEmpty())
		return;
	for (int i = 0; i < _count; i++) {
		const int px = x + _particles[i].x;
		const int py = y + _particles[i].y;
		if (px >= clip.left && px < clip.right && py >= clip.top && py < clip.bottom)
			*(uint16 *)dst.getBasePtr(px, py) = _particles[i].color;
	}
}

// ---------------------------------------------------------------------------
// CounterSprite (the book)
// ---------------------------------------------------------------------------

// FUN_0042a830
CounterSprite::CounterSprite(int a, int m) : param(a), max(m) {
	reset();
}

// FUN_00410200
void CounterSprite::increment() {
	if (frame < max) {
		g_engine->_game->playSound(nullptr, 0xf1a);
		frame++;
	}
}

// FUN_00410240
void CounterSprite::decrement() {
	if (frame > 0) {
		g_engine->_game->playSound(nullptr, 0xf1a);
		frame--;
	}
}

// FUN_004101f0
void CounterSprite::reset() {
	frame = 0;
}

// FUN_0040ffc0
Common::Point CounterSprite::getSize() const {
	return Common::Point(640, 400);
}

// FUN_004101b0
void CounterSprite::draw(Graphics::Surface &dst) {
	const Common::Rect clip = getSpriteClip(*this, dst);
	if (clip.isEmpty())
		return;
	drawColumn(dst, clip, 20, param + frame * 2);
	drawColumn(dst, clip, 360, param + 1 + frame * 2);
}

// FUN_0040ffe0: the column x is absolute (the sprite's x is not added), lines start at y + 28.
void CounterSprite::drawColumn(Graphics::Surface &dst, const Common::Rect &clip, int columnX, int stringId) {
	if (stringId == 0)
		return;
	const Common::String text = g_engine->_res->getText(stringId);
	int lineY = y + 28;
	const char *start = text.c_str();
	for (const char *p = start; ; p++) {
		if (!isLineEnd(*p))
			continue;

		if (*start == '@') {
			// "@n": frame n of the room sheet, centred by its own content size
			const int frameNo = atoi(start + 1);
			Image *roomSheet = g_engine->_game->roomSheet();
			Common::Array<uint16> pixels;
			decodeImageFrame(*roomSheet, frameNo, pixels);
			const Common::Point size = contentSize(pixels, roomSheet->getWidth(), roomSheet->getHeight());
			Graphics::Surface area = dst.getSubArea(clip);
			roomSheet->draw(area, frameNo, (250 - size.x) / 2 + columnX - clip.left, lineY - clip.top, g_engine->_lut);
			lineY += size.y;
		} else {
			// Black text, no outline (the outline colour 0x01000000 is unused)
			const Common::Point size = measureLine(start, p - start, kFontBold, 0);
			int dx = 0;
			if (*p == '\r')
				dx = 250 - size.x;
			if (*p == '\v')
				dx = (250 - size.x) / 2;
			drawTextLine(dst, clip, columnX + dx, lineY, Common::String(start, p), kFontBold, 0, 0x01000000, 0);
			lineY += size.y;
		}

		if (*p == 0 || p[1] == 0)
			return;
		start = p + 1;
	}
}

// ---------------------------------------------------------------------------
// SparkleSprite
// ---------------------------------------------------------------------------

// FUN_00411fe0: colour 0, then FUN_00411fc0.
SparkleSprite::Spark::Spark() : cx(0), cy(0), size(-1), color(0), delta(0) {
}

// FUN_00411fc0
void SparkleSprite::Spark::stop() {
	hide();
	size = -1;
}

// FUN_00411f70
void SparkleSprite::Spark::spawn(int px, int py, int prio, int subPrio, uint32 rgb) {
	priority = prio;
	subPriority = subPrio;
	cx = px;
	cy = py;
	color = rgb;
	size = 0;
	delta = 1;
	x = px;
	y = py;
	show();
}

// FUN_00411f10: grows every tick up to 5, then shrinks every other tick.
void SparkleSprite::Spark::tick() {
	if (size < 0 || (delta < 0 && (g_engine->_tickCounter & 1)))
		return;
	size += delta;
	if (size > 4)
		delta = -1;
	if (size < 0) {
		hide();
		delta = 0;
	}
	if (size >= 0) {
		x = cx - size;
		y = cy - size;
	}
}

// FUN_00411e70
Common::Point SparkleSprite::Spark::getSize() const {
	return Common::Point(size * 2, size * 2);
}

// FUN_00411e90: brightness 50 % at size 0 up to 112 % at size 5.
void SparkleSprite::Spark::draw(Graphics::Surface &dst) {
	if (size == -1)
		return;
	const Common::Rect clip = getSpriteClip(*this, dst);
	if (clip.isEmpty())
		return;
	const int percent = size * 50 / 4 + 50;
	const uint32 c = adjustColor(color, percent, percent);
	vLine(dst, clip, cx, cy - size, size * 2, c);
	hLine(dst, clip, cx - size, cy, size * 2, c);
}

// FUN_00412160
SparkleSprite::SparkleSprite() : _remaining(0), _x(0), _y(0), _priority(0), _spread(0), _color(0) {
	reset();
}

// FUN_00412080
void SparkleSprite::start(int px, int py, int prio, int count, int duration, int param) {
	_remaining = duration;
	_x = px;
	_y = py;
	_priority = prio;
	_spread = count;
	_color = param;
	debugC(3, kDebugGraphics, "Sparkle at %d,%d for %d ticks", px, py, duration);
}

// FUN_00412050
void SparkleSprite::reset() {
	for (int i = 0; i < 16; i++)
		_sparks[i].stop();
	_remaining = 0;
}

// FUN_004120c0: an idle spark respawns with probability 1/8 while the burst lasts.
void SparkleSprite::step() {
	for (int i = 0; i < 16; i++) {
		Spark &s = _sparks[i];
		s.tick();
		if (s.size >= 0)
			continue;
		if (rnd(8) == 0 && _remaining != 0) {
			const int sy = rnd(_spread * 2) + (_y - _spread);
			const int sx = rnd(_spread * 2) + (_x - _spread);
			s.spawn(sx, sy, _priority, i, _color);
		}
	}
	if (_remaining > 0)
		_remaining--;
}

// The container is not drawn: every spark is its own scene sprite.
Common::Point SparkleSprite::getSize() const {
	return Common::Point(0, 0);
}

// See getSize().
void SparkleSprite::draw(Graphics::Surface &dst) {
}

// ---------------------------------------------------------------------------
// RectSprite / FrameSprite
// ---------------------------------------------------------------------------

// FUN_00412e10
RectSprite::RectSprite() : width(0), height(0), color(0) {
}

// FUN_00412c10
Common::Point RectSprite::getSize() const {
	return Common::Point(width, height);
}

// FUN_00409590: the filled area is exactly the clip rectangle.
void RectSprite::draw(Graphics::Surface &dst) {
	const Common::Rect clip = getSpriteClip(*this, dst);
	if (!clip.isEmpty())
		dst.fillRect(clip, rgbToScreen(color));
}

// Plain sprite constructor (CMetaFileDC::CMetaFileDC at 0x41be30).
FrameSprite::FrameSprite() : width(0), height(0) {
}

// FUN_00412c10
Common::Point FrameSprite::getSize() const {
	return Common::Point(width, height);
}

// FUN_00412c30
void FrameSprite::draw(Graphics::Surface &dst) {
	const Common::Rect clip = getSpriteClip(*this, dst);
	if (clip.isEmpty())
		return;
	roundFrame(dst, clip, Common::Rect(x, y, x + width, y + height), 0);
	// Deviation: skipped below 2x2, where the original draws a few stray pixels
	if (width >= 2 && height >= 2)
		roundFrame(dst, clip, Common::Rect(x + 1, y + 1, x + width - 1, y + height - 1), 0xFFFF);
}

// ---------------------------------------------------------------------------
// EndTextScroller (game-over blinds)
// ---------------------------------------------------------------------------

EndTextScroller::EndTextScroller() : _next(0) {
}

// FUN_00412d30: colour DAT_004481b4 is black.
void EndTextScroller::reset() {
	for (int i = 0; i < 16; i++) {
		g_engine->_game->registerHotspot(&_bars[i], -1);
		_bars[i].width = 0;
		_bars[i].height = 0;
		_bars[i].color = 0;
	}
	_next = 0;
}

// FUN_00412d80. FUN_0041ca30 also runs FUN_0041df70 (stop sounds, hide hero/inventory/cursor,
// leave the room) before the title screen; that is left to setState().
void EndTextScroller::addLine() {
	if (_next >= 80) {
		g_engine->setState(kStateTitle);
		return;
	}
	const int bar = _next / 5;
	RectSprite &r = _bars[bar];
	r.show();
	r.x = 0;
	r.priority = 999999;
	r.subPriority = 0;
	r.y = bar * 30;
	r.width = 640;
	r.height = (_next % 5 + 1) * 6;
	_next++;
}

// The bars only change in addLine().
void EndTextScroller::step() {
}

} // End of namespace Gooka
