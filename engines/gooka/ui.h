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
 * User interface: per-tick input handling of the playing state
 * (Game::tick FUN_0041d280), cursor, hover names, the in-game menu,
 * title screen and credits.
 */

#ifndef GOOKA_UI_H
#define GOOKA_UI_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

#include "gooka/effects.h"
#include "gooka/sprite.h"
#include "gooka/text.h"

namespace Gooka {

/** Mouse and keyboard state collected by the engine's event loop. */
struct InputState {
	int mouseX, mouseY;
	bool leftDown;      ///< button currently held
	bool rightDown;
	bool escPressed;    ///< Esc since the last tick (skip)
	bool menuPressed;   ///< key that opens the in-game menu since the last tick
};

/**
 * Everything the player interacts with while playing: cursor shapes
 * (sheet 402), hover name text, clicks on the floor, hotspots and
 * inventory, holding and combining items, the menu button.
 */
class Ui {
public:
	Ui();
	~Ui();

	/** Resets the UI for a new game (part of FUN_0041de10). */
	void reset();

	/**
	 * One tick of the playing state (FUN_0041d280): computes button edges,
	 * updates hover and cursor, dispatches clicks, runs the sequencer and
	 * the hero via Game::sequencerTick().
	 */
	void tick(const InputState &input);

	/** Shows the title screen (state 1, FUN_0042b410). */
	void showTitle();
	/** One tick of the title screen; returns true when the player starts the game. */
	bool titleTick(const InputState &input);

	/** Starts the credits scroller (state 5, FUN_0042b500 / FUN_00410b90). */
	void startCredits();
	/** One tick of the credits; returns true when they are over. */
	bool creditsTick(const InputState &input);

	/** Opens the in-game menu (FUN_0041ce80). */
	void openMenu();
	/** True while the menu is open (the game is paused). */
	bool isMenuOpen() const { return _menuOpen; }

	/**
	 * The next title screen starts a new game without offering a save
	 * (what deleting GAME.SAVE achieved in the original, e.g. after the ending).
	 */
	void forceNewGameAtTitle() { _forceNewGame = true; }

private:
	/** Ids of the menu strings, windows and buttons (FUN_0041ce80). */
	enum {
		kMenuWindow = 0x1c2,     ///< window id and caption "Volby"
		kCmdOptions = 0x1c2,     ///< "Volby"
		kCmdSave = 0x1c3,        ///< "Ulozit hru"
		kCmdLoad = 0x1c4,        ///< "Nacist hru"
		kCmdQuit = 0x1c5,        ///< "Ukoncit hru"
		kCmdRestart = 0x1c6,     ///< "Nova hra"
		kCmdClose = 0x1c7,       ///< "Zrusit"
		kConfirmRestart = 0x1c8, ///< message box "Zacit znovu ?"
		kConfirmQuit = 0x1c9,    ///< message box "Ukoncit program ?"
		kOfferWindow = 0x1000,   ///< port only: "load the last save?" on the title screen
		kButtonOk = 0x7ff6,      ///< string "OK"
		kButtonCancel = 0x7ff7,  ///< string "Zrusit"
		kStringGooka = 0x19a     ///< message box caption "Gooka"
	};

	/** Push button of the original GUI toolkit (vtable 0x437500). */
	struct GuiButton {
		int id;              ///< +0x78 command id passed to the callback
		Common::String text; ///< +0x7c label
		Common::Rect rect;   ///< +0x54 screen rectangle
		bool pressed;        ///< bit 8 of +0x74: held with the mouse inside, drawn sunken
	};

	/**
	 * Modal frame window of the original GUI (menu vtable 0x437a18, message
	 * box vtable 0x437460) with its buttons, drawn as one sprite at the GUI
	 * root's priority 0x3ec.
	 */
	class GuiWindow : public Sprite {
	public:
		GuiWindow();

		int id;                  ///< +0x78 window id passed to the callback
		Common::String caption;  ///< +0x7c title bar text
		Common::String message;  ///< message box text (label at +0x9c)
		Common::Rect rect;       ///< +0x54 screen rectangle
		Common::Rect messageRect;
		Common::Array<GuiButton> buttons;
		int captured;            ///< button with the mouse capture, -1 = none

		/** Size of the window rectangle. */
		Common::Point getSize() const override;
		/** Frame window (FUN_0042a8d0), label (FUN_0040abc0) and buttons (FUN_0042aab0). */
		void draw(Graphics::Surface &dst) override;
	};

	/** Actions run at the start of the next tick, once the closed window is off the screen. */
	enum PendingAction {
		kPendingNone,
		kPendingSave,
		kPendingLoad,
		kPendingOptions,
		kPendingQuit,
		kPendingRestart
	};

	/** Credits layout (FUN_00410b90). */
	enum {
		kCreditLines = 48,    ///< text/picture sprite pairs, one per 21-px row
		kCreditPitch = 21,
		kCreditCycle = 0x3f0, ///< kCreditLines * kCreditPitch
		kCreditTextX = 0x80,
		kCreditPictureX = 0xa0
	};

	/** The body of tick(). */
	void tickPlaying(const InputState &input);
	/** Runs FUN_0040f8c0; returns its result: sequence busy, else hero talking. */
	int runSequencer();
	/** Sets the cursor frame and puts its hot spot on (x, y) (FUN_00408430). */
	void setCursor(int frame, int x, int y);
	/** Hotspot under a scene pixel: cursorType << 16 | id, 0 = none (FUN_0040f950). */
	uint32 hotspotAt(int px, int py) const;
	/** Hover label coloured by cursor type; type 7 adds the pulsing frames (FUN_0041be90). */
	void showHoverText(const Common::String &text, int type, int mx, int my);
	/** Action id of the held item over a hotspot (event 0x10) or item (event 0x11) (FUN_0041d140). */
	int heldItemAction(int mx, int my);
	/** Moves the held item with the mouse and shows the action name (FUN_0041d1c0). */
	void updateHeldItem(int mx, int my);

	/** Mouse handling of the open window (FUN_00406fd0 for the parts the game uses). */
	void guiTick(int mx, int my, bool left, bool escPressed);
	/** Builds the menu window (the block of FUN_0041d280 that creates DAT_004778a0). */
	void buildMenu();
	/** Builds a message box with OK/Cancel (FUN_0040d1c0 + layout FUN_0040cf50). */
	void buildMessageBox(int id, const Common::String &caption, const Common::String &message,
		const Common::String &okText, const Common::String &cancelText);
	/** Closes the window (FUN_0040c510). */
	void closeWindow();
	/** Button callback of all windows (FUN_0041ce80). */
	void onCommand(int windowId, int buttonId);
	/** Runs the action chosen in a window during the previous tick. */
	void runPendingAction();

	/** Hides all scene sprites: the title and credits own the screen (the original did not render the scene). */
	void clearScene();
	/** Leaves the title: loads a save or starts a new game (FUN_0042b520). */
	void startGame(int slot);
	/** Most recent save slot (for the title's load offer), -1 if none. */
	int findContinueSlot(Common::String &description) const;

	bool _menuOpen;              ///< a modal window is open (DAT_004482e8 top window has flag 0x40)
	Sprite _cursor;              ///< DAT_004787d0: sheet 402, priority 10000
	LabelSprite _hoverText;      ///< DAT_00478838: name under the cursor, priority 0x3eb
	FrameSprite _hoverFrames[2]; ///< DAT_00478608: pulsing frames for cursor type 7
	Sprite _heldSprite;          ///< DAT_00478688: item on the cursor, sheet 407, priority 0x3e9
	GuiWindow _window;           ///< the one window the game ever shows at a time

	bool _leftHeld, _rightHeld;  ///< DAT_004786d4 / DAT_004786d8: buttons in the previous tick
	bool _leftPress, _leftRelease, _rightPress, _rightRelease; ///< edges of this tick
	bool _waitRelease;           ///< ignore the buttons until released (click that left the title)
	int _clickTimer;             ///< DAT_004419f8 ticks since the last release (double click <= 13)
	Common::Point _lastClickPos; ///< DAT_004781d8 / DAT_004781dc position of the last release
	bool _holding;               ///< DAT_004786d0 an inventory item is on the cursor
	bool _guiLatch;              ///< DAT_00478600 button went down while a window was open
	int _scrollSpeed;            ///< DAT_00478884 edge scrolling speed (8..32)
	Common::Point _grab;         ///< DAT_00478888 / DAT_0047888c grab point inside the icon
	bool _guiPrevLeft;           ///< +0x88 of the GUI root: button state seen by guiTick()
	PendingAction _pending;

	bool _startupChecked;        ///< launcher "save_slot" handled
	bool _forceNewGame;          ///< restart confirmed: the title starts a new game without an offer
	int _offerSlot;              ///< save offered on the title screen, -1 = none
	bool _gameStarted;           ///< the title screen started or loaded a game in this tick

	TextSprite _creditText[kCreditLines]; ///< credits object +0x38, stride 0x4c
	Sprite _creditPicture[kCreditLines];  ///< credits object +0xe78, stride 0x44
	Common::String _creditsText;          ///< text 4000
	int _creditsPos;                      ///< +0x1b38 read position, -1 = end reached
	int _creditsScroll;                   ///< +0x1b3c
};

} // End of namespace Gooka

#endif // GOOKA_UI_H
