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
 * Text layout/drawing and 2D helpers (see text.h). All drawing is clipped
 * to the given clip rectangle and to the surface, like the original clips
 * to the dirty rectangle it redraws.
 */

#include "common/debug.h"
#include "common/config-manager.h"
#include "common/language.h"

#include "graphics/surface.h"

#include "gooka/detection.h"
#include "gooka/font.h"
#include "gooka/gooka.h"
#include "gooka/text.h"

namespace Gooka {

// ---------------------------------------------------------------------------
// Low-level helpers (int coordinates: the original passes unchecked rects)
// ---------------------------------------------------------------------------

// Intersects (l, t, r, b) with the clip and the surface; false if nothing is left.
static bool clipBox(const Graphics::Surface &dst, const Common::Rect &clip, int &l, int &t, int &r, int &b) {
	l = MAX<int>(l, MAX<int>(clip.left, 0));
	t = MAX<int>(t, MAX<int>(clip.top, 0));
	r = MIN<int>(r, MIN<int>(clip.right, dst.w));
	b = MIN<int>(b, MIN<int>(clip.bottom, dst.h));
	return l < r && t < b;
}

// FUN_00405550 on int coordinates: fills the clipped box with a screen colour.
static void fillBox(Graphics::Surface &dst, const Common::Rect &clip, int l, int t, int r, int b, uint32 color) {
	if (clipBox(dst, clip, l, t, r, b))
		dst.fillRect(Common::Rect(l, t, r, b), color);
}

// FUN_00405940 on int coordinates.
static void roundFrameBox(Graphics::Surface &dst, const Common::Rect &clip, int l, int t, int r, int b, uint32 rgb) {
	const int w = (r - l) - 4;
	const int h = (b - t) - 4;
	hLine(dst, clip, l + 2, t, w, rgb);
	vLine(dst, clip, l, t + 2, h, rgb);
	vLine(dst, clip, r - 1, t + 2, h, rgb);
	hLine(dst, clip, l + 2, b - 1, w, rgb);
	plotPixel(dst, clip, l + 1, t + 1, rgb);
	plotPixel(dst, clip, l + 1, b - 2, rgb);
	plotPixel(dst, clip, r - 2, t + 1, rgb);
	plotPixel(dst, clip, r - 2, b - 2, rgb);
}

// True for the characters that end a laid-out line (FUN_00409b70 / FUN_00409cc0).
static bool isLineEnd(char c) {
	return c == '\n' || c == 0 || c == '\r' || c == '\v';
}

// FUN_00409790 + asm 0x42da3d/0x42da91: up to `count` glyphs (stops at NUL), each
// only `lineHeight` rows high (not 16), clipped per pixel; advances by the glyph width.
static void blitGlyphs(Graphics::Surface &dst, const Common::Rect &clip, const GookaFont &font, int lineHeight,
		int x, int y, const char *text, int count, uint32 color) {
	int l = 0, t = y, r = dst.w, b = y + lineHeight;
	if (!clipBox(dst, clip, l, t, r, b))
		return;
	Graphics::Surface area = dst.getSubArea(Common::Rect(l, t, r, b));
	for (const byte *s = (const byte *)text; *s && count > 0; s++, count--) {
		font.drawChar(&area, *s, x - l, y - t, color);
		x += font.getCharWidth(*s);
	}
}

// FUN_00409940: optional outline (8 neighbour copies in the outline colour), then the text.
// Colours are already in screen format; only bit 0 of `flags` matters here.
static void drawStyled(Graphics::Surface &dst, const Common::Rect &clip, const GookaFont &font, int lineHeight,
		int x, int y, const char *text, int count, uint32 color, uint32 outline, uint32 flags) {
	if (!(flags & kTextOutline)) {
		blitGlyphs(dst, clip, font, lineHeight, x, y, text, count, color);
		return;
	}
	static const int8 kOutline[8][2] = {
		{ 0, 0 }, { 1, 0 }, { 2, 0 }, { 0, 1 }, { 2, 1 }, { 0, 2 }, { 2, 2 }, { 1, 2 }
	};
	for (int i = 0; i < 8; i++)
		blitGlyphs(dst, clip, font, lineHeight, x + kOutline[i][0], y + kOutline[i][1], text, count, outline);
	blitGlyphs(dst, clip, font, lineHeight, x + 1, y + 1, text, count, color);
}

// ---------------------------------------------------------------------------
// Fonts and text
// ---------------------------------------------------------------------------

// Third argument of FUN_004096f0 in FUN_0040e380 (404), FUN_0040e3b0/FUN_0042a8b0 (403), FUN_00410aa0 (405).
int getLineHeight(int fontId) {
	switch (fontId) {
	case kFontBold:
		return 14;
	case kFontRegular:
		return 11;
	case kFontSerif:
		return 16;
	default:
		return 16;
	}
}

// FUN_00409b00
Common::Point measureLine(const char *text, int count, int fontId, uint32 flags) {
	const GookaFont *font = g_engine->getFont(fontId);
	int w = 0;
	for (const byte *s = (const byte *)text; *s && count > 0; s++, count--)
		w += font ? font->getCharWidth(*s) : 0;
	const int extra = (flags & kTextOutline) ? 2 : 0;
	return Common::Point(w + extra, getLineHeight(fontId) + extra);
}

// FUN_00409cc0
Common::Point measureText(const Common::String &text, int fontId, uint32 flags) {
	const char *start = text.c_str();
	int w = 0, h = 0;
	for (const char *p = start; ; p++) {
		if (!isLineEnd(*p))
			continue;
		const Common::Point line = measureLine(start, p - start, fontId, flags);
		h += line.y;
		w = MAX<int>(w, line.x + 1);
		if (*p == 0 || p[1] == 0)
			break;
		start = p + 1;
	}
	if (flags & kTextBoxed) {
		w += 16;
		h += 4;
	}
	return Common::Point(w, h);
}

// FUN_00409b70. Note: for boxed text the lines start at x + 8 but '\r'/'\v' still align
// in `width`, so right-aligned lines overflow the box (as in the original; clipped by the sprite).
void drawTextBlock(Graphics::Surface &dst, const Common::Rect &clip, int x, int y, int width,
		const Common::String &text, int fontId, uint32 color, uint32 outlineColor, uint32 flags) {
	const GookaFont *font = g_engine->getFont(fontId);
	if (!font)
		return;

	if (flags & kTextBoxed) {
		const Common::Point size = measureText(text, fontId, flags);
		fillBox(dst, clip, x + 1, y + 1, x + size.x - 1, y + size.y - 1, rgbToScreen(0xFFFFFF));
		roundFrameBox(dst, clip, x, y, x + size.x, y + size.y, 0);
		x += 8;
		y += 2;
	}

	const int lineHeight = getLineHeight(fontId);
	const uint32 c = rgbToScreen(color);
	const uint32 o = rgbToScreen(outlineColor);
	const char *start = text.c_str();
	for (const char *p = start; ; p++) {
		if (!isLineEnd(*p))
			continue;
		const int count = p - start;
		const Common::Point size = measureLine(start, count, fontId, flags);
		int dx = 0;
		if (*p == '\r')
			dx = width - size.x;
		if (*p == '\v')
			dx = (width - size.x) / 2;
		drawStyled(dst, clip, *font, lineHeight, x + dx, y, start, count, c, o, flags & 0xFF);
		y += size.y;
		if (*p == 0 || p[1] == 0)
			return;
		start = p + 1;
	}
}

// FUN_00409b70 with the text's own width, as the Text sprite (FUN_00409ea0) calls it.
void drawText(Graphics::Surface &dst, const Common::Rect &clip, int x, int y, const Common::String &text,
		int fontId, uint32 color, uint32 outlineColor, uint32 flags) {
	const int width = measureText(text, fontId, flags).x;
	drawTextBlock(dst, clip, x, y, width, text, fontId, color, outlineColor, flags);
}

// FUN_00409a90: FUN_00409940 with an unlimited count; control characters are drawn as glyphs.
void drawTextLine(Graphics::Surface &dst, const Common::Rect &clip, int x, int y, const Common::String &text,
		int fontId, uint32 color, uint32 outlineColor, uint32 flags) {
	const GookaFont *font = g_engine->getFont(fontId);
	if (!font)
		return;
	drawStyled(dst, clip, *font, getLineHeight(fontId), x, y, text.c_str(), 0x7FFFFFFF,
		rgbToScreen(color), rgbToScreen(outlineColor), flags & 0xFF);
}

// Scene renderer FUN_00408ed0: draw(clip) gets the sprite rectangle ∩ the dirty rectangle.
Common::Rect getSpriteClip(const Sprite &sprite, const Graphics::Surface &dst) {
	const Common::Point size = sprite.getSize();
	const int l = MAX<int>(sprite.x, 0);
	const int t = MAX<int>(sprite.y, 0);
	const int r = MIN<int>(sprite.x + size.x, dst.w);
	const int b = MIN<int>(sprite.y + size.y, dst.h);
	if (l >= r || t >= b)
		return Common::Rect();
	return Common::Rect(l, t, r, b);
}

// ---------------------------------------------------------------------------
// TextSprite / LabelSprite
// ---------------------------------------------------------------------------

// Text constructor FUN_0040ff90: empty text, colours and flags 0.
TextSprite::TextSprite() : _fontId(kFontBold), _color(0), _outlineColor(0), _flags(0), _size(0, 0) {
}

// FUN_00409db0 (text) + FUN_00409e20 (font) + FUN_00409e70 (colours) + FUN_00409e30 (flags).
void TextSprite::setText(const Common::String &text, int fontId, uint32 color, uint32 outlineColor, uint32 flags) {
	_text = text;
	// English-only soft wrapping. Keep explicit alignment and paragraph controls.
	if (Common::parseLanguage(ConfMan.get("language")) == Common::EN_ANY) {
		const GookaFont *font = g_engine->getFont(fontId);
		uint lineStart = 0;
		uint lastSpace = 0;
		int width = 0;
		for (uint i = 0; font && i < _text.size(); ++i) {
			if (isLineEnd(_text[i])) {
				lineStart = i + 1;
				lastSpace = 0;
				width = 0;
				continue;
			}
			if (_text[i] == ' ')
				lastSpace = i;
			width += font->getCharWidth((byte)_text[i]);
			if (width > 590 && lastSpace > lineStart) {
				_text.setChar('\n', lastSpace);
				i = lastSpace;
				lineStart = i + 1;
				lastSpace = 0;
				width = 0;
			}
		}
	}
	_fontId = fontId;
	_color = color;
	_outlineColor = outlineColor;
	_flags = flags;
	updateSize();
	debugC(5, kDebugGraphics, "TextSprite: '%s' font %d, %dx%d", _text.c_str(), _fontId, _size.x, _size.y);
}

// Not in the original (it hides the sprite instead): an empty size draws nothing.
void TextSprite::clearText() {
	_text.clear();
	_size = Common::Point(0, 0);
}

// FUN_00409e70
void TextSprite::setColors(uint32 color, uint32 outlineColor) {
	_color = color;
	_outlineColor = outlineColor;
}

// FUN_00409d50
Common::Point TextSprite::getSize() const {
	return _size;
}

// FUN_00409ea0
void TextSprite::draw(Graphics::Surface &dst) {
	const Common::Rect clip = getSpriteClip(*this, dst);
	if (clip.isEmpty())
		return;
	drawTextBlock(dst, clip, x, y, _size.x, _text, _fontId, _color, _outlineColor, _flags);
}

// FUN_00409d70
void TextSprite::updateSize() {
	_size = measureText(_text, _fontId, _flags);
}

// FUN_00412a80: box in the outline colour, border and text in the text colour.
void LabelSprite::draw(Graphics::Surface &dst) {
	const Common::Rect clip = getSpriteClip(*this, dst);
	if (clip.isEmpty())
		return;
	const int w = _size.x;
	const int h = _size.y;
	fillBox(dst, clip, x + 1, y + 1, x + w - 1, y + h - 1, rgbToScreen(_outlineColor));
	hLine(dst, clip, x + 1, y, w - 2, _color);
	hLine(dst, clip, x + 1, y + h - 1, w - 2, _color);
	vLine(dst, clip, x, y + 1, h - 2, _color);
	vLine(dst, clip, x + w - 1, y + 1, h - 2, _color);
	drawTextLine(dst, clip, x + 3, y + 2, _text, _fontId, _color, _outlineColor, _flags & 0xFF);
}

// FUN_00412b60
void LabelSprite::updateSize() {
	TextSprite::updateSize();
	_size.x += 5;
	_size.y += 4;
}

// ---------------------------------------------------------------------------
// 2D helpers
// ---------------------------------------------------------------------------

// FUN_004052a0 (16-bit branch): 8-bit channels scaled to 5 bits, low byte = red.
uint32 rgbToScreen(uint32 rgb) {
	const uint r = (rgb & 0xFF) * 31 / 255;
	const uint g = ((rgb >> 8) & 0xFF) * 31 / 255;
	const uint b = ((rgb >> 16) & 0xFF) * 31 / 255;
	return g_engine->_lut[(r << 10) | (g << 5) | b];
}

// FUN_004054e0
void plotPixel(Graphics::Surface &dst, const Common::Rect &clip, int x, int y, uint32 rgb) {
	fillBox(dst, clip, x, y, x + 1, y + 1, rgbToScreen(rgb));
}

// FUN_00405840 = FUN_00405620(x, y, w, 1)
void hLine(Graphics::Surface &dst, const Common::Rect &clip, int x, int y, int w, uint32 rgb) {
	fillBox(dst, clip, x, y, x + w, y + 1, rgbToScreen(rgb));
}

// FUN_004057a0
void vLine(Graphics::Surface &dst, const Common::Rect &clip, int x, int y, int h, uint32 rgb) {
	fillBox(dst, clip, x, y, x + 1, y + h, rgbToScreen(rgb));
}

// FUN_00405550
void fillRectRGB(Graphics::Surface &dst, const Common::Rect &clip, const Common::Rect &r, uint32 rgb) {
	fillBox(dst, clip, r.left, r.top, r.right, r.bottom, rgbToScreen(rgb));
}

// FUN_00405870: the left edge is one pixel shorter so the dark bottom edge owns the corner.
void frame3D(Graphics::Surface &dst, const Common::Rect &clip, const Common::Rect &r, uint32 light, uint32 dark, int inset) {
	const int w = (r.right - r.left) - inset * 2;
	const int h = (r.bottom - r.top) - inset * 2;
	hLine(dst, clip, r.left + inset, r.top + inset, w, light);
	vLine(dst, clip, r.left + inset, r.top + inset, h - 1, light);
	vLine(dst, clip, r.right - inset - 1, r.top + inset, h, dark);
	hLine(dst, clip, r.left + inset, r.bottom - inset - 1, w, dark);
}

// FUN_00405920
void flatFrame(Graphics::Surface &dst, const Common::Rect &clip, const Common::Rect &r, uint32 rgb) {
	frame3D(dst, clip, r, rgb, rgb, 0);
}

// FUN_00405940
void roundFrame(Graphics::Surface &dst, const Common::Rect &clip, const Common::Rect &r, uint32 rgb) {
	roundFrameBox(dst, clip, r.left, r.top, r.right, r.bottom, rgb);
}

// FUN_00405a20 (raised) / FUN_00405ad0 (sunken)
void panel(Graphics::Surface &dst, const Common::Rect &clip, const Common::Rect &r, uint32 light, uint32 dark,
		uint32 fill, uint32 edge, bool sunken) {
	fillRectRGB(dst, clip, r, fill);
	if (sunken) {
		frame3D(dst, clip, r, dark, light, 0);
		frame3D(dst, clip, r, edge, edge, 1);
	} else {
		frame3D(dst, clip, r, edge, edge, 0);
		frame3D(dst, clip, r, light, dark, 1);
	}
}

} // End of namespace Gooka
