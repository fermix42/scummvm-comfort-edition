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
 * The Gooka engine: owns all subsystems and runs the 40 Hz main loop.
 */

#ifndef GOOKA_GOOKA_H
#define GOOKA_GOOKA_H

#include "common/hashmap.h"
#include "common/str.h"
#include "common/system.h"

#include "engines/engine.h"

#include "graphics/screen.h"

#include "gooka/ui.h"

struct ADGameDescription;

namespace Gooka {

class Game;
class GookaFont;
class Image;
class Music;
class ResourceManager;
class Scene;
class SoundManager;
class Console;

/** Screen geometry and timing of the original game. */
enum {
	kScreenWidth = 640,
	kScreenHeight = 480,
	kPlayAreaHeight = 400, ///< the scene; the bottom 80 px hold the inventory bar
	kTickMillis = 25       ///< 40 ticks per second (Scene::render frame limiter)
};

/** Top-level states (original DAT_00481428). */
enum EngineState {
	kStateTitle = 1,   ///< title screen
	kStateIntro = 2,   ///< intro video (skipped: the repack's AVI codec is unsupported)
	kStatePlaying = 3,
	kStateOutro = 4,   ///< outro video (skipped)
	kStateCredits = 5
};

/**
 * Engine for "Gooka" (JRC Interactive, 1997).
 * The subsystems are public so that game code can reach them through g_engine.
 */
class GookaEngine : public Engine {
public:
	GookaEngine(OSystem *syst, const ADGameDescription *gameDesc);
	~GookaEngine() override;

	Common::Error run() override;
	bool hasFeature(EngineFeature f) const override;

	/** @name Subsystems */
	///@{
	ResourceManager *_res;
	SoundManager *_soundMan;
	Music *_music;
	Graphics::Screen *_screen;
	Scene *_scene;
	Game *_game;
	Ui *_ui;
	Console *_console;
	///@}

	/** Converts an RGB555 value from the data files to the screen format. */
	uint16 _lut[32768];
	/** Global tick counter (DAT_00448390); drives animation phases. */
	uint32 _tickCounter;
	/** Current top-level state. */
	EngineState _state;
	/** Input collected since the last tick. */
	InputState _input;

	/** Returns a cached image resource, loading it on first use (nullptr if missing). */
	Image *getImage(uint16 id);
	/** Draws a frame of an image resource onto the screen at (x, y). */
	void drawImage(uint16 id, uint frame, int x, int y, bool mirror = false);

	/** Returns a cached font resource, loading it on first use (nullptr if missing). */
	GookaFont *getFont(uint16 id);
	/** Draws CP1250 text; '\n' starts a new line. */
	void drawText(const Common::String &text, uint16 fontId, int x, int y, uint32 color);
	/** Plays the in-game cinematics (the into and the outro) */
	bool playVideo(const Common::Path&);

	/**
	 * Switches the top-level state (title, playing, credits) at the start of
	 * the next tick, so room code may call it from inside its own events.
	 */
	void setState(EngineState state);

	/** Keeps the screen and events alive for `ms` milliseconds. */
	void waitMillis(uint32 ms);

	/** @name Development tools (devtools.cpp) */
	///@{
	/** Runs a GOOKA_TEST command list, e.g. "bg:800;text:2;shot:C:/x.png;quit". */
	void runTestScript(const Common::String &script);
	/** Writes the current screen to a PNG file. */
	void saveScreenshot(const Common::String &path);
	///@}

	/**
	 * @name Save and load (saveload.cpp)
	 * ScummVM stream version of the original GAME.SAVE (FUN_0041c850).
	 */
	///@{
	/** Writes flags, variables, inventory, room, hero and walk target. */
	Common::Error saveGameStream(Common::WriteStream *stream, bool isAutosave = false) override;
	/** Rebuilds the game from a save: new-game reset, room entered again, hero re-placed (FUN_0041cb00). */
	Common::Error loadGameStream(Common::SeekableReadStream *stream) override;
	/** While playing, with no menu open and no blocking sequence running. */
	bool canSaveGameStateCurrently(Common::U32String *msg = nullptr) override;
	/** On the title screen and while playing. */
	bool canLoadGameStateCurrently(Common::U32String *msg = nullptr) override;
	///@}

private:
	/** Selects a 16-bit screen mode and builds the RGB555 conversion table. */
	bool initScreen();
	/** Reads pending events into _input. */
	void pollInput();
	/** Runs one tick of the current state and redraws the screen. */
	void tick();
	/** Performs a state change requested with setState() (FUN_0041df70 cleanup for the title). */
	void applyState();

	const ADGameDescription *_gameDescription;
	EngineState _nextState;   ///< requested state, 0 = none
	bool _leftPressedInTick;  ///< the left button went down since the last tick
	bool _leftReleasePending; ///< ...and up again: release after the tick sees the press
	bool _rightPressedInTick;
	bool _rightReleasePending;
	Common::HashMap<uint16, Image *> _images;    ///< image cache by resource ID
	Common::HashMap<uint16, GookaFont *> _fonts; ///< font cache by resource ID
};

/** The running engine instance. */
extern GookaEngine *g_engine;

} // End of namespace Gooka

#endif // GOOKA_GOOKA_H
