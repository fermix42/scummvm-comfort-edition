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
 * Text rendering with the game's bitmap fonts (original Font/Text classes,
 * FUN_004096f0, FUN_00409b70 layout, FUN_00409940 drawing) and the small
 * 2D drawing helpers used by the UI and a few rooms.
 *
 * Colours: like the original (GDI COLORREF), every colour is 0x00BBGGRR,
 * i.e. the LOW byte is red. The constants can be copied from GAME.c as they
 * are: 0xffffff = white, 0x00ffff = yellow, 0xff0000 = blue. Bits 24..31
 * (the COLORREF palette flags) are ignored.
 */

#ifndef GOOKA_TEXT_H
#define GOOKA_TEXT_H

#include "common/rect.h"
#include "common/str.h"

#include "gooka/sprite.h"

namespace Graphics {
struct Surface;
}

namespace Gooka {

/** Font resources; the line heights come from the font constructors (FUN_004096f0). */
enum {
	kFontBold = 403,    ///< line height 14 (subtitles, book pages, menu)
	kFontRegular = 404, ///< line height 11 (hover names)
	kFontSerif = 405    ///< line height 16 (credits)
};

/** Text style flags (FUN_00409940 / FUN_00409b70). */
enum TextFlags {
	kTextOutline = 1,         ///< draw the outline colour at the 8 neighbours first, text at (+1, +1)
	kTextBoxed = 0x10000000   ///< white filled box with a rounded black frame, text inset by (8, 2)
};

/** Line height of a font (third argument of FUN_004096f0): 403 → 14, 404 → 11, 405 → 16. */
int getLineHeight(int fontId);

/**
 * Multi-line text laid out like the original (vtable 0x4375f8): lines end
 * with '\n' (left-aligned), '\r' (the line is right-aligned) or '\v' (0x0b,
 * the line is centred); a string ending in a terminator has no empty last line.
 */
class TextSprite : public Sprite {
public:
	TextSprite();

	/** Sets the CP1250 text and style; the size is recomputed. */
	void setText(const Common::String &text, int fontId = kFontBold, uint32 color = 0xFFFFFF,
		uint32 outlineColor = 0, uint32 flags = 0);
	/** Removes the text (draws nothing). */
	void clearText();
	/** The current text. */
	const Common::String &getText() const { return _text; }
	/** Changes the text and outline colours only (FUN_00409e70). */
	void setColors(uint32 color, uint32 outlineColor);

	Common::Point getSize() const override;
	/** Lays the text out in its own width, clipped to its bounds (FUN_00409ea0). */
	void draw(Graphics::Surface &dst) override;

protected:
	/** Recomputes _size from the text (FUN_00409d70, virtual slot 3 of the original). */
	virtual void updateSize();

	Common::String _text;
	int _fontId;
	uint32 _color;        ///< +0x34 text colour
	uint32 _outlineColor; ///< +0x38 outline colour
	uint32 _flags;        ///< +0x44 TextFlags
	Common::Point _size;  ///< +0x3c/+0x40 laid-out size in pixels
};

/**
 * Hover-name label (vtable 0x437970, ctor FUN_00412b90): the text in colour
 * `color` on a box filled with `outlineColor`, framed by a 1-px border in the
 * text colour (corners left out); single line, text at (+3, +2).
 * The original hover label (FUN_0041be90) uses font 404, text colour 0 and
 * the box colour of the cursor type.
 */
class LabelSprite : public TextSprite {
public:
	/** FUN_00412a80. */
	void draw(Graphics::Surface &dst) override;

protected:
	/** Text size + (5, 4) (FUN_00412b60). */
	void updateSize() override;
};

/** Size of one line of at most `count` characters, +2 in both directions with kTextOutline (FUN_00409b00). */
Common::Point measureLine(const char *text, int count, int fontId, uint32 flags = 0);

/** Size of a laid-out text: widest line + 1, sum of line heights, +16/+4 with kTextBoxed (FUN_00409cc0). */
Common::Point measureText(const Common::String &text, int fontId, uint32 flags = 0);

/**
 * Lays out and draws a text at (x, y), clipped to `clip` (FUN_00409b70).
 * `width` is the width '\r' and '\v' lines are aligned in (without the box inset).
 */
void drawTextBlock(Graphics::Surface &dst, const Common::Rect &clip, int x, int y, int width,
	const Common::String &text, int fontId, uint32 color, uint32 outlineColor = 0, uint32 flags = 0);

/** Draws a laid-out text with its top-left corner at (x, y), aligned in its own measured width, clipped to `clip`. */
void drawText(Graphics::Surface &dst, const Common::Rect &clip, int x, int y, const Common::String &text,
	int fontId, uint32 color, uint32 outlineColor = 0, uint32 flags = 0);

/**
 * Draws the characters as they are, without line breaks or box (FUN_00409a90);
 * only kTextOutline of `flags` is used.
 */
void drawTextLine(Graphics::Surface &dst, const Common::Rect &clip, int x, int y, const Common::String &text,
	int fontId, uint32 color, uint32 outlineColor = 0, uint32 flags = 0);

/** Clip rectangle the original renderer passes to a sprite: its bounds ∩ the surface. */
Common::Rect getSpriteClip(const Sprite &sprite, const Graphics::Surface &dst);

/** Converts a colour (0x00BBGGRR) to the screen format via RGB555 like FUN_004052a0 (16-bit mode). */
uint32 rgbToScreen(uint32 rgb);

/** Sets one pixel (FUN_004054e0). */
void plotPixel(Graphics::Surface &dst, const Common::Rect &clip, int x, int y, uint32 rgb);
/** Horizontal line of `w` pixels (FUN_00405840). */
void hLine(Graphics::Surface &dst, const Common::Rect &clip, int x, int y, int w, uint32 rgb);
/** Vertical line of `h` pixels (FUN_004057a0). */
void vLine(Graphics::Surface &dst, const Common::Rect &clip, int x, int y, int h, uint32 rgb);

/** Fills a rectangle (FUN_00405550; FUN_00405620 is the x, y, w, h form). */
void fillRectRGB(Graphics::Surface &dst, const Common::Rect &clip, const Common::Rect &r, uint32 rgb);
/**
 * 1-px 3D border inset by `inset`: `light` top and left, `dark` right and bottom,
 * drawn in that order (FUN_00405870).
 */
void frame3D(Graphics::Surface &dst, const Common::Rect &clip, const Common::Rect &r, uint32 light, uint32 dark, int inset);
/** 1-px border in one colour (FUN_00405920). */
void flatFrame(Graphics::Surface &dst, const Common::Rect &clip, const Common::Rect &r, uint32 rgb);
/** 1-px border with cut corners (a pixel inside each corner), as around boxed text (FUN_00405940). */
void roundFrame(Graphics::Surface &dst, const Common::Rect &clip, const Common::Rect &r, uint32 rgb);
/**
 * Panel = fill + two 1-px borders.
 * Raised (FUN_00405a20): flat `edge` border outside, 3D border (light/dark) inside.
 * Sunken (FUN_00405ad0): inverted 3D border (dark/light) outside, flat `edge` border inside.
 */
void panel(Graphics::Surface &dst, const Common::Rect &clip, const Common::Rect &r, uint32 light, uint32 dark,
	uint32 fill, uint32 edge, bool sunken);

} // End of namespace Gooka

#endif // GOOKA_TEXT_H
