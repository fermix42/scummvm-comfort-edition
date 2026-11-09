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
 * Engine setup, resource caches and the 40 Hz main loop.
 */

#define FORBIDDEN_SYMBOL_EXCEPTION_getenv

#include "common/debug.h"
#include "common/config-manager.h"
#include "common/events.h"
#include "common/ptr.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "engines/util.h"
#include "graphics/blit.h"

#include "gooka/detection.h"
#include "gooka/font.h"
#include "gooka/game.h"
#include "gooka/gooka.h"
#include "gooka/image.h"
#include "gooka/music.h"
#include "gooka/resource.h"
#include "gooka/scene.h"
#include "gooka/sound.h"
#include "gooka/ui.h"

#include "video/avi_decoder.h"

namespace Gooka {

GookaEngine *g_engine = nullptr;

GookaEngine::GookaEngine(OSystem *syst, const ADGameDescription *gameDesc) :
		Engine(syst), _gameDescription(gameDesc), _res(nullptr), _soundMan(nullptr),
		_music(nullptr), _screen(nullptr), _scene(nullptr), _game(nullptr), _ui(nullptr),
		_tickCounter(0), _state(kStateTitle), _nextState((EngineState)0), _leftPressedInTick(false),
		_leftReleasePending(false), _rightPressedInTick(false), _rightReleasePending(false) {
	g_engine = this;
	memset(_lut, 0, sizeof(_lut));
	memset(&_input, 0, sizeof(_input));
}

// Game objects own sprites, so they go before the scene; the caches last.
GookaEngine::~GookaEngine() {
	delete _ui;
	delete _game;
	delete _scene;
	for (auto &i : _images)
		delete i._value;
	for (auto &f : _fonts)
		delete f._value;
	delete _music;
	delete _soundMan;
	delete _screen;
	delete _res;
	g_engine = nullptr;
}

bool GookaEngine::hasFeature(EngineFeature f) const {
	return f == kSupportsReturnToLauncher || f == kSupportsLoadingDuringRuntime || f == kSupportsSavingDuringRuntime || f == kSupportsSubtitleOptions;
}

// Prefers RGB555 (the native data format), falls back to RGB565.
bool GookaEngine::initScreen() {
	Common::List<Graphics::PixelFormat> formats;
	formats.push_back(Graphics::PixelFormat(2, 5, 5, 5, 0, 10, 5, 0, 0));
	formats.push_back(Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0));
	initGraphics(kScreenWidth, kScreenHeight, formats);

	const Graphics::PixelFormat format = _system->getScreenFormat();
	debug(1, "Gooka: screen format %s", format.toString().c_str());
	if (format.bytesPerPixel != 2)
		return false;

	// Expand 5-bit channels to 8 bits (x << 3 | x >> 2) before converting
	for (uint c = 0; c < 32768; c++) {
		const byte r = (c >> 10) & 0x1F, g = (c >> 5) & 0x1F, b = c & 0x1F;
		_lut[c] = format.RGBToColor((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2));
	}

	_screen = new Graphics::Screen();
	return true;
}

// Missing or broken images are cached as nullptr so they are reported only once.
Image *GookaEngine::getImage(uint16 id) {
	if (_images.contains(id))
		return _images[id];

	Image *img = nullptr;
	Common::ScopedPtr<Common::SeekableReadStream> s(_res->get(kResImage, id));
	if (s) {
		img = new Image();
		if (!img->load(s.get())) {
			warning("Gooka: cannot decode image %d", id);
			delete img;
			img = nullptr;
		}
	}
	_images[id] = img;
	return img;
}

// Draws onto the screen buffer and marks the area dirty (development tools only).
void GookaEngine::drawImage(uint16 id, uint frame, int x, int y, bool mirror) {
	Image *img = getImage(id);
	if (!img)
		return;
	img->draw(*_screen->surfacePtr(), frame, x, y, _lut, mirror);
	_screen->addDirtyRect(Common::Rect(x, y, x + img->getWidth(), y + img->getHeight()));
}

// Same caching scheme as getImage().
GookaFont *GookaEngine::getFont(uint16 id) {
	if (_fonts.contains(id))
		return _fonts[id];

	GookaFont *font = nullptr;
	Common::ScopedPtr<Common::SeekableReadStream> s(_res->get(kResFont, id));
	if (s) {
		font = new GookaFont();
		if (!font->load(s.get())) {
			delete font;
			font = nullptr;
		}
	}
	_fonts[id] = font;
	return font;
}

// Draws line by line; lines are separated by '\n' as in the text resources.
void GookaEngine::drawText(const Common::String &text, uint16 fontId, int x, int y, uint32 color) {
	GookaFont *font = getFont(fontId);
	if (!font)
		return;

	int lineY = y;
	uint start = 0;
	while (start <= text.size()) {
		size_t nl = text.findFirstOf('\n', start);
		if (nl == Common::String::npos)
			nl = text.size();
		font->drawString(_screen, text.substr(start, nl - start), x, lineY, kScreenWidth - x, color);
		lineY += font->getFontHeight();
		start = nl + 1;
	}
}

bool GookaEngine::playVideo(const Common::Path &filename) {
	Video::AVIDecoder decoder;
	decoder.setOutputPixelFormat(_system->getScreenFormat());

	if (!decoder.loadFile(filename)) {
		warning("Gooka: Could not load video '%s'", filename.toString().c_str());
		return false;
	}

	int width = decoder.getWidth();
	int height = decoder.getHeight();
	if (width <= 0 || height <= 0)
		return false;
	if (ConfMan.hasKey("gooka_scale_videos") && ConfMan.getBool("gooka_scale_videos")) {
		width = MAX(1, (int)((int64)width * _system->getHeight() / height));
		height = _system->getHeight();
		// Wider movies must still fit horizontally; never stretch or crop them.
		if (width > _system->getWidth()) {
			width = _system->getWidth();
			height = MAX(1, (int)((int64)decoder.getHeight() * width / decoder.getWidth()));
		}
	}
	const int x = (_system->getWidth() - width) / 2;
	const int y = (_system->getHeight() - height) / 2;
	debug("Gooka: AVI %s: %dx%d -> %dx%d at %d,%d", filename.toString().c_str(),
		decoder.getWidth(), decoder.getHeight(), width, height, x, y);
	const Graphics::PixelFormat screenFmt = _system->getScreenFormat();

	// Black out the game graphics around the video
	_system->fillScreen(0);
	_system->updateScreen();

	decoder.start();

	bool skipped = false;

	// In case the vid's color format isn't the same as the screen format
	Graphics::Surface converted;
	converted.create(decoder.getWidth(), decoder.getHeight(), screenFmt);
	Graphics::Surface scaled;
	if (width != decoder.getWidth() || height != decoder.getHeight())
		scaled.create(width, height, screenFmt);

	while (!shouldQuit() && !skipped && !decoder.endOfVideo()) {
		if (decoder.needsUpdate()) {
			const Graphics::Surface *frame = decoder.decodeNextFrame();
			if (frame) {
				if (frame->format != screenFmt) {
					Graphics::crossBlit((byte *)converted.getPixels(), (const byte *)frame->getPixels(),
										converted.pitch, frame->pitch, frame->w, frame->h,
										screenFmt, frame->format);
					frame = &converted;
				}
				if (scaled.getPixels()) {
					Graphics::scaleBlit((byte *)scaled.getPixels(), (const byte *)frame->getPixels(),
						scaled.pitch, frame->pitch, width, height, frame->w, frame->h, screenFmt);
					frame = &scaled;
				}
				_system->copyRectToScreen(frame->getPixels(), frame->pitch, x, y, frame->w, frame->h);
				_system->updateScreen();
			}
		}

		Common::Event event;
		while (_system->getEventManager()->pollEvent(event)) {
			if (event.type == Common::EVENT_KEYDOWN ||
			    event.type == Common::EVENT_LBUTTONDOWN)
				skipped = true;
		}

		_system->delayMillis(10);
	}

	converted.free();
	scaled.free();
	decoder.stop();

	return !skipped;
}

void GookaEngine::setState(EngineState state) {
	_nextState = state;
}

// Title: stop the sounds, leave the room (FUN_0041df70), then the title screen.
void GookaEngine::applyState() {
	if (_nextState == 0)
		return;
	const EngineState state = _nextState;
	_nextState = (EngineState)0;
	debugC(1, kDebugScript, "Engine state %d -> %d", _state, state);
	_state = state;

	if (state == kStateTitle) {
		_soundMan->stopEverything();
		_game->mainSequencer().stop();
		_game->leaveRoom();
		playVideo("12.AVI");
		_ui->showTitle();
	} else if (state == kStateCredits) {
		playVideo("13.AVI");
		_ui->startCredits();
	}
}

// Keys follow the original: Esc skips, F12 opens the menu here (the original saved and quit).
// A press and release within one tick is kept pressed for that tick so no click is lost.
void GookaEngine::pollInput() {
	Common::Event event;
	while (_eventMan->pollEvent(event)) {
		switch (event.type) {
		case Common::EVENT_MOUSEMOVE:
			_input.mouseX = event.mouse.x;
			_input.mouseY = event.mouse.y;
			break;
		case Common::EVENT_LBUTTONDOWN:
			_input.leftDown = true;
			_leftPressedInTick = true;
			_input.mouseX = event.mouse.x;
			_input.mouseY = event.mouse.y;
			break;
		case Common::EVENT_LBUTTONUP:
			if (_leftPressedInTick)
				_leftReleasePending = true;
			else
				_input.leftDown = false;
			break;
		case Common::EVENT_RBUTTONDOWN:
			_input.rightDown = true;
			_rightPressedInTick = true;
			break;
		case Common::EVENT_RBUTTONUP:
			if (_rightPressedInTick)
				_rightReleasePending = true;
			else
				_input.rightDown = false;
			break;
		case Common::EVENT_KEYDOWN:
			if (event.kbd.keycode == Common::KEYCODE_ESCAPE)
				_input.escPressed = true;
			else if (event.kbd.keycode == Common::KEYCODE_F12)
				_input.menuPressed = true;
			break;
		default:
			break;
		}
	}
}

// One 25 ms tick of the current state (Game::idle FUN_0042b940), then a full redraw.
void GookaEngine::tick() {
	applyState();

	switch (_state) {
	case kStateTitle:
		if (_ui->titleTick(_input))
			_state = kStatePlaying;
		break;
	case kStatePlaying:
		_ui->tick(_input);
		break;
	case kStateCredits:
		if (_ui->creditsTick(_input))
			setState(kStateTitle);
		break;
	default:
		// The videos of the repack use a codec ScummVM cannot decode: skip them
		setState(_state == kStateIntro ? kStateTitle : kStateCredits);
		break;
	}
	_input.escPressed = false;
	_input.menuPressed = false;
	_leftPressedInTick = _rightPressedInTick = false;
	if (_leftReleasePending) {
		_input.leftDown = false;
		_leftReleasePending = false;
	}
	if (_rightReleasePending) {
		_input.rightDown = false;
		_rightReleasePending = false;
	}
	_tickCounter++;

	_scene->render(*_screen->surfacePtr());
	_screen->markAllDirty();
	_screen->update();
}

// Creates the subsystems, then runs ticks at 40 Hz until the player quits.
Common::Error GookaEngine::run() {

	if (!initScreen())
		return Common::Error(Common::kUnsupportedColorMode, "Gooka needs a 16-bit display mode");

	_res = new ResourceManager();
	if (!_res->init())
		return Common::Error(Common::kNoGameDataFoundError, "Cannot open the .BIN resource files");

	_soundMan = new SoundManager(_mixer, _res);
	_music = new Music(_res);
	syncSoundSettings();

	const char *testScript = getenv("GOOKA_TEST");
	if (testScript && Common::String(testScript).hasPrefix("raw:")) {
		// Resource viewer mode without the game (see devtools.cpp)
		runTestScript(Common::String(testScript).substr(4));
		while (!shouldQuit())
			waitMillis(20);
		return Common::kNoError;
	}

	_scene = new Scene();
	_game = new Game();
	_ui = new Ui();
	_game->init();
	setState(kStateTitle);
	applyState();

	if (testScript)
		runTestScript(testScript);

	uint32 nextTick = _system->getMillis();
	while (!shouldQuit()) {
		pollInput();
		tick();

		nextTick += kTickMillis;
		const uint32 now = _system->getMillis();
		if (nextTick > now)
			_system->delayMillis(nextTick - now);
		else
			nextTick = now; // running late: do not try to catch up
	}

	return Common::kNoError;
}

// Pumps events and keeps ticking the game while waiting.
void GookaEngine::waitMillis(uint32 ms) {
	const uint32 end = _system->getMillis() + ms;
	while (!shouldQuit() && _system->getMillis() < end) {
		pollInput();
		if (_scene)
			tick();
		else
			_screen->update();
		_system->delayMillis(kTickMillis);
	}
}

} // End of namespace Gooka
