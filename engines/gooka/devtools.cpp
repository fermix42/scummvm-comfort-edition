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
 * Development tools: a tiny command language for automated, headless tests
 * (environment variable GOOKA_TEST) and PNG screenshots.
 */

#include "common/debug.h"
#include "common/file.h"
#include "common/str-array.h"
#include "common/textconsole.h"

#include "image/png.h"

#include "gooka/game.h"
#include "gooka/gooka.h"
#include "gooka/music.h"
#include "gooka/resource.h"
#include "gooka/scene.h"
#include "gooka/sound.h"

namespace Gooka {

/** Splits a string at every separator; empty parts are kept. */
static Common::StringArray splitString(const Common::String &str, char sep) {
	Common::StringArray parts;
	Common::String cur;
	for (uint i = 0; i < str.size(); i++) {
		if (str[i] == sep) {
			parts.push_back(cur);
			cur.clear();
		} else {
			cur += str[i];
		}
	}
	parts.push_back(cur);
	return parts;
}

// Commands (separated by ';', arguments by ':'). Resource viewer (GOOKA_TEST="raw:..."):
//   clear | img:id:frame:x:y | bg:id[:frame] | text:id[:font[:y]] | music:id | speech:id | video:id
// Game (GOOKA_TEST="..."): newgame | room:id | hero:gx:gy:frame | click:x:y | rclick:x:y |
//   move:x:y | ticks:n | esc | save:slot | load:slot
// Both: wait:ms | shot:<path> | quit
void GookaEngine::runTestScript(const Common::String &script) {
	for (const auto &cmd : splitString(script, ';')) {
		const Common::StringArray args = splitString(cmd, ':');
		const Common::String &op = args[0];
		const int a = args.size() > 1 ? atoi(args[1].c_str()) : 0;
		const int b = args.size() > 2 ? atoi(args[2].c_str()) : 0;
		const int c = args.size() > 3 ? atoi(args[3].c_str()) : 0;
		const int d = args.size() > 4 ? atoi(args[4].c_str()) : 0;

		if (op == "clear") {
			_screen->clear();
		} else if (op == "img") {
			drawImage(a, b, c, d);
		} else if (op == "bg") {
			drawImage(a, b, 0, 0);
			drawImage(409, 0, 0, kPlayAreaHeight);
		} else if (op == "text") {
			drawText(_res->getText(a), b ? b : 404, 8, c ? c : 410, _lut[0x7FFF]);
		} else if (op == "music") {
			_music->play(a);
		} else if (op == "video" && (a == 12 || a == 13)) {
			playVideo(Common::Path(Common::String::format("%d.AVI", a)));
		} else if (op == "speech") {
			int handle = kNoSound;
			_soundMan->play(&handle, a, 0, false, 0, 0, Audio::Mixer::kSpeechSoundType);
		} else if (op == "newgame" && _game) {
			_ui->reset();
			_game->newGame();
			_nextState = (EngineState)0;
			_state = kStatePlaying;
		} else if (op == "room" && _game) {
			_game->changeRoom(a);
			_nextState = (EngineState)0;
			_state = kStatePlaying;
		} else if (op == "hero" && _game) {
			_game->_hero.place(a, b, c);
		} else if (op == "move") {
			_input.mouseX = a;
			_input.mouseY = b;
		} else if (op == "click" || op == "rclick") {
			// Press for one tick, release on the next, like a real click
			_input.mouseX = a;
			_input.mouseY = b;
			bool &button = (op == "click") ? _input.leftDown : _input.rightDown;
			button = true;
			if (_scene)
				tick();
			button = false;
			if (_scene)
				tick();
		} else if (op == "save") {
			const Common::Error err = saveGameState(a, "test", false);
			debug("Gooka: save slot %d: %s", a, err.getDesc().c_str());
		} else if (op == "load") {
			const Common::Error err = loadGameState(a);
			debug("Gooka: load slot %d: %s", a, err.getDesc().c_str());
		} else if (op == "esc") {
			_input.escPressed = true;
		} else if (op == "ticks") {
			for (int i = 0; i < a && !shouldQuit(); i++)
				tick();
		} else if (op == "wait") {
			waitMillis(a);
		} else if (op == "shot") {
			// Windows paths contain ':', so the path is the rest of the command
			if (_scene)
				_scene->render(*_screen->surfacePtr());
			_screen->update();
			saveScreenshot(cmd.substr(5));
		} else if (op == "quit") {
			quitGame();
		} else if (!op.empty()) {
			warning("Gooka: unknown test command '%s'", op.c_str());
		}
	}
}

// Encodes the screen buffer (16-bit) as PNG; missing directories are created.
void GookaEngine::saveScreenshot(const Common::String &path) {
	Common::DumpFile out;
	if (!out.open(Common::Path(path, '/'), true)) {
		warning("Gooka: cannot write %s", path.c_str());
		return;
	}
	::Image::writePNG(out, *_screen->surfacePtr());
	debug("Gooka: screenshot saved to %s", path.c_str());
}

} // End of namespace Gooka
