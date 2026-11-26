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

#include "mohawk/livingbooks.h"
#include "mohawk/resource.h"
#include "mohawk/cursors.h"
#include "mohawk/video.h"

#include "common/config-manager.h"
#include "common/error.h"
#include "common/events.h"
#include "common/fs.h"
#include "common/archive.h"
#include "common/textconsole.h"
#include "common/system.h"
#include "common/memstream.h"

#include "graphics/paletteman.h"

#include "engines/util.h"
#include "engines/ce_test_cli.h"

#include "gui/message.h"

#include "graphics/cursorman.h"

namespace Mohawk {

Common::JSONValue *MohawkEngine_LivingBooks::testCLIState() {
	if (!CETestCLI::active())
		return CETestCLI::unsupported();
	return livingBooksState(false);
}

Common::JSONValue *MohawkEngine_LivingBooks::diagnosticState() {
	return livingBooksState(true);
}

void MohawkEngine_LivingBooks::recordVisibilityEvent(uint16 item, int16 before, int16 after, bool global, bool requested) {
	if (_visibilityEvents.size() == 64) _visibilityEvents.remove_at(0);
	VisibilityEvent event = { _system->getMillis(), item, before, after, global, requested };
	_visibilityEvents.push_back(event);
}

Common::JSONValue *MohawkEngine_LivingBooks::livingBooksState(bool detailed) {
	using namespace Common;
	JSONObject state;
	state["ok"] = new JSONValue(true);
	state["mode"] = new JSONValue((long long)_curMode);
	state["page"] = new JSONValue((long long)_curPage);
	state["subpage"] = new JSONValue((long long)_curSubPage);
	state["phase"] = new JSONValue((long long)_phase);
	state["page_file"] = new JSONValue(_diagnosticPageFile);
	state["read_only"] = new JSONValue(_page && _readOnly);
	state["intro_done"] = new JSONValue(_page && _introDone);
	state["pending_notifications"] = new JSONValue((long long)_notifyEvents.size());
	state["pending_delayed_events"] = new JSONValue((long long)_eventQueue.size());
	state["focus"] = new JSONValue((long long)(_focus ? _focus->getId() : 0));
	state["cursor_visible"] = new JSONValue(CursorMan.isVisible());
	state["key_bindings"] = new JSONValue((long long)_keyEvents.size());
	state["rollover"] = new JSONValue((long long)(_rollover ? _rollover->getId() : 0));
	JSONObject variables;
	static const char *const observed[] = { "dTool", "InBasket", "vmItemName", "vmVelx", "vmVely", "vmAccelx", "vmAccely", "TurnOn",
		"JumCnt", "matches", "strlen", "str1", "curQD", "curP", "iterate",
		"QD1", "QD2", "QD3", "QD4", "QD5", "QD6", "P1", "P2", "P3", "P4", "P5", "P6",
		"ClockRoll", "HandDown", "HourChime", "FirstClick", "ResetCursor", "ZoneNum", "PrevZoneNum",
		"ClockRotation", "HrNum", "CurMin", "PrevMin", "CurHr", "PrevHr", "PlayHourNum", "AddHour" };
	for (uint i = 0; i < ARRAYSIZE(observed); ++i) {
		if (!_variables.contains(observed[i]))
			continue;
		const LBValue &value = _variables[observed[i]];
		if (value.type == kLBValueInteger)
			variables[observed[i]] = new JSONValue((long long)value.integer);
		else if (value.type == kLBValueString)
			variables[observed[i]] = new JSONValue(value.string);
	}
	state["variables"] = new JSONValue(variables);
	JSONArray items;
	for (uint i = 0; i < _items.size(); ++i) {
		LBItem *source = _items[i];
		JSONObject item;
		item["id"] = new JSONValue((long long)source->getId());
		item["name"] = new JSONValue(source->getName());
		if (detailed) source->appendDiagnosticState(item);
		item["loaded"] = new JSONValue(source->isLoaded());
		item["visible"] = new JSONValue(source->isVisible());
		item["visibility_balance"] = new JSONValue((long long)source->getVisibilityBalance());
		item["enabled"] = new JSONValue(source->isEnabled());
		item["dragging"] = new JSONValue(source->isDragging());
		item["playing"] = new JSONValue(source->isPlaying());
		item["position"] = new JSONValue((long long)source->getPosition());
		item["uninitialized_draws"] = new JSONValue((long long)source->getUninitializedDraws());
		item["sound"] = new JSONValue((long long)source->getActiveSound());
		item["sound_samples"] = new JSONValue((long long)source->getSoundSamples());
		item["sound_starts"] = new JSONValue((long long)source->getSoundStarts());
		const Rect &rect = source->getRect();
		JSONArray bounds;
		bounds.push_back(new JSONValue((long long)rect.left));
		bounds.push_back(new JSONValue((long long)rect.top));
		bounds.push_back(new JSONValue((long long)rect.right));
		bounds.push_back(new JSONValue((long long)rect.bottom));
		item["rect"] = new JSONValue(bounds);
		items.push_back(new JSONValue(item));
	}
	state["items"] = new JSONValue(items);
	JSONArray order;
	for (Common::List<LBItem *>::const_iterator i = _orderedItems.reverse_begin(); i != _orderedItems.end(); --i)
		order.push_back(new JSONValue((long long)(*i)->getId()));
	state["draw_order_back_to_front"] = new JSONValue(order);
	JSONArray renders;
	for (const RenderEvent &event : _renderEvents) {
		JSONObject frame;
		frame["serial"] = new JSONValue((long long)event.serial);
		frame["millis"] = new JSONValue((long long)event.millis);
		frame["mode"] = new JSONValue((long long)event.mode);
		frame["page"] = new JSONValue((long long)event.page);
		frame["subpage"] = new JSONValue((long long)event.subpage);
		frame["phase"] = new JSONValue((long long)event.phase);
		JSONArray sprites;
		for (uint16 id : event.sprites) sprites.push_back(new JSONValue((long long)id));
		frame["drawable_animations"] = new JSONValue(sprites);
		renders.push_back(new JSONValue(frame));
	}
	state["render_history"] = new JSONValue(renders);
	if (detailed) {
		JSONArray history;
		for (const VisibilityEvent &event : _visibilityEvents) {
			JSONObject item;
			item["millis"] = new JSONValue((long long)event.millis);
			item["item"] = new JSONValue((long long)event.item);
			item["before"] = new JSONValue((long long)event.before);
			item["after"] = new JSONValue((long long)event.after);
			item["global"] = new JSONValue(event.global);
			item["requested"] = new JSONValue(event.requested);
			history.push_back(new JSONValue(item));
		}
		state["visibility_history"] = new JSONValue(history);
	}
	return new JSONValue(state);
}

// read a null-terminated string from a stream
Common::String MohawkEngine_LivingBooks::readString(Common::ReadStream *stream) {
	Common::String ret;
	while (!stream->eos()) {
		byte in = stream->readByte();
		if (!in)
			break;
		ret += in;
	}
	return ret;
}

// read a rect from a stream
Common::Rect MohawkEngine_LivingBooks::readRect(Common::ReadStreamEndian *stream) {
	Common::Rect rect;

	// the V1 mac games have their rects in QuickDraw order
	if (isPreMohawk() && getPlatform() == Common::kPlatformMacintosh) {
		rect.top = stream->readSint16();
		rect.left = stream->readSint16();
		rect.bottom = stream->readSint16();
		rect.right = stream->readSint16();
	} else {
		rect.left = stream->readSint16();
		rect.top = stream->readSint16();
		rect.right = stream->readSint16();
		rect.bottom = stream->readSint16();
	}

	return rect;
}

MohawkEngine_LivingBooks::MohawkEngine_LivingBooks(OSystem *syst, const MohawkGameDescription *gamedesc) : MohawkEngine(syst, gamedesc) {
	DebugMan.addDebugChannel(kDebugCode, "Code", "Track Script Execution");

	_needsUpdate = false;
	_needsRedraw = false;
	_screenWidth = _screenHeight = 0;

	_curLanguage = 1;
	_curSelectedPage = 1;
	_curMode = kLBIntroMode;
	_curPage = _curSubPage = 0;
	_phase = kLBPhaseInit;

	_alreadyShowedIntro = false;

	_rnd = new Common::RandomSource("livingbooks");

	_sound = nullptr;
	_video = nullptr;
	_page = nullptr;
	_focus = _rollover = nullptr;

	const Common::FSNode gameDataDir(ConfMan.getPath("path"));
	// Rugrats
	SearchMan.addSubDirectoryMatching(gameDataDir, "program", 0, 2);
	SearchMan.addSubDirectoryMatching(gameDataDir, "Rugrats Adventure Game", 0, 2);
	// CarmenTQ
	SearchMan.addSubDirectoryMatching(gameDataDir, "95instal", 0, 4);

	// Sheila Rae, the Brave (Europe version) contains a junk line (bug #13920) 
	_bookInfoFile.requireKeyValueDelimiter();
}

MohawkEngine_LivingBooks::~MohawkEngine_LivingBooks() {
	destroyPage();

	delete _sound;
	delete _video;
	delete _gfx;
	delete _rnd;
	_bookInfoFile.clear();
}

Common::Error MohawkEngine_LivingBooks::run() {
	MohawkEngine::run();

	if (!_mixer->isReady()) {
		return Common::kAudioDeviceInitFailed;
	}

	setDebugger(new LivingBooksConsole(this));
	// Load the book info from the detected file
	loadBookInfo(getBookInfoFileName());

	if (!_title.empty()) // Some games don't have the title stored
		debug("Starting Living Books Title \'%s\'", _title.c_str());
	if (!_copyright.empty())
		debug("Copyright: %s", _copyright.c_str());
	debug("This book has %d page(s) in %d language(s).", _numPages, _numLanguages);
	if (_poetryMode)
		debug("Running in poetry mode.");

	if (!_screenWidth || !_screenHeight)
		error("Could not find xRes/yRes variables");

	_gfx = new LBGraphics(this, _screenWidth, _screenHeight);
	_video = new VideoManager(this);
	_sound = new Sound(this);

	if (getGameType() != GType_LIVINGBOOKSV1)
		_cursor = new LivingBooksCursorManager_v2();
	else if (getPlatform() == Common::kPlatformMacintosh)
		_cursor = new MacCursorManager(getAppName());
	else
		_cursor = new NECursorManager(getAppName());

	_cursor->setDefaultCursor();
	_cursor->showCursor();

	if (!tryLoadPageStart(kLBIntroMode, 1))
		error("Could not load intro page");

	Common::Event event;
	while (!shouldQuit()) {
		while (_eventMan->pollEvent(event)) {
			LBItem *found = nullptr;

			switch (event.type) {
			case Common::EVENT_MOUSEMOVE:
				for (uint i = 0; i < _items.size(); ++i)
					_items[i]->updateMouse(event.mouse);
				if (_focus)
					_focus->handleMouseMove(event.mouse);
				for (Common::List<LBItem *>::const_iterator i = _orderedItems.begin(); i != _orderedItems.end(); ++i) {
					if ((*i)->rolloverContains(event.mouse)) {
						found = *i;
						break;
					}
				}
				if (found != _rollover) {
					LBItem *previous = _rollover;
					_rollover = found;
					if (previous)
						previous->dispatchEvent(kLBEventRolloverEnd);
					if (found && _rollover == found)
						found->dispatchEvent(kLBEventRolloverBegin);
				} else if (found) {
					found->dispatchEvent(kLBEventRolloverMove);
				}
				_needsUpdate = true;
				break;

			case Common::EVENT_LBUTTONUP:
				if (_focus)
					_focus->handleMouseUp(event.mouse);
				break;

			case Common::EVENT_LBUTTONDOWN:
				for (Common::List<LBItem *>::const_iterator i = _orderedItems.begin(); i != _orderedItems.end(); ++i) {
					if ((*i)->contains(event.mouse)) {
						found = *i;
						break;
					}
				}

				if (found && CursorMan.isVisible())
					found->handleMouseDown(event.mouse);
				break;

			case Common::EVENT_KEYDOWN:
				{
				if (dispatchKey(event.kbd, event.kbdRepeat))
					break;
				}
				switch (event.kbd.keycode) {
				case Common::KEYCODE_SPACE:
					pauseGame();
					break;

				case Common::KEYCODE_ESCAPE:
					if (_curMode == kLBIntroMode)
						tryLoadPageStart(kLBControlMode, 1);
					else
						_video->stopVideos();
					break;

				case Common::KEYCODE_LEFT:
					prevPage();
					break;

				case Common::KEYCODE_RIGHT:
					nextPage();
					break;

				default:
					break;
				}
				break;

			default:
				break;
			}
		}

		updatePage();

		if (_video->updateMovies())
			_needsUpdate = true;

		if (_needsUpdate) {
			_system->updateScreen();
			_needsUpdate = false;
		}

		// Cut down on CPU usage
		_system->delayMillis(10);

		// handle pending notifications
		while (_notifyEvents.size()) {
			NotifyEvent notifyEvent = _notifyEvents.pop();
			handleNotify(notifyEvent);
		}
	}

	return Common::kNoError;
}

void MohawkEngine_LivingBooks::pauseEngineIntern(bool pause) {
	MohawkEngine::pauseEngineIntern(pause);

	if (pause) {
		if (_video != nullptr) {
			_video->pauseVideos();
		}
	} else {
		if (_video != nullptr) {
			_video->resumeVideos();
		}
		_system->updateScreen();
	}
}

void MohawkEngine_LivingBooks::destroyPage() {
	_sound->stopSound();
	_lastSoundOwner = 0;
	_lastSoundId = 0;
	_soundLockOwner = 0;

	_gfx->clearCache();
	_video->stopVideos();

	_eventQueue.clear();

	delete _page;
	assert(_items.empty());
	assert(_orderedItems.empty());
	_page = nullptr;

	_notifyEvents.clear();

	_focus = nullptr;
	_rollover = nullptr;
}

// Replace any colons (originally a slash) with another character
static Common::String replaceColons(const Common::String &in, char replace) {
	Common::String out;

	for (uint32 i = 0; i < in.size(); i++) {
		if (in[i] == ':')
			out += replace;
		else
			out += in[i];
	}

	return out;
}

// Helper function to assist in opening pages
bool tryOpenPage(Archive *archive, const Common::String &fileName) {
	// Try the plain file name first
	if (archive->openFile(Common::Path(fileName)))
		return true;

	// No colons, then bail out
	if (!fileName.contains(':'))
		return false;

	// Try replacing colons with underscores (in case the original was
	// a Mac version and had slashes not as a separator).
	if (archive->openFile(Common::Path(replaceColons(fileName, '_'))))
		return true;

	// Try replacing colons with slashes (in case the original was a Mac
	// version and had slashes as a separator).
	if (archive->openFile(Common::Path(replaceColons(fileName, '/'))))
		return true;

	// Failed to open the archive
	return false;
}

bool MohawkEngine_LivingBooks::loadPage(LBMode mode, uint page, uint subpage) {
	destroyPage();

	Common::String name = stringForMode(mode);

	Common::String base;
	if (subpage)
		base = Common::String::format("Page%d.%d", page, subpage);
	else
		base = Common::String::format("Page%d", page);

	Common::String filename, leftover;

	filename = getFileNameFromConfig(name, base, leftover);
	_readOnly = false;

	if (filename.empty()) {
		leftover.clear();
		filename = getFileNameFromConfig(name, base + ".r", leftover);
		_readOnly = true;
	}

	// TODO: fading between pages
#if 0
	bool fade = false;
	if (leftover.contains("fade")) {
		fade = true;
	}
#endif

	if (leftover.contains("read")) {
		_readOnly = true;
	}
	if (leftover.contains("load")) {
		// FIXME: don't ignore this
		warning("ignoring 'load' for filename '%s'", filename.c_str());
	}
	if (leftover.contains("cut")) {
		// FIXME: don't ignore this
		warning("ignoring 'cut' for filename '%s'", filename.c_str());
	}
	if (leftover.contains("killgag")) {
		// FIXME: don't ignore this
		warning("ignoring 'killgag' for filename '%s'", filename.c_str());
	}

	Archive *pageArchive = createArchive();
	if (!filename.empty() && tryOpenPage(pageArchive, filename)) {
		_diagnosticPageFile = filename;
		_page = new LBPage(this);
		_page->open(pageArchive, 1000);
	} else {
		delete pageArchive;
		debug(2, "Could not find page %d.%d for '%s'", page, subpage, name.c_str());
		return false;
	}

	if (getFeatures() & GF_LB_10) {
		if (_readOnly) {
			error("found .r entry in Living Books 1.0 game");
		} else {
			// some very early versions of the LB engine don't have
			// .r entries in their book info; instead, it is just hardcoded
			// like this (which would unfortunately break later games)
			_readOnly = (mode != kLBControlMode && mode != kLBPlayMode);
		}
	}

	debug(1, "Page Version: %d", _page->getResourceVersion());

	_curMode = mode;
	_curPage = page;
	_curSubPage = subpage;

	_cursor->showCursor();

	_gfx->setPalette(1000);

	_phase = 0;
	_introDone = false;

	_needsRedraw = true;

	return true;
}

void MohawkEngine_LivingBooks::updatePage() {
	switch (_phase) {
	case kLBPhaseInit:
		for (uint32 i = 0; i < _items.size(); i++)
			_items[i]->startPhase(kLBPhaseCreate);

		for (uint32 i = 0; i < _items.size(); i++)
			_items[i]->startPhase(_phase);

		if (_curMode == kLBControlMode) {
			// hard-coded control page startup
			LBItem *item;

			uint16 page = _curPage;
			if (getFeatures() & GF_LB_10) {
				// Living Books 1.0 had the meanings of these pages reversed
				if (page == 2)
					page = 3;
				else if (page == 3)
					page = 2;
			}

			switch (page) {
			case 1:
				debug(2, "updatePage() for control page 1 (menu)");

				if (_poetryMode) {
					for (uint16 i = 0; i < _numPages; i++) {
						item = getItemById(1000 + i);
						if (item)
							item->setVisible(_curSelectedPage == i + 1);
						item = getItemById(1100 + i);
						if (item)
							item->setVisible(_curSelectedPage == i + 1);
					}
				}

				for (uint16 i = 0; i < _numLanguages; i++) {
					item = getItemById(100 + i);
					if (item)
						item->seek((i + 1 == _curLanguage) ? 0xFFFF : 1);
					item = getItemById(200 + i);
					if (item)
						item->setVisible(false);
				}

				item = getItemById(12);
				if (item)
					item->setVisible(false);

				if (_alreadyShowedIntro) {
					item = getItemById(10);
					if (item) {
						item->setVisible(false);
						item->seek(0xFFFF);
					}
				} else {
					_alreadyShowedIntro = true;
					item = getItemById(11);
					if (item)
						item->setVisible(false);
				}
				break;

			case 2:
				debug(2, "updatePage() for control page 2 (quit)");

				item = getItemById(12);
				if (item)
					item->setVisible(false);
				item = getItemById(13);
				if (item)
					item->setVisible(false);
				break;

			case 3:
				debug(2, "updatePage() for control page 3 (options)");

				for (uint i = 0; i < _numLanguages; i++) {
					item = getItemById(100 + i);
					if (item)
						item->setVisible(_curLanguage == i + 1);
				}
				for (uint i = 0; i < _numPages; i++) {
					item = getItemById(1000 + i);
					if (item)
						item->setVisible(_curSelectedPage == i + 1);
					item = getItemById(1100 + i);
					if (item)
						item->setVisible(_curSelectedPage == i + 1);
				}
				item = getItemById(202);
				if (item)
					item->setVisible(false);
				break;

			default:
				break;
			}
		}
		_phase++;
		break;

	case kLBPhaseIntro:
		for (uint32 i = 0; i < _items.size(); i++)
			_items[i]->startPhase(_phase);

		if (_curMode == kLBControlMode) {
			LBItem *item = getItemById(10);
			if (item)
				item->togglePlaying(false);
		}

		_phase++;
		break;

	case kLBPhaseMain:
		if (!_introDone)
			break;

		for (uint32 i = 0; i < _items.size(); i++)
			_items[i]->startPhase(_phase);

		_phase++;
		break;

	default:
		break;
	}

	while (_eventQueue.size()) {
		DelayedEvent delayedEvent = _eventQueue.pop();
		for (uint32 i = 0; i < _items.size(); i++) {
			if (_items[i] != delayedEvent.item)
				continue;

			switch (delayedEvent.type) {
			case kLBDelayedEventDestroy:
				_items.remove_at(i);
				i--;
				_orderedItems.remove(delayedEvent.item);
				_page->itemDestroyed(delayedEvent.item);
				delete delayedEvent.item;
				if (_focus == delayedEvent.item)
					_focus = nullptr;
				break;
			case kLBDelayedEventSetNotVisible:
				_items[i]->setVisible(false);
				break;
			case kLBDelayedEventDone:
				_items[i]->done(true);
				break;
			default:
				break;
			}

			break;
		}
	}

	for (uint16 i = 0; i < _items.size(); i++)
		_items[i]->update();

	if (_needsRedraw) {
		RenderEvent rendered;
		rendered.serial = ++_renderSerial;
		rendered.millis = _system->getMillis();
		rendered.mode = _curMode;
		rendered.page = _curPage;
		rendered.subpage = _curSubPage;
		rendered.phase = _phase;
		for (Common::List<LBItem *>::const_iterator i = _orderedItems.reverse_begin(); i != _orderedItems.end(); --i) {
			if ((*i)->hasDrawableAnimation()) rendered.sprites.push_back((*i)->getId());
			(*i)->draw();
		}
		if (_renderEvents.size() == 64) _renderEvents.remove_at(0);
		_renderEvents.push_back(rendered);

		_needsRedraw = false;
		_needsUpdate = true;
	}
}

void MohawkEngine_LivingBooks::addArchive(Archive *archive) {
	_mhk.push_back(archive);
}

void MohawkEngine_LivingBooks::removeArchive(Archive *archive) {
	for (uint i = 0; i < _mhk.size(); i++) {
		if (archive != _mhk[i])
			continue;
		_mhk.remove_at(i);
		return;
	}

	error("removeArchive didn't find archive");
}

void MohawkEngine_LivingBooks::addItem(LBItem *item) {
	_items.push_back(item);
	_orderedItems.push_front(item);
	item->_iterator = _orderedItems.begin();
}

void MohawkEngine_LivingBooks::removeItems(const Common::Array<LBItem *> &items) {
	for (uint i = 0; i < items.size(); i++) {
		bool found = false;
		for (uint16 j = 0; j < _items.size(); j++) {
			if (items[i] != _items[j])
				continue;
			found = true;
			_items.remove_at(j);
			break;
		}
		assert(found);
		_orderedItems.erase(items[i]->_iterator);
	}
}

void MohawkEngine_LivingBooks::setItemZOrder(LBItem *item, LBItem *relative, bool back) {
	if (item == relative)
		return;
	_orderedItems.erase(item->_iterator);
	Common::List<LBItem *>::iterator position = _orderedItems.begin();
	if (relative) {
		position = relative->_iterator;
		++position;
	} else if (back) {
		position = _orderedItems.end();
	}
	item->_iterator = _orderedItems.insert(position, item);
	_needsRedraw = true;
}

LBItem *MohawkEngine_LivingBooks::getItemById(uint16 id) {
	for (uint16 i = 0; i < _items.size(); i++)
		if (_items[i]->getId() == id)
			return _items[i];

	return nullptr;
}

LBItem *MohawkEngine_LivingBooks::getItemByName(Common::String name) {
	for (uint16 i = 0; i < _items.size(); i++)
		if (_items[i]->getName().equalsIgnoreCase(name))
			return _items[i];

	return nullptr;
}

void MohawkEngine_LivingBooks::setFocus(LBItem *focus) {
	_focus = focus;
}

void MohawkEngine_LivingBooks::registerKeyEvent(LBItem *owner, byte code, byte modifiers, byte character, byte repeats, uint16 target, byte event) {
	RegisteredKeyEvent binding = { owner, code, modifiers, character, repeats, event, target };
	for (uint i = 0; i < _keyEvents.size(); ++i) {
		RegisteredKeyEvent &old = _keyEvents[i];
		if (old.code == code && old.modifiers == modifiers && old.character == character && old.repeats == repeats) {
			old = binding;
			return;
		}
	}
	_keyEvents.push_back(binding);
}

void MohawkEngine_LivingBooks::removeKeyEvents(LBItem *owner) {
	for (uint i = 0; i < _keyEvents.size();) {
		if (_keyEvents[i].owner == owner)
			_keyEvents.remove_at(i);
		else
			++i;
	}
}

void MohawkEngine_LivingBooks::clearRollover(LBItem *item, bool notify) {
	if (_rollover != item)
		return;
	_rollover = nullptr;
	if (notify)
		item->dispatchEvent(kLBEventRolloverEnd);
}

bool MohawkEngine_LivingBooks::dispatchKey(const Common::KeyState &key, bool repeat) {
	uint code = key.keycode;
	if (code == Common::KEYCODE_LEFT) code = 0x25;
	else if (code == Common::KEYCODE_RIGHT) code = 0x27;
	else if (code >= Common::KEYCODE_a && code <= Common::KEYCODE_z) code = code - 'a' + 'A';
	uint modifiers = ((key.flags & Common::KBD_SHIFT) ? LBKEY_MOD_SHIFT : 0)
		| ((key.flags & Common::KBD_CTRL) ? LBKEY_MOD_CTRL : 0)
		| ((key.flags & Common::KBD_ALT) ? LBKEY_MOD_ALT : 0);
	for (uint i = 0; i < _keyEvents.size(); ++i) {
		RegisteredKeyEvent binding = _keyEvents[i];
		if (binding.code != code || binding.modifiers != modifiers || (repeat && !binding.repeats))
			continue;
		LBItem *target = binding.target ? getItemById(binding.target) : binding.owner;
		if (target)
			target->dispatchEvent(binding.event);
		return true;
	}
	return false;
}

void MohawkEngine_LivingBooks::setEnableForAll(bool enable, LBItem *except) {
	for (uint16 i = 0; i < _items.size(); i++)
		if (except != _items[i])
			_items[i]->setEnabled(enable);
}

void MohawkEngine_LivingBooks::notifyAll(uint16 data, uint16 from) {
	for (uint16 i = 0; i < _items.size(); i++)
		_items[i]->notify(data, from);
}

void MohawkEngine_LivingBooks::queueDelayedEvent(DelayedEvent event) {
	_eventQueue.push(event);
}

bool MohawkEngine_LivingBooks::playSound(LBItem *source, uint16 resourceId, bool loop) {
	// Ambient beds are independent mixer streams, not foreground stop targets.
	if (source->isAmbient())
		return _sound->playSound(resourceId, Audio::Mixer::kMaxChannelVolume, loop) != nullptr;
	if (_lastSoundId && !_sound->isPlaying(_lastSoundId))
		_lastSoundId = 0;

	if (!source->isAmbient() || !_sound->isPlaying()) {
		if (!_soundLockOwner) {
			if (_lastSoundId && _lastSoundOwner != source->getId())
				if (source->getSoundPriority() >= _lastSoundPriority)
					return false;
		} else {
			if (_soundLockOwner != source->getId() && source->getSoundPriority() >= _maxSoundPriority)
				return false;
		}

		if (_lastSoundId)
			_sound->stopSound(_lastSoundId);

		_lastSoundOwner = source->getId();
		_lastSoundPriority = source->getSoundPriority();
	}

	_lastSoundId = resourceId;
	_sound->playSound(resourceId, Audio::Mixer::kMaxChannelVolume, loop);

	return true;
}

void MohawkEngine_LivingBooks::lockSound(LBItem *owner, bool lock) {
	if (!lock) {
		_soundLockOwner = 0;
		return;
	}

	if (_soundLockOwner || (owner->isAmbient() && _sound->isPlaying()))
		return;

	if (_lastSoundId && !_sound->isPlaying(_lastSoundId))
		_lastSoundId = 0;

	_soundLockOwner = owner->getId();
	_maxSoundPriority = owner->getSoundPriority();
	if (_lastSoundId && _maxSoundPriority <= _lastSoundPriority) {
		_sound->stopSound(_lastSoundId);
		_lastSoundId = 0;
	}
}

Common::SeekableReadStreamEndian *MohawkEngine_LivingBooks::wrapStreamEndian(uint32 tag, uint16 id) {
	Common::SeekableReadStream *dataStream = getResource(tag, id);
	return new Common::SeekableReadStreamEndianWrapper(dataStream, isBigEndian(), DisposeAfterUse::YES);
}

Archive *MohawkEngine_LivingBooks::createArchive() const {
	if (isPreMohawk())
		return new LivingBooksArchive_v1();

	return new MohawkArchive();
}

bool MohawkEngine_LivingBooks::isPreMohawk() const {
	return getGameType() == GType_LIVINGBOOKSV1
		|| (getGameType() == GType_LIVINGBOOKSV2 && getPlatform() == Common::kPlatformMacintosh);
}

void MohawkEngine_LivingBooks::addNotifyEvent(NotifyEvent event) {
	_notifyEvents.push(event);
}

bool MohawkEngine_LivingBooks::tryLoadPageStart(LBMode mode, uint page) {
	// try first subpage of the page
	if (loadPage(mode, page, 1))
		return true;

	// then just the plain page
	if (loadPage(mode, page, 0))
		return true;

	return false;
}

bool MohawkEngine_LivingBooks::tryDefaultPage() {
	if (_curMode == kLBCreditsMode || _curMode == kLBPreviewMode) {
		// go to options page
		if (getFeatures() & GF_LB_10) {
			if (tryLoadPageStart(kLBControlMode, 2))
				return true;
		} else {
			if (tryLoadPageStart(kLBControlMode, 3))
				return true;
		}
	}

	// go to menu page
	if (tryLoadPageStart(kLBControlMode, 1))
		return true;

	return false;
}

void MohawkEngine_LivingBooks::prevPage() {
	if (_curPage > 1 && (tryLoadPageStart(_curMode, _curPage - 1)))
		return;

	if (tryDefaultPage())
		return;

	error("Could not find page before %d.%d for mode %d", _curPage, _curSubPage, (int)_curMode);
}

void MohawkEngine_LivingBooks::nextPage() {
	// we try the next subpage first
	if (loadPage(_curMode, _curPage, _curSubPage + 1))
		return;

	if (tryLoadPageStart(_curMode, _curPage + 1))
		return;

	if (tryDefaultPage())
		return;

	error("Could not find page after %d.%d for mode %d", _curPage, _curSubPage, (int)_curMode);
}

void MohawkEngine_LivingBooks::handleNotify(NotifyEvent &event) {
	// hard-coded behavior (GUI/navigation)

	switch (event.type) {
	case kLBNotifyGUIAction:
		debug(2, "kLBNotifyGUIAction: %d", event.param);

		if (_curMode != kLBControlMode)
			break;

		// The scripting passes us the control ID as param, so we work
		// out which control was clicked, then run the relevant code.

		uint16 page;
		page = _curPage;
		if (getFeatures() & GF_LB_10) {
			// Living Books 1.0 had the meanings of these pages reversed
			if (page == 2)
				page = 3;
			else if (page == 3)
				page = 2;
		}

		switch (page) {
		case 1:
			// main menu
			if (_poetryMode)
				handleUIPoetryMenuClick(event.param);
			else
				handleUIMenuClick(event.param);
			break;

		case 2:
			// quit screen
			handleUIQuitClick(event.param);
			break;

		case 3:
			// options screen
			handleUIOptionsClick(event.param);
			break;

		default:
			break;
		}
		break;

	case kLBNotifyGoToControls:
		debug(2, "kLBNotifyGoToControls: %d", event.param);

		if (!tryLoadPageStart(kLBControlMode, 1))
			error("couldn't load controls page");
		break;

	case kLBNotifyChangePage:
		switch (event.param) {
		case 0xfffe:
			debug(2, "kLBNotifyChangePage: next page");
			nextPage();
			return;

		case 0xffff:
			debug(2, "kLBNotifyChangePage: previous page");
			prevPage();
			break;

		default:
			debug(2, "kLBNotifyChangePage: trying %d", event.param);
			if (!tryLoadPageStart(_curMode, event.param)) {
				if (!tryDefaultPage()) {
					error("failed to load default page after change to page %d (mode %d) failed", event.param, _curMode);
				}
			}
			break;
		}
		break;

	case kLBNotifyGotoQuit:
		debug(2, "kLBNotifyGotoQuit: %d", event.param);

		if (!tryLoadPageStart(kLBControlMode, 2))
			error("couldn't load quit page");
		break;

	case kLBNotifyIntroDone:
		debug(2, "kLBNotifyIntroDone: %d", event.param);

		if (event.param != 1)
			break;

		_introDone = true;

		// TODO: if !_readOnly, go to next page (-2 case above)
		// if in older one (not in e.g. 1.4 w/tortoise),
		//   if mode is 6 (kLBPlayMode?), go to next page (-2 case) if curr page > nPages (i.e. the end)
		// else, nothing

		if (!_readOnly)
			break;

		nextPage();
		break;

	case kLBNotifyChangeMode:
		if (getGameType() == GType_LIVINGBOOKSV1) {
			debug(2, "kLBNotifyChangeMode: %d", event.param);
			quitGame();
			break;
		}

		debug(2, "kLBNotifyChangeMode: v2 type %d", event.param);
		switch (event.param) {
		case 1:
			debug(2, "kLBNotifyChangeMode: mode %d, page %d.%d",
				event.newMode, event.newPage, event.newSubpage);
			// TODO: what is entry.newUnknown?
			if (!event.newMode)
				event.newMode = _curMode;
			if (!loadPage((LBMode)event.newMode, event.newPage, event.newSubpage)) {
				if (event.newPage != 0 || !loadPage((LBMode)event.newMode, _curPage, event.newSubpage))
					if (event.newSubpage != 0 || !loadPage((LBMode)event.newMode, event.newPage, 1))
						if (event.newSubpage != 1 || !loadPage((LBMode)event.newMode, event.newPage, 0))
							error("kLBNotifyChangeMode failed to move to mode %d, page %d.%d",
								event.newMode, event.newPage, event.newSubpage);
			}
			break;
		case 3:
			debug(2, "kLBNotifyChangeMode: new cursor '%s'", event.newCursor.c_str());
			_cursor->setCursor(event.newCursor);
			break;
		default:
			error("unknown v2 kLBNotifyChangeMode type %d", event.param);
		}
		break;

	case kLBNotifyCursorChange:
		debug(2, "kLBNotifyCursorChange: %d", event.param);

		// TODO: show/hide cursor according to parameter?
		break;

	case kLBNotifyPrintPage:
		debug(2, "kLBNotifyPrintPage: %d", event.param);

		warning("kLBNotifyPrintPage unimplemented");
		break;

	case kLBNotifyQuit:
		debug(2, "kLBNotifyQuit: %d", event.param);

		quitGame();
		break;

	default:
		error("Unknown notification %d (param 0x%04x)", event.type, event.param);
	}
}

} // End of namespace Mohawk
