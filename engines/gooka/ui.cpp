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
 * The player's side of the game: Game::tick (FUN_0041d280) with cursor,
 * hover names, inventory handling and clicks, the in-game menu built with
 * the original GUI toolkit's look, the title screen and the credits.
 */

#include "common/config-manager.h"
#include "common/debug.h"
#include "common/savefile.h"
#include "common/str-enc.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/ustr.h"

#include "engines/metaengine.h"

#include "graphics/surface.h"

#include "gooka/detection.h"
#include "gooka/game.h"
#include "gooka/gooka.h"
#include "gooka/image.h"
#include "gooka/music.h"
#include "gooka/resource.h"
#include "gooka/scene.h"
#include "gooka/sound.h"
#include "gooka/ui.h"

namespace Gooka {

/** Images, frames and render priorities used by the UI. */
enum {
	kCursorSheet = 402,       ///< cursor shapes and GUI textures
	kIconSheet = 407,         ///< inventory icons (frame = item + 10)
	kTitleImage = 464,
	kCreditsImage = 4000,     ///< background and text id of the credits
	kCreditsPictures = 4001,  ///< "@n" lines of the credits
	kTitleMusic = 435,
	kCreditsMusic = 434,

	kCursorDefault = 0,       ///< DAT_0044837c: arrow (the file dialog's edit box set it to 2, the I-beam, for good)
	kCursorScrollLeft = 3,
	kCursorScrollRight = 4,
	kCursorArrows = 29,     ///< + cursor type: 1..4 exit arrows, 5 hand
	kCursorSpin = 35,       ///< cursor type 9: 4 frames

	kTexWindow = 24,        ///< window background texture
	kTexCaption = 25,       ///< title bar of the active window
	kTexButton = 27,
	kTexButtonCancel = 28,
	kTexButtonOk = 29,

	kPrioCursor = 10000,
	kPrioGui = 1004,         ///< GUI root DAT_004482e8 (FUN_004072b0)
	kPrioHover = 1003,
	kPrioHoverFrames = 1002,
	kPrioHeldItem = 1001,

	kMaxSkipSteps = 20000     ///< port only: safety limit of the Esc fast-forward loop
};

/** GUI style DAT_004450e0 (colours are COLORREF 0x00BBGGRR). */
enum {
	kColBlack = 0x000000,     ///< +0x0c text and outer border
	kColWhite = 0xFFFFFF,     ///< +0x18 light edge, active caption text
	kColGray = 0x808080       ///< +0x1c dark edge
};

/** Direction of the animated exit arrows per cursor type 1..5 (table 0x441a00). */
static const int8 kArrowDirs[5][2] = {
	{ -1, 0 }, { 0, -1 }, { 1, 0 }, { 0, 1 }, { 0, 0 }
};

/** String resource, empty if missing or the id is out of range (FUN_00407de0). */
static Common::String getString(int id) {
	if (id <= 0 || id > 0xFFFF)
		return Common::String();
	return g_engine->_res->getText(id);
}

/** Tiles a frame of the GUI sheet over `r`, starting at its top-left corner (FUN_00405770 / FUN_00405670). */
static void tileFrame(Graphics::Surface &dst, const Common::Rect &clip, const Common::Rect &r, int frame) {
	Image *img = g_engine->getImage(kCursorSheet);
	if (!img)
		return;
	Common::Rect area = r;
	area.clip(clip);
	area.clip(Common::Rect(dst.w, dst.h));
	const Common::Point size = img->getFrameSize(frame);
	if (area.isEmpty() || size.x <= 0 || size.y <= 0)
		return;

	Graphics::Surface sub = dst.getSubArea(area);
	for (int ty = r.top; ty < r.bottom; ty += size.y) {
		if (ty + size.y <= area.top || ty >= area.bottom)
			continue;
		for (int tx = r.left; tx < r.right; tx += size.x) {
			if (tx + size.x > area.left && tx < area.right)
				img->draw(sub, frame, tx - area.left, ty - area.top, g_engine->_lut);
		}
	}
}

/** Size of a message text: widest line and the sum of the line heights (FUN_0040ac00). */
static Common::Point measureMessage(const Common::String &text) {
	Common::Point size(0, 0);
	const char *start = text.c_str();
	for (const char *p = start;; p++) {
		if (*p == '\n' || *p == '\r' || *p == '\v' || *p == 0) {
			const Common::Point line = measureLine(start, p - start, kFontBold, 0);
			size.x = MAX<int16>(size.x, line.x);
			size.y += line.y;
			if (*p == 0 || p[1] == 0)
				break;
			start = p + 1;
		}
	}
	return size;
}

// ---------------------------------------------------------------------------
// GUI window
// ---------------------------------------------------------------------------

// Frame windows are children of the GUI root: priority 0x3ec, sub-priority 1.
Ui::GuiWindow::GuiWindow() : id(0), captured(-1) {
	priority = kPrioGui;
	subPriority = 1;
}

// FUN_0040bca0: the window rectangle.
Common::Point Ui::GuiWindow::getSize() const {
	return Common::Point(rect.width(), rect.height());
}

// FUN_0042a8d0 (window), FUN_0040abc0 (message label), FUN_0042aab0 + FUN_0040a930 (buttons).
void Ui::GuiWindow::draw(Graphics::Surface &dst) {
	const Common::Rect clip = getSpriteClip(*this, dst);
	if (clip.isEmpty())
		return;

	// Textured background, black outer edge, white/gray bevel
	tileFrame(dst, clip, rect, kTexWindow);
	frame3D(dst, clip, rect, kColBlack, kColBlack, 0);
	frame3D(dst, clip, rect, kColWhite, kColGray, 1);

	// Title bar of the active window: texture 0x19, white centred caption, engraved line below
	const Common::Point ts = measureLine(caption.c_str(), caption.size(), kFontBold, 0);
	const Common::Rect bar(rect.left + 2, rect.top + 2, rect.right - 2, rect.top + 6 + ts.y);
	tileFrame(dst, clip, bar, kTexCaption);
	drawTextLine(dst, clip, (bar.width() - ts.x) / 2 + bar.left, rect.top + 4, caption, kFontBold, kColWhite, kColBlack, 0);
	hLine(dst, clip, bar.left, bar.bottom, bar.width(), kColGray);
	hLine(dst, clip, bar.left, bar.bottom + 1, bar.width(), kColWhite);

	if (!message.empty())
		drawTextBlock(dst, clip, messageRect.left, messageRect.top, messageRect.width(), message, kFontBold, kColBlack, kColWhite, 0);

	for (uint i = 0; i < buttons.size(); i++) {
		const GuiButton &b = buttons[i];
		const int tex = (b.id == kButtonOk) ? kTexButtonOk : (b.id == kButtonCancel ? kTexButtonCancel : kTexButton);
		// A pressed button loses its bevel and its text moves by one pixel
		const uint32 light = b.pressed ? kColBlack : kColWhite;
		const uint32 dark = b.pressed ? kColBlack : kColGray;
		const int shift = b.pressed ? 1 : 0;
		tileFrame(dst, clip, b.rect, tex);
		frame3D(dst, clip, b.rect, kColBlack, kColBlack, 0);
		frame3D(dst, clip, b.rect, light, dark, 1);
		const Common::Point bs = measureLine(b.text.c_str(), b.text.size(), kFontBold, 0);
		drawTextLine(dst, clip, (b.rect.width() - bs.x) / 2 + b.rect.left + shift,
			(b.rect.height() - bs.y) / 2 + b.rect.top + shift, b.text, kFontBold, kColBlack, kColGray, 0);
	}
}

// ---------------------------------------------------------------------------
// Construction and reset
// ---------------------------------------------------------------------------

// Initial values of the globals the UI replaces (DAT_004419f8 starts at 100).
Ui::Ui() : _menuOpen(false), _leftHeld(false), _rightHeld(false), _leftPress(false), _leftRelease(false),
		_rightPress(false), _rightRelease(false), _waitRelease(false), _clickTimer(100), _holding(false),
		_guiLatch(false), _scrollSpeed(8), _guiPrevLeft(false), _pending(kPendingNone), _startupChecked(false),
		_forceNewGame(false), _offerSlot(-1), _gameStarted(false), _creditsPos(-1), _creditsScroll(0) {
	_cursor.priority = kPrioCursor;
	_cursor.frame = kCursorDefault;
	_heldSprite.priority = kPrioHeldItem;
}

Ui::~Ui() {
}

// UI part of FUN_0041de10: nothing held, no window, cursor sheet 402 shown.
void Ui::reset() {
	closeWindow();
	_pending = kPendingNone;
	_holding = false;
	_heldSprite.hide();
	_hoverText.hide();
	_hoverFrames[0].hide();
	_hoverFrames[1].hide();
	for (int i = 0; i < kCreditLines; i++) {
		_creditText[i].hide();
		_creditPicture[i].hide();
	}
	g_engine->_game->_inventory.heldSlot = -1;
	g_engine->_game->_skip = false;
	_cursor.sheet = g_engine->getImage(kCursorSheet);
	_cursor.priority = kPrioCursor;
	_cursor.subPriority = 0;
	_cursor.show();
	_guiLatch = false;
	_scrollSpeed = 8;
}

// ---------------------------------------------------------------------------
// Game::tick
// ---------------------------------------------------------------------------

// A room may end the game in the middle of a tick (event 0x17 of room 1200
// starts the credits, the game-over blinds return to the title). The
// original then simply stopped rendering the game scene; here whatever the
// rest of the tick showed is hidden again.
void Ui::tick(const InputState &input) {
	tickPlaying(input);
	if (g_engine->_state != kStatePlaying)
		clearScene();
}

// FUN_0041d280
void Ui::tickPlaying(const InputState &input) {
	Game *game = g_engine->_game;
	Inventory &inv = game->_inventory;

	// Port: menu actions wait one tick so that saves do not show the menu in the thumbnail
	if (_pending != kPendingNone) {
		runPendingAction();
		if (g_engine->_state != kStatePlaying || g_engine->shouldQuit())
			return;
	}
	if (!_cursor.sheet)
		_cursor.sheet = g_engine->getImage(kCursorSheet);
	if (!_heldSprite.sheet)
		_heldSprite.sheet = g_engine->getImage(kIconSheet);

	const int mx = input.mouseX;
	const int my = input.mouseY;

	// Esc requests the fast-forward (FUN_0042c050); an open window takes it as Cancel instead
	bool guiEsc = false;
	if (input.escPressed) {
		if (_menuOpen)
			guiEsc = true;
		else
			game->_skip = true;
	}
	if (input.menuPressed && !_menuOpen)
		openMenu();

	// After the title or a dialog the buttons count as up until released (no stray edges)
	bool left = input.leftDown;
	bool right = input.rightDown;
	if (_waitRelease) {
		if (left || right)
			left = right = false;
		else
			_waitRelease = false;
		_leftHeld = _rightHeld = false;
	}

	// Button edges and double click (2 releases within 14 ticks and 3 px)
	const bool prevLeft = _leftHeld;
	const bool prevRight = _rightHeld;
	_leftHeld = left;
	_rightHeld = right;
	_leftPress = left && !prevLeft;
	_leftRelease = !left && prevLeft;
	_rightPress = right && !prevRight;
	_rightRelease = !right && prevRight;
	bool doubleClick = false;
	if (prevLeft && !left && _clickTimer <= 13)
		doubleClick = MAX(ABS(_lastClickPos.y - my), ABS(_lastClickPos.x - mx)) <= 3;

	_hoverText.hide();
	_hoverFrames[0].hide();
	_hoverFrames[1].hide();

	// Screen edges of the bar scroll the inventory; the menu button is at (620..638, 403..422)
	bool atLeft = false, atRight = false, onMenuButton = false;
	if (my > 399) {
		atLeft = (mx == 0);
		atRight = (mx == kScreenWidth - 1);
		onMenuButton = my >= 403 && my <= 422 && mx >= 620 && mx <= 638;
	}

	// Deviation: the cursor also stays visible for a window opened during a sequence
	if (!game->sequenceBusy() || _menuOpen) {
		_cursor.show();
		setCursor(kCursorDefault, mx, my);
	} else {
		_cursor.hide();
	}

	if (_menuOpen)
		_guiLatch = _leftHeld;
	guiTick(mx, my, left, guiEsc);
	inv.sparkle.step();
	inv.setButtons(atLeft && left, atRight && left, onMenuButton && left);

	if (atLeft || atRight) {
		// Edge scrolling accelerates from 4 to 16 px per tick; the game keeps running
		setCursor(atLeft ? kCursorScrollLeft : kCursorScrollRight, mx, my);
		_scrollSpeed = MIN(_scrollSpeed + 1, 0x20);
		inv.scrollBy(((int)atRight - (int)atLeft) * (_scrollSpeed >> 1));
		runSequencer();
		return;
	}
	_scrollSpeed = 8;

	// Right click in the bar pages the ring by 8 slots
	if (_rightPress && my > 399)
		inv.scrollBy(mx < 320 ? -400 : 400);

	if (_leftRelease) {
		_lastClickPos = Common::Point(mx, my);
		_clickTimer = 0;
		if (onMenuButton) {
			if (!_menuOpen)
				openMenu();
			else if (_window.id == kMenuWindow)
				closeWindow();
		}
	}
	_clickTimer++;

	// A modal window pauses the game; a click that closed it is ignored until released
	if (_menuOpen)
		return;
	if (_guiLatch && _leftHeld)
		return;
	_guiLatch = false;

	bool hover = !_holding;
	if (_holding) {
		if (!left) {
			// Released: combine, swap slots or look at the item (double click on its own slot)
			const int action = heldItemAction(mx, my);
			if (action == 0) {
				const int slot = (my > 399) ? inv.slotAt(mx, my) : -1;
				if (slot != -1 && inv.heldSlot != -1) {
					if (slot == inv.heldSlot) {
						if (doubleClick)
							game->raiseEvent(kEvLookItem, inv.heldItem);
					} else {
						inv.setSlot(inv.heldSlot, inv.slots[slot]);
						inv.setSlot(slot, inv.heldItem);
						inv.heldSlot = -1;
					}
				}
			} else {
				game->raiseEvent(kEvAction, action);
			}
		}
		if (!_holding || !left)
			hover = true;
		else
			updateHeldItem(mx, my);
	}

	if (hover) {
		_heldSprite.hide();
		_holding = false;
		if (!game->_hero.isTalking()) {
			int type, id;
			if (my < kPlayAreaHeight || onMenuButton) {
				const uint32 value = hotspotAt(mx, my);
				type = value >> 16;
				id = value & 0xFFFF;
			} else {
				const int slot = inv.slotAt(mx, my);
				type = 8;
				id = (slot < 0) ? 0 : inv.slots[slot];
			}

			Common::String name = getString(id);
			if (name.empty())
				name = getString(game->raiseEvent(kEvHoverName, id));
			if (!name.empty() && !game->sequenceBusy())
				showHoverText(name, type, mx, my);

			if (type >= 1 && type <= 5) {
				// Exit arrows move back and forth in their direction
				const int phase = (g_engine->_tickCounter >> 1) & 7;
				setCursor(type + kCursorArrows, mx + kArrowDirs[type - 1][0] * phase, my + kArrowDirs[type - 1][1] * phase);
			} else if (type == 9) {
				setCursor(kCursorSpin + ((g_engine->_tickCounter / 3) & 3), mx, my);
			}
		}
	}

	// Esc: run the sequence until it ends, with the sound effects silenced
	if (game->_skip && game->sequenceBusy()) {
		SoundManager *snd = g_engine->_soundMan;
		snd->stopAll();
		int busy = runSequencer();
		snd->stopAll();
		for (int steps = 0; busy && game->_skip && g_engine->_state == kStatePlaying; steps++) {
			if (steps >= kMaxSkipSteps) {
				warning("Gooka: fast-forward did not end, giving up");
				break;
			}
			g_engine->_tickCounter++;
			busy = runSequencer();
			snd->stopAll();
		}
	}
	game->_skip = false;

	if (runSequencer() != 0 || g_engine->_state != kStatePlaying)
		return;

	// Raw mouse events for the room first, then the default handling
	const int mouseParam = (my << 16) | (mx & 0xFFFF);
	if (_leftRelease && game->raiseEvent(kEvLeftUp, mouseParam))
		return;
	if (_leftPress && game->raiseEvent(kEvLeftDown, mouseParam))
		return;
	if (_rightRelease && game->raiseEvent(kEvRightUp, mouseParam))
		return;
	if (_rightPress && game->raiseEvent(kEvRightDown, mouseParam))
		return;
	if (game->raiseEvent(kEvClick, mouseParam))
		return;

	if (!_holding)
		game->dropHeldItem();
	if (!left || prevLeft)
		return;
	// (FUN_0040f6e0, the dialogue choice list, is never filled by any room)

	if (my > kPlayAreaHeight && !onMenuButton) {
		// Pick an item up; its slot shows -1 while it is on the cursor
		int offsetX = 0;
		const int slot = inv.slotAt(mx, my, &offsetX);
		const int item = (slot < 0) ? 0 : inv.slots[slot];
		if (item < 1)
			return;
		_grab = Common::Point(offsetX, my - kPlayAreaHeight);
		_holding = true;
		inv.heldItem = item;
		inv.heldSlot = slot;
		inv.setSlot(slot, -1);
		_heldSprite.setFrame(item + 10);
		updateHeldItem(mx, my);
		return;
	}
	if (my >= kPlayAreaHeight)
		return;

	const int gx = mx >> 2;
	const int gy = my >> 2;
	if (game->raiseEvent(kEvFloorClick, gx | (gy << 16)))
		return;

	// A hotspot under the cursor or at the walk target is used after walking there
	int id = hotspotAt(mx, my) & 0xFFFF;
	Common::Point cell;
	const bool found = game->_walk.findWalkTarget(gx, gy, cell);
	if (found && id == 0)
		id = hotspotAt(cell.x * 4 + 2, cell.y * 4 + 2) & 0xFFFF;
	if (id != 0) {
		game->actOnHotspot(id, 1);
		return;
	}
	// Deviation: without a walk target the original read stale locals (the cursor hot spot)
	if (!found)
		return;
	if (game->_walk.zoneType(cell.x, cell.y) != -1 && game->_hero.isVisible()) {
		// FUN_0040f360: cancel a non-blocking sequence, then walk
		game->clearWalkTarget();
		game->mainSequencer().stop();
		game->_hero.stopWalking();
		game->setWalkTarget(cell.x, cell.y, -1000);
	}
}

// FUN_0040f8c0 returns the busy state of the sequence it ran, else whether the hero talks.
int Ui::runSequencer() {
	Game *game = g_engine->_game;
	const bool running = game->mainSequencer().isRunning();
	game->sequencerTick();
	return running ? (game->sequenceBusy() ? 1 : 0) : (game->_hero.isTalking() ? 1 : 0);
}

// FUN_00409530 + FUN_00408430: the frame's prefix is the cursor hot spot.
void Ui::setCursor(int frame, int x, int y) {
	_cursor.setFrame(frame);
	Common::Point hs(0, 0);
	if (_cursor.sheet) {
		hs = _cursor.sheet->getHotspot(frame);
		if (hs.x < 0 || hs.y < 0)
			hs = Common::Point(0, 0);
	}
	_cursor.x = x - hs.x;
	_cursor.y = y - hs.y;
	_cursor.priority = kPrioCursor;
	_cursor.subPriority = 0;
}

// FUN_0040f950: the hotspot map first, then the top-most visible dynamic
// hotspot sprite; event 0x14 may replace the id, a cleared flag hides it.
uint32 Ui::hotspotAt(int px, int py) const {
	if (px < 0 || px >= kScreenWidth || py < 0 || py >= kPlayAreaHeight)
		return 0;

	Game *game = g_engine->_game;
	const RoomDescriptor *desc = game->descriptor();
	int32 value = -1;

	const int cell = game->_walk.hotspotCell(px >> 2, py >> 2);
	if (cell != 0 && desc && desc->hotspots) {
		if (desc->numHotspots > 0) {
			// The original reads hotspots[cell - 1] unchecked; the tables cover every slot the map uses
			if (cell <= desc->numHotspots)
				value = desc->hotspots[cell - 1];
		} else {
			// Length unknown (descriptors built by room code): stop at the first -1
			int i = 0;
			while (i < cell - 1 && desc->hotspots[i] != -1)
				i++;
			if (i == cell - 1)
				value = desc->hotspots[i];
		}
	}

	if (value == -1) {
		const Common::Array<DynamicHotspot> &list = game->getDynamicHotspots();
		int best = -1, bestPriority = -1, bestSub = -1;
		for (uint i = 0; i < list.size(); i++) {
			const Sprite *s = list[i].sprite;
			const Common::Point size = s->getSize();
			if (s->x <= px && s->y <= py && px < s->x + size.x && py < s->y + size.y &&
					(bestPriority < s->priority || (s->priority == bestPriority && bestSub < s->subPriority)) &&
					s->isVisible()) {
				bestSub = s->subPriority;
				bestPriority = s->priority;
				best = i;
			}
		}
		if (best != -1)
			value = list[best].id;
	}

	if (value != -1) {
		int result = game->raiseEvent(kEvHotspotOverride, value & 0xFFFF);
		if (result == 0 && !game->flagTest(value & 0xFFFF))
			result = -1;
		if (result != -1)
			return (uint32)(result != 0 ? result : value);
	}
	return 0;
}

// FUN_0041be90: black text on a box in the cursor type's colour below the
// mouse; type 7 (an action of the held item) adds two pulsing frames.
void Ui::showHoverText(const Common::String &text, int type, int mx, int my) {
	uint32 color;
	if (type == 6)
		color = 0x55da91;
	else if (type == 8)
		color = 0xaaffff;
	else if (type == 7)
		color = 0x00ffff;
	else
		color = 0xffff00;

	_hoverText.setText(text, kFontRegular, kColBlack, color, 0);
	const Common::Point size = _hoverText.getSize();
	_hoverText.x = mx;
	_hoverText.y = my + 18;
	_hoverText.priority = kPrioHover;
	_hoverText.subPriority = 0;
	// FUN_00408af0: keep it on the screen
	if (_hoverText.x + size.x >= kScreenWidth)
		_hoverText.x = kScreenWidth - size.x;
	if (_hoverText.x < 0)
		_hoverText.x = 0;
	if (_hoverText.y + size.y >= kScreenHeight)
		_hoverText.y = kScreenHeight - size.y;
	if (_hoverText.y < 0)
		_hoverText.y = 0;
	_hoverText.show();

	if (type == 7) {
		for (int i = 0; i < 2; i++) {
			const int grow = ((int)(g_engine->_tickCounter / 3) + i * 5) % 10;
			FrameSprite &f = _hoverFrames[i];
			f.x = _hoverText.x - grow;
			f.y = _hoverText.y - grow;
			f.width = size.x + 2 * grow;
			f.height = size.y + 2 * grow;
			f.priority = kPrioHoverFrames;
			f.subPriority = 0;
			f.show();
		}
	}
}

// FUN_0041d140
int Ui::heldItemAction(int mx, int my) {
	Game *game = g_engine->_game;
	const int held = game->_inventory.heldItem;
	if (my < 0x191) {
		const uint32 value = hotspotAt(mx, my);
		if (value & 0xFFFF)
			return game->raiseEvent(kEvItemOnHotspot, (held << 16) | (value & 0xFFFF));
	} else {
		const int item = game->_inventory.itemAt(mx);
		if (item > 0)
			return game->raiseEvent(kEvItemOnItem, (held << 16) | (item & 0xFFFF));
	}
	return 0;
}

// FUN_0041d1c0
void Ui::updateHeldItem(int mx, int my) {
	_heldSprite.show();
	_heldSprite.x = mx - _grab.x;
	_heldSprite.y = my - _grab.y;
	_heldSprite.priority = kPrioHeldItem;
	_heldSprite.subPriority = 0;
	const int action = heldItemAction(mx, my);
	const Common::String name = getString(action);
	if (!name.empty())
		showHoverText(name, 7, mx, my);
}

// ---------------------------------------------------------------------------
// Menu and message boxes
// ---------------------------------------------------------------------------

// The menu is created when the menu button is released (block in FUN_0041d280).
void Ui::openMenu() {
	if (!_menuOpen)
		buildMenu();
}

// Frame window 140x190 centred on the screen; buttons 70 % wide, the first
// 30 px below the top, then 5 px apart and 10 px before "Zrusit".
void Ui::buildMenu() {
	static const int kButtons[] = { kCmdSave, kCmdLoad, kCmdRestart, kCmdQuit, kCmdOptions, kCmdClose };

	_window.id = kMenuWindow;
	_window.caption = getString(kMenuWindow);
	_window.message.clear();
	_window.rect = Common::Rect(140, 190);
	_window.rect.moveTo((kScreenWidth - 140) / 2, (kScreenHeight - 190) / 2);
	_window.buttons.clear();

	const int w = 0x46 * _window.rect.width() / 100;
	Common::Rect r(w, 20);
	r.moveTo((_window.rect.width() - w) / 2 + _window.rect.left, _window.rect.top + 30);
	for (uint i = 0; i < ARRAYSIZE(kButtons); i++) {
		if (i > 0)
			r.moveTo(r.left, r.bottom + (kButtons[i] == kCmdClose ? 10 : 5));
		GuiButton b;
		b.id = kButtons[i];
		b.text = getString(kButtons[i]);
		b.rect = r;
		b.pressed = false;
		_window.buttons.push_back(b);
	}

	_window.captured = -1;
	_window.x = _window.rect.left;
	_window.y = _window.rect.top;
	_window.show();
	_menuOpen = true;
}

// FUN_0040cf50 with the OK/Cancel style 0x2000: the box is the text size
// (at least 200 wide) plus 20 x 70, Cancel bottom left, OK bottom right,
// the text 10 px above the buttons. Wider labels widen their buttons (port).
void Ui::buildMessageBox(int id, const Common::String &caption, const Common::String &message,
		const Common::String &okText, const Common::String &cancelText) {
	const Common::Point text = measureMessage(message);
	const int textW = MAX<int>(text.x, 200);

	_window.id = id;
	_window.caption = caption;
	_window.message = message;
	_window.rect = Common::Rect(textW + 20, text.y + 70);
	_window.rect.moveTo((kScreenWidth - _window.rect.width()) / 2, (kScreenHeight - _window.rect.height()) / 2);
	_window.buttons.clear();

	GuiButton cancel;
	cancel.id = kButtonCancel;
	cancel.text = cancelText;
	cancel.rect = Common::Rect(MAX<int>(0x32, measureLine(cancelText.c_str(), cancelText.size(), kFontBold).x + 16), 20);
	cancel.rect.moveTo(_window.rect.left + 20, _window.rect.bottom - 8 - 20);
	cancel.pressed = false;
	_window.buttons.push_back(cancel);

	GuiButton ok;
	ok.id = kButtonOk;
	ok.text = okText;
	ok.rect = Common::Rect(MAX<int>(0x32, measureLine(okText.c_str(), okText.size(), kFontBold).x + 16), 20);
	ok.rect.moveTo(_window.rect.right - 20 - ok.rect.width(), _window.rect.bottom - 8 - 20);
	ok.pressed = false;
	_window.buttons.push_back(ok);

	_window.messageRect = Common::Rect(textW, text.y);
	_window.messageRect.moveTo((_window.rect.width() - textW) / 2 + _window.rect.left, ok.rect.top - 10 - text.y);

	_window.captured = -1;
	_window.x = _window.rect.left;
	_window.y = _window.rect.top;
	_window.show();
	_menuOpen = true;
}

// FUN_0040c510
void Ui::closeWindow() {
	_window.hide();
	_window.buttons.clear();
	_window.captured = -1;
	_menuOpen = false;
}

// FUN_00406fd0 as far as buttons go: a press captures the button under the
// mouse, it is shown pressed while the mouse stays inside (FUN_0040ac90) and
// fires on release (FUN_0040ad60); Esc fires Cancel (FUN_0040af70).
void Ui::guiTick(int mx, int my, bool left, bool escPressed) {
	const bool prevLeft = _guiPrevLeft;
	_guiPrevLeft = left;
	if (!_menuOpen)
		return;

	Common::Array<GuiButton> &buttons = _window.buttons;
	if (left && !prevLeft) {
		for (uint i = 0; i < buttons.size(); i++) {
			if (buttons[i].rect.contains(mx, my)) {
				_window.captured = i;
				buttons[i].pressed = true;
				break;
			}
		}
	} else if (_window.captured >= 0 && _window.captured < (int)buttons.size()) {
		GuiButton &b = buttons[_window.captured];
		if (left) {
			b.pressed = b.rect.contains(mx, my);
		} else {
			const bool fire = b.pressed;
			const int buttonId = b.id;
			b.pressed = false;
			_window.captured = -1;
			if (fire) {
				onCommand(_window.id, buttonId);
				return;
			}
		}
	}

	if (escPressed && _menuOpen) {
		// Deviation: the menu has no Cancel button in the original; Esc closes it too
		if (_window.id == kMenuWindow)
			closeWindow();
		else
			onCommand(_window.id, kButtonCancel);
	}
}

// FUN_0041ce80. Deviations: saving and loading use ScummVM's dialogs, the
// options dialog (FUN_0041c4c0) is replaced by the ScummVM main menu, and
// "Nova hra" does not delete the autosave but makes the title skip the
// load offer (the original deleted GAME.SAVE for that).
void Ui::onCommand(int windowId, int buttonId) {
	debugC(1, kDebugScript, "Ui: command %x of window %x", buttonId, windowId);
	switch (windowId) {
	case kMenuWindow:
		closeWindow();
		switch (buttonId) {
		case kCmdOptions:
			_pending = kPendingOptions;
			break;
		case kCmdSave:
			_pending = kPendingSave;
			break;
		case kCmdLoad:
			_pending = kPendingLoad;
			break;
		case kCmdQuit:
			buildMessageBox(kConfirmQuit, getString(kStringGooka), getString(kConfirmQuit),
				getString(kButtonOk), getString(kButtonCancel));
			break;
		case kCmdRestart:
			buildMessageBox(kConfirmRestart, getString(kStringGooka), getString(kConfirmRestart),
				getString(kButtonOk), getString(kButtonCancel));
			break;
		default:
			break;
		}
		break;

	case kConfirmRestart:
		closeWindow();
		if (buttonId == kButtonOk)
			_pending = kPendingRestart;
		break;

	case kConfirmQuit:
		closeWindow();
		if (buttonId == kButtonOk)
			_pending = kPendingQuit;
		break;

	case kOfferWindow:
		closeWindow();
		startGame(buttonId == kButtonOk ? _offerSlot : -1);
		break;

	default:
		closeWindow();
		break;
	}
}

// Actions of the menu: the dialogs run between two ticks.
void Ui::runPendingAction() {
	const PendingAction action = _pending;
	Game *game = g_engine->_game;
	_pending = kPendingNone;
	switch (action) {
	case kPendingSave:
		g_engine->saveGameDialog();
		_waitRelease = true;
		break;
	case kPendingLoad:
		g_engine->loadGameDialog();
		_waitRelease = true;
		break;
	case kPendingOptions:
		g_engine->openMainMenuDialog();
		game->_subtitles = ConfMan.getBool("subtitles");
		_waitRelease = true;
		break;
	case kPendingQuit:
		// FUN_0042c000: autosave, then end the program
		g_engine->saveAutosaveIfEnabled();
		g_engine->quitGame();
		break;
	case kPendingRestart:
		// FUN_0041ca30: back to the title screen, which then starts a new game
		g_engine->_soundMan->stopEverything();
		_forceNewGame = true;
		g_engine->setState(kStateTitle);
		break;
	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// Title screen
// ---------------------------------------------------------------------------

// The engine renders the scene in every state, so the title and the credits
// hide all sprites (FUN_0041df70 hid the game's own) and use the background.
void Ui::clearScene() {
	Scene *scene = g_engine->_scene;
	while (!scene->getSprites().empty())
		scene->getSprites().back()->hide();
	_menuOpen = false;
	_window.buttons.clear();
	_window.captured = -1;
}

// FUN_0042b410: full volume, title music 0x1b3, picture 464. The sounds
// stop as in FUN_0041df70, which ran before it when a game ended.
void Ui::showTitle() {
	g_engine->_soundMan->stopEverything();
	clearScene();
	_holding = false;
	_pending = kPendingNone;
	_offerSlot = -1;
	g_engine->_scene->setBackground(g_engine->getImage(kTitleImage));
	g_engine->_game->setMusicVolume(0x100);
	g_engine->_game->setRoomMusic(kTitleMusic);
}

// FUN_0042c050 / FUN_0042bf30 in state 1: a key or a left click continues
// (FUN_0042b520). Port: instead of loading GAME.SAVE silently, the newest
// ScummVM save is offered in a message box; a save chosen in the launcher
// is loaded directly.
bool Ui::titleTick(const InputState &input) {
	_gameStarted = false;

	if (!_startupChecked) {
		_startupChecked = true;
		if (ConfMan.hasKey("save_slot")) {
			const int slot = ConfMan.getInt("save_slot");
			if (slot >= 0) {
				startGame(slot);
				return true;
			}
		}
	}

	const bool prevLeft = _leftHeld;
	_leftHeld = input.leftDown;
	_rightHeld = input.rightDown;

	if (_menuOpen) {
		// The load offer is open
		setCursor(kCursorDefault, input.mouseX, input.mouseY);
		guiTick(input.mouseX, input.mouseY, input.leftDown, input.escPressed);
		return _gameStarted;
	}

	if (!(input.leftDown && !prevLeft) && !input.escPressed && !input.menuPressed)
		return false;

	Common::String description;
	const int slot = _forceNewGame ? -1 : findContinueSlot(description);
	if (slot < 0) {
		startGame(-1);
		return true;
	}

	// Message box in the game's style: "Nova hra" (Cancel) or "Nacist hru" (OK)
	_offerSlot = slot;
	const Common::String text = Common::U32String(description, Common::kUtf8).encode(Common::kWindows1250) + '\v';
	buildMessageBox(kOfferWindow, getString(kStringGooka), text, getString(kCmdLoad), getString(kCmdRestart));
	_guiPrevLeft = input.leftDown;
	_cursor.sheet = g_engine->getImage(kCursorSheet);
	_cursor.show();
	setCursor(kCursorDefault, input.mouseX, input.mouseY);
	return false;
}

// FUN_0042b520 (load GAME.SAVE, else FUN_0041df40 = new game). The click
// that got here is ignored until the button is released.
void Ui::startGame(int slot) {
	_forceNewGame = false;
	_offerSlot = -1;
	closeWindow();

	bool loaded = false;
	if (slot >= 0) {
		const Common::Error err = g_engine->loadGameState(slot);
		loaded = (err.getCode() == Common::kNoError);
		if (!loaded)
			warning("Gooka: cannot load save slot %d: %s", slot, err.getDesc().c_str());
	}
	if (!loaded) {
		reset();
		g_engine->_game->newGame();
	}

	_guiLatch = false;
	_waitRelease = true;
	_gameStarted = true;
}

// The save with the latest date and time in its ScummVM header.
int Ui::findContinueSlot(Common::String &description) const {
	Common::SaveFileManager *saveMan = g_system->getSavefileManager();
	if (!saveMan || !g_engine->getMetaEngine())
		return -1;
	const Common::String target = ConfMan.getActiveDomainName();
	const Common::StringArray files = saveMan->listSavefiles(g_engine->getMetaEngine()->getSavegameFilePattern(target.c_str()));

	int bestSlot = -1;
	uint64 bestKey = 0;
	for (uint i = 0; i < files.size(); i++) {
		const Common::String &name = files[i];
		if (name.size() < 3)
			continue;
		const int slot = atoi(name.c_str() + name.size() - 3);
		Common::InSaveFile *in = saveMan->openForLoading(name);
		if (!in)
			continue;
		ExtendedSavegameHeader header;
		if (MetaEngine::readSavegameHeader(in, &header, true)) {
			// date = day << 24 | month << 16 | year, time = hour << 8 | minute
			const uint64 key = ((uint64)(header.date & 0xFFFF) << 32) | (((header.date >> 16) & 0xFF) << 24) |
				(((header.date >> 24) & 0xFF) << 16) | header.time;
			if (bestSlot == -1 || key >= bestKey) {
				bestSlot = slot;
				bestKey = key;
				description = header.description;
			}
		}
		delete in;
	}
	return bestSlot;
}

// ---------------------------------------------------------------------------
// Credits
// ---------------------------------------------------------------------------

// FUN_0042b500 / FUN_00410b00: background 4000, text 4000, music 0x1b2 at full volume.
void Ui::startCredits() {
	clearScene();
	_holding = false;
	_pending = kPendingNone;
	g_engine->_scene->setBackground(g_engine->getImage(kCreditsImage));
	_creditsText = g_engine->_res->getText(kCreditsImage);
	_creditsPos = _creditsText.empty() ? -1 : 0;
	_creditsScroll = 0;
	for (int i = 0; i < kCreditLines; i++) {
		_creditText[i].hide();
		_creditPicture[i].hide();
	}
	g_engine->_game->setRoomMusic(kCreditsMusic);
	g_engine->_game->setMusicVolume(0x100);
}

// FUN_00410b90: every other tick all rows move up by one pixel; a row that
// reaches the bottom (y = 479) gets the next line, "@n" = frame n of image
// 4001. After the last line the music fades out, then the credits end
// (the original quit the program there, FUN_0042c000, as Esc did).
bool Ui::creditsTick(const InputState &input) {
	if (input.escPressed)
		return true;

	if ((g_engine->_tickCounter & 1) == 0) {
		for (int i = 0; i < kCreditLines; i++) {
			const int y = 0x1df - (i * kCreditPitch + _creditsScroll) % kCreditCycle;
			TextSprite &text = _creditText[i];
			Sprite &picture = _creditPicture[i];

			if (y == 0x1df) {
				Common::String line;
				if (_creditsPos >= 0) {
					bool newline = false;
					while (_creditsPos < (int)_creditsText.size()) {
						const char c = _creditsText[_creditsPos++];
						if (c == '\n') {
							newline = true;
							break;
						}
						line += c;
					}
					if (!newline)
						_creditsPos = -1;
				}

				text.hide();
				picture.hide();
				if (line.hasPrefix("@")) {
					picture.sheet = g_engine->getImage(kCreditsPictures);
					picture.setFrame(atoi(line.c_str() + 1));
					picture.x = kCreditPictureX;
					picture.y = 0;
					picture.priority = 1000;
					picture.subPriority = 0;
					picture.show();
				} else {
					text.setText(line, kFontSerif, kColWhite, kColBlack, 0);
					text.show();
				}
			}

			text.x = kCreditTextX;
			text.y = y;
			text.priority = 1000;
			text.subPriority = 0;
			picture.y = y;
			picture.priority = 1000;
			picture.subPriority = 0;
		}

		if (_creditsPos < 0) {
			// Fade out by 3 steps of every 4
			const int volume = g_engine->_game->getMusicVolume();
			if (volume < 1)
				return true;
			g_engine->_game->setMusicVolume(volume - ((_creditsScroll & 3) != 0 ? 1 : 0));
		}
		_creditsScroll++;
	}
	return false;
}

} // End of namespace Gooka
