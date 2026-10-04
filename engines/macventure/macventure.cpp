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
 * Based on
 * WebVenture (c) 2010, Sean Kasun
 * https://github.com/mrkite/webventure, http://seancode.com/webventure/
 *
 * Used with explicit permission from the author
 */

#include "common/system.h"
#include "common/debug-channels.h"
#include "common/debug.h"
#include "common/error.h"
#include "common/config-manager.h"
#include "common/str-enc.h"
#include "engines/advancedDetector.h"
#include "engines/util.h"

#include "macventure/macventure.h"

// To move
#include "common/file.h"

namespace MacVenture {

enum {
	kMaxMenuTitleLength = 30
};

enum {
	kFrameDelay = 20
};

enum {
	kDejaVuPoliceTimerGlobal = 6,
	kDejaVuPoliceTimerMaxSafeValue = 5,
	kDejaVuSlotMachineFirstLossScript = 914,
	kDejaVuSlotMachineScript = 915,
	kDejaVuSlotMachineRollMax = 3,
	kDejaVuSlotMachineWinningRoll = 1,
	kDejaVuSewerAlligatorSpawnScript = 970,
	kDejaVuSewerAlligatorRollMax = 2,
	kDejaVuSewerAlligatorSpawnRoll = 1,
	kDejaVuGunScript = 870,
	kDejaVuGunConsumeAmmoScript = 663,
	kDejaVuGunConsumeAmmoStackArgs = 4,
	kDejaVuMoveValidationScript = 771,
	kDejaVuMoveCapacityFailure = 15,
	kDejaVuTrenchCoatObject = 389,
	kDejaVuMuggerScript = 884,
	kDejaVuMuggerCounterGlobal = 24,
	kDejaVuMuggerDeathCounter = 5,
	kDejaVuMuggerMaxSafeCounter = 4
};

#ifdef MACVENTURE_TRACE_BUILD
static Common::String escapeTraceString(const Common::String &text) {
	Common::String out;
	for (uint i = 0; i < text.size(); ++i) {
		switch (text[i]) {
		case '\r':
			out += "\\r";
			break;
		case '\n':
			out += "\\n";
			break;
		case '\t':
			out += "\\t";
			break;
		case '"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		default:
			out += text[i];
			break;
		}
	}
	return out;
}
#endif

MacVentureEngine::MacVentureEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst) {
	_gameDescription = gameDesc;
	_rnd = new Common::RandomSource("macventure");

	_resourceManager = nullptr;
	_globalSettings = nullptr;
	_gui = nullptr;
	_world = nullptr;
	_scriptEngine = nullptr;
	_filenames = nullptr;

	_decodingDirectArticles = nullptr;
	_decodingNamingArticles = nullptr;
	_decodingIndirectArticles = nullptr;
	_textHuffman = nullptr;

	_soundManager = nullptr;

	_dataBundle = nullptr;

	_nextFrameTime = 0;

	debug("MacVenture::MacVentureEngine()");
}

MacVentureEngine::~MacVentureEngine() {
	debug("MacVenture::~MacVentureEngine()");

	if (_rnd)
		delete _rnd;

	if (_resourceManager)
		delete _resourceManager;

	if (_globalSettings)
		delete _globalSettings;

	if (_gui)
		delete _gui;

	if (_world)
		delete _world;

	if (_scriptEngine)
		delete _scriptEngine;

	if (_filenames)
		delete _filenames;

	if (_decodingDirectArticles)
		delete _decodingDirectArticles;

	if (_decodingNamingArticles)
		delete _decodingNamingArticles;

	if (_decodingIndirectArticles)
		delete _decodingIndirectArticles;

	if (_textHuffman)
		delete _textHuffman;

	if (_soundManager)
		delete _soundManager;

	if (_dataBundle)
		delete _dataBundle;
}

Common::Error MacVentureEngine::run() {
	debug("MacVenture::MacVentureEngine::init()");
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("engine.run.begin", "game=%s", getGameFileName());
#endif
	initGraphics(kScreenWidth, kScreenHeight);

	setInitialFlags();

	setDebugger(new Console(this));

	// Additional setup.
	debug("MacVentureEngine::init");
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("engine.init.begin", "gamePath=%s", _gamePath.getPath().toString().c_str());
#endif

	_resourceManager = new Common::MacResManager();
	if (!_resourceManager->open(getGameFileName()))
		error("ENGINE: Could not open %s as a resource fork", getGameFileName());
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("engine.resource_manager.opened", "game=%s", getGameFileName());
#endif

	// Engine-wide loading
	if (!loadGlobalSettings())
		error("ENGINE: Could not load the engine settings");
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("engine.global_settings.loaded", "dataBundle=%p", (const void *)_dataBundle);
#endif

	_oldTextEncoding = !loadTextHuffman();
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("engine.text_huffman.loaded", "oldTextEncoding=%d", _oldTextEncoding ? 1 : 0);
#endif

	_filenames = new StringTable(this, _resourceManager, kFilenamesStringTableID);
	_decodingDirectArticles = new StringTable(this, _resourceManager, kCommonArticlesStringTableID);
	_decodingNamingArticles = new StringTable(this, _resourceManager, kNamingArticlesStringTableID);
	_decodingIndirectArticles = new StringTable(this, _resourceManager, kIndirectArticlesStringTableID);
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("engine.string_tables.loaded", "subdir=%s title=%s object=%s filter=%s text=%s graphic=%s sound=%s",
		getFilePath(kSubdirPathID).toString().c_str(), getFilePath(kTitlePathID).toString().c_str(),
		getFilePath(kObjectPathID).toString().c_str(), getFilePath(kFilterPathID).toString().c_str(),
		getFilePath(kTextPathID).toString().c_str(), getFilePath(kGraphicPathID).toString().c_str(),
		getFilePath(kSoundPathID).toString().c_str());
#endif

	SearchMan.addSubDirectoryMatching(_gamePath, _filenames->getString(3));
	if (!strcmp(_gameDescription->gameId, "deja_vu"))
		SearchMan.addSubDirectoryMatching(_gamePath, "Deja Vu 2");
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("engine.search_paths.loaded", "canonicalSubdir=%s", _filenames->getString(3).c_str());
#endif

	loadDataBundle();
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("engine.data_bundle.loaded", "bundle=%p", (const void *)_dataBundle);
#endif

	// Big class instantiation
	_gui = new Gui(this, _resourceManager);
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("engine.gui.created", "gui=%p", (const void *)_gui);
#endif
	_world = new World(this, _resourceManager);
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("engine.world.created", "world=%p", (const void *)_world);
#endif
	_scriptEngine = new ScriptEngine(this, _world);
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("engine.script.created", "script=%p", (const void *)_scriptEngine);
#endif

	_soundManager = new SoundManager(this, _mixer);
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("engine.sound.created", "sound=%p", (const void *)_soundManager);
#endif

	int directSaveSlotLoading = ConfMan.getInt("save_slot");
	if (directSaveSlotLoading >= 0) {
		if (loadGameState(directSaveSlotLoading).getCode() != Common::kNoError) {
			error("ENGINE: Could not load game from slot '%d'", directSaveSlotLoading);
		}
	} else {
#ifdef MACVENTURE_TRACE_BUILD
		traceRuntime("engine.new_game.before", "save_slot=%d", directSaveSlotLoading);
#endif
		setNewGameState();
#ifdef MACVENTURE_TRACE_BUILD
		traceRuntime("engine.title.before", "state=%d", _gameState);
#endif
		_gui->drawTitle();
#ifdef MACVENTURE_TRACE_BUILD
		traceRuntime("engine.title.after", "state=%d", _gameState);
#endif
	}
	selectControl(kStartOrResume);
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("engine.initial_control.selected", "control=%d", _selectedControl);
#endif

#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("engine.self_window.add_child.before", "obj=1");
#endif
	_gui->addChild(kSelfWindow, 1);
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("engine.self_window.add_child.after", "obj=1");
	traceRuntime("engine.self_window.update.before", "obj=1");
#endif
	_gui->updateWindow(kSelfWindow, false);
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("engine.self_window.update.after", "obj=1");
#endif

	while (_gameState != kGameStateQuitting) {
		processEvents();

		if (!_enginePaused && _gameState != kGameStateQuitting && !_gui->isDialogOpen()) {

			if (_prepared) {
				_prepared = false;

				bool busy = _cmdReady || _halted;
				if (busy)
					_gui->setWaitCursor(true);

				if (!_halted)
					updateState(false);

				if (_cmdReady || _halted) {
					_halted = false;
					if (runScriptEngine()) {
						_halted = true;
						_paused = true;
					} else {
						_paused = false;
						updateState(true);
						updateControls();
						updateExits();
					}
				}

				if (_gameState == kGameStateLosing) {
					endGame();
				}

				if (busy)
					_gui->setWaitCursor(false);

				_gui->markRedraw();
			}
		}
		refreshScreen();
	}

#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("engine.run.end", "state=%d", _gameState);
#endif
	return Common::kNoError;
}

void MacVentureEngine::refreshScreen() {
	_gui->draw();
	g_system->updateScreen();

	uint32 now = g_system->getMillis();
	if (now < _nextFrameTime)
		g_system->delayMillis(_nextFrameTime - now);
	_nextFrameTime = g_system->getMillis() + kFrameDelay;
}

void MacVentureEngine::newGame() {
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("game.new", "previousState=%d", _gameState);
#endif
	_world->startNewGame();
	reset();
	setInitialFlags();
	setNewGameState();
}

void MacVentureEngine::setInitialFlags(GameState gameState) {
	_paused = false;
	_halted = false;
	_cmdReady = false;
	_haltedAtEnd = false;
	_haltedInSelection = false;
	_clickToContinue = true;
	_gameState = gameState;
	_destObject = 0;
	_prepared = true;
	_enginePaused = false;
	_consoleRowsSincePause = 0;
	_consolePageStartRow = 0;
}

void MacVentureEngine::setNewGameState() {
	_cmdReady = true;
	ObjID playerParent = _world->getObjAttr(1, kAttrParentObject);
	_currentSelection.push_back(playerParent);// Push the parent of the player
	_world->setObjAttr(playerParent, kAttrContainerOpen, 1);
}

void MacVentureEngine::reset() {
	resetInternals();
	resetGui();
}

void MacVentureEngine::resetInternals() {
	_gui->resetWindows();
	_scriptEngine->reset();
	_currentSelection.clear();
	_objQueue.clear();
	_textQueue.clear();
	_consoleRowsSincePause = 0;
	_consolePageStartRow = 0;
}

void MacVentureEngine::resetGui() {
	_gui->reloadInternals();
	updateControls();
	updateExits();
	_gui->markRedraw();
	refreshScreen();
}

void MacVentureEngine::requestQuit() {
	// TODO: Display save game dialog and such
	_gameState = kGameStateQuitting;
}

void MacVentureEngine::requestUnpause() {
	_paused = false;
	_gameState = kGameStatePlaying;
}

void MacVentureEngine::selectControl(ControlAction id) {
	debugC(2, kMVDebugMain, "Select control %x", id);
	if (id == kClickToContinue) {
		uint rowCount = _gui->getConsoleRowCount();
		uint visibleRows = _gui->getConsoleVisibleRows();
		uint pageRows = MAX<uint>(1, visibleRows - 1);
#ifdef MACVENTURE_TRACE_BUILD
		traceRuntime("control.continue", "rowCount=%u visibleRows=%u pageRows=%u pageStart=%u rowsSincePause=%u",
			rowCount, visibleRows, pageRows, _consolePageStartRow, _consoleRowsSincePause);
#endif
		if (_consolePageStartRow + visibleRows < rowCount) {
			_consolePageStartRow = MIN<uint>(_consolePageStartRow + pageRows, rowCount > visibleRows ? rowCount - visibleRows : 0);
			clickToContinue();
			return;
		}

		_consoleRowsSincePause = 0;
		_consolePageStartRow = rowCount;
		_clickToContinue = false;
		_enginePaused = false;
		_paused = true;
		_prepared = true;
		return;
	}

	if (!_clickToContinue) {
		_consoleRowsSincePause = 0;
		_consolePageStartRow = _gui->getConsoleRowCount();
	}

	_selectedControl = id;
	refreshReady();
}

void MacVentureEngine::refreshReady() {
	switch (getInvolvedObjects()) {
	case 0: // No selected object
		_cmdReady = true;
		break;
	case 1: // We have some selected object
		_cmdReady = _currentSelection.size() != 0;
		break;
	case 2:
		if (_destObject > 0) // We have a destination selected
			_cmdReady = true;
		break;
	default:
		break;
	}
}

void MacVentureEngine::preparedToRun() {
	_prepared = true;
}

void MacVentureEngine::gameChanged() {
	_gameChanged = true;
}

void MacVentureEngine::winGame() {
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("game.win", "state=%d", _gameState);
#endif
	_paused = true;
	_gui->loadDiploma();
	_gameState = kGameStateWinning;
}

void MacVentureEngine::loseGame() {
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("game.lose", "state=%d", _gameState);
#endif
	_gui->showPrebuiltDialog(kLoseGameDialog);
	_paused = true;
	//_gameState = kGameStateLosing;
}

void MacVentureEngine::clickToContinue() {
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("console.pause", "rowCount=%u visibleRows=%u pageStart=%u rowsSincePause=%u",
		_gui->getConsoleRowCount(), _gui->getConsoleVisibleRows(), _consolePageStartRow, _consoleRowsSincePause);
#endif
	_gui->scrollConsoleToRow(_consolePageStartRow);
	_clickToContinue = true;
	_enginePaused = true;
}

void MacVentureEngine::enqueueObject(ObjectQueueID type, ObjID objID, ObjID target) {
	QueuedObject obj;
	obj.id = type;

	if (type == kUpdateObject && isObjEnqueued(objID)) {
		return;
	}

	if (type == kUpdateWindow) {
		obj.target = target;
	}

	if (type != kHightlightExits) {
		obj.object = objID;
		obj.parent = _world->getObjAttr(objID, kAttrParentObject);
		obj.x = _world->getObjAttr(objID, kAttrPosX);
		obj.y = _world->getObjAttr(objID, kAttrPosY);
		obj.exitx = _world->getObjAttr(objID, kAttrExitX);
		obj.exity = _world->getObjAttr(objID, kAttrExitY);
		obj.hidden = _world->getObjAttr(objID, kAttrHiddenExit);
		obj.offscreen = _world->getObjAttr(objID, kAttrInvisible);
		obj.invisible = _world->getObjAttr(objID, kAttrUnclickable);
	}
	_objQueue.push_back(obj);
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("queue.object.enqueue", "type=%d obj=%u target=%u parent=%u pos=(%u,%u) exit=(%u,%u) hidden=%d offscreen=%d invisible=%d size=%u",
		type, objID, target, obj.parent, obj.x, obj.y, obj.exitx, obj.exity, obj.hidden ? 1 : 0, obj.offscreen ? 1 : 0, obj.invisible ? 1 : 0, (uint)_objQueue.size());
#endif
}

void MacVentureEngine::enqueueText(TextQueueID type, ObjID target, ObjID source, ObjID text) {
	QueuedText newText;
	newText.id = type;
	newText.destination = target;
	newText.source = source;
	newText.asset = text;
	_textQueue.push_back(newText);
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("queue.text.enqueue", "type=%d text=%u source=%u target=%u size=%u", type, text, source, target, (uint)_textQueue.size());
#endif
}

void MacVentureEngine::enqueueSound(SoundQueueID type, ObjID target) {
	QueuedSound newSound;
	newSound.id = type;
	newSound.reference = target;
	_soundQueue.push_back(newSound);
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("queue.sound.enqueue", "type=%d sound=%u size=%u", type, target, (uint)_soundQueue.size());
#endif
}

void MacVentureEngine::handleObjectSelect(ObjID objID, WindowReference win, bool shiftPressed, bool isDoubleClick) {
	if (win == kExitsWindow) {
		win = kMainGameWindow;
	}

	const WindowData &windata = _gui->getWindowData(win);

	if (shiftPressed) {
		if (objID == 0) {
			objID = windata.objRef;
		}
		if (objID > 0) {
			if (findObjectInArray(objID, _currentSelection) != -1) {
				unselectObject(objID);
			} else {
				selectObject(objID);
			}
			refreshReady();
			preparedToRun();
		}
	} else {
		if (_selectedControl && _currentSelection.size() > 0 && getInvolvedObjects() > 1) {
			if (objID == 0) {
				selectPrimaryObject(windata.objRef);
			} else {
				selectPrimaryObject(objID);
			}
			preparedToRun();
		} else {
			if (objID == 0) {
				unselectAll();
				objID = windata.objRef;
			}
			if (objID > 0) {
				int currentObjectIndex = findObjectInArray(objID, _currentSelection);

				if (currentObjectIndex == -1)
					unselectAll();

				if (isDoubleClick) {
					selectObject(objID);
					_destObject = objID;
					setDeltaPoint(Common::Point(0, 0));
					if (!_cmdReady) {
						selectControl(kActivateObject);
						_cmdReady = true;
					}
				} else {
					selectObject(objID);
					if (getInvolvedObjects() == 1)
						_cmdReady = true;
				}
				preparedToRun();
			}
		}
	}
}

void MacVentureEngine::handleObjectDrop(ObjID objID, Common::Point delta, ObjID newParent) {
	_destObject = newParent;
	setDeltaPoint(delta);
	selectControl(kMoveObject);
	refreshReady();
	preparedToRun();
}

void MacVentureEngine::setDeltaPoint(Common::Point newPos) {
	debugC(4, kMVDebugMain, "Update delta: Old(%d, %d), New(%d, %d)",
		_deltaPoint.x, _deltaPoint.y,
		newPos.x, newPos.y);
	_deltaPoint = newPos;
}

void MacVentureEngine::updateWindow(WindowReference winID) {
	_gui->updateWindow(winID, true);
}

bool MacVentureEngine::showTextEntry(ObjID text, ObjID srcObj, ObjID destObj) {
	debugC(3, kMVDebugMain, "Showing speech dialog, asset %d from %d to %d", text, srcObj, destObj);
	Common::String title = _world->getText(text, srcObj, destObj);
	_gui->getTextFromUser(title);

	_prepared = false;
	warning("Show text entry: not fully tested");
	return true;
}

void MacVentureEngine::setTextInput(const Common::String &content) {
	_prepared = true;
	_userInput = content;
	_clickToContinue = false;
	_enginePaused = false;
}

Common::String MacVentureEngine::getUserInput() {
	return _userInput;
}

Common::Path MacVentureEngine::getDiplomaFileName() {
	Common::SeekableReadStream *res;
	res = _resourceManager->getResource(MKTAG('S', 'T', 'R', ' '), kDiplomaFilenameID);
	if (!res)
		return "";

	byte length = res->readByte();
	char *fileName = new char[length + 1];
	res->read(fileName, length);
	fileName[length] = '\0';

	Common::U32String result(fileName, Common::kMacRoman);

	delete[] fileName;
	delete res;

	Common::Path path(result);
	if (!strcmp(_gameDescription->gameId, "deja_vu") && !Common::File::exists(path))
		path = Common::Path("Deja Diploma");

	return path;
}

Common::Path MacVentureEngine::getStartGameFileName() {
	Common::SeekableReadStream *res;
	res = _resourceManager->getResource(MKTAG('S', 'T', 'R', ' '), kStartGameFilenameID);
	if (!res)
		return "";

	byte length = res->readByte();
	char *fileName = new char[length + 1];
	res->read(fileName, length);
	fileName[length] = '\0';

	Common::U32String result(fileName, Common::kMacRoman);

	delete[] fileName;
	delete res;

	Common::Path path(result);
	if (!strcmp(_gameDescription->gameId, "deja_vu") && !Common::File::exists(path))
		path = Common::Path("Deja Game");

	return path;
}

const GlobalSettings& MacVentureEngine::getGlobalSettings() const {
	return *_globalSettings;
}

uint16 MacVentureEngine::clampGlobalValue(uint32 attrID, uint16 value) const {
	if (strcmp(_gameDescription->gameId, "deja_vu") ||
			attrID != kDejaVuPoliceTimerGlobal ||
			value <= kDejaVuPoliceTimerMaxSafeValue ||
			!ConfMan.hasKey("deja_vu_freeze_police_timer") ||
			!ConfMan.getBool("deja_vu_freeze_police_timer")) {
		return value;
	}

#ifdef MACVENTURE_TRACE_BUILD
	const_cast<MacVentureEngine *>(this)->traceRuntime("cheat.freeze_police_timer", "global=%u requested=%u clamped=%u",
		attrID, value, kDejaVuPoliceTimerMaxSafeValue);
#endif

	return kDejaVuPoliceTimerMaxSafeValue;
}

int16 MacVentureEngine::adjustGlobalValue(uint32 scriptID, ControlAction action, uint32 globalID, int16 oldValue, int16 value) const {
	if (strcmp(_gameDescription->gameId, "deja_vu") ||
			scriptID != kDejaVuMuggerScript ||
			action != kHit ||
			globalID != kDejaVuMuggerCounterGlobal ||
			oldValue != kDejaVuMuggerMaxSafeCounter ||
			value != kDejaVuMuggerDeathCounter ||
			!ConfMan.hasKey("deja_vu_mugger_wont_kill") ||
			!ConfMan.getBool("deja_vu_mugger_wont_kill")) {
		return value;
	}

#ifdef MACVENTURE_TRACE_BUILD
	const_cast<MacVentureEngine *>(this)->traceRuntime("cheat.mugger_wont_kill", "script=%u global=%u old=%d requested=%d clamped=%d",
		scriptID, globalID, oldValue, value, kDejaVuMuggerMaxSafeCounter);
#endif

	return kDejaVuMuggerMaxSafeCounter;
}

int16 MacVentureEngine::adjustRandomValue(uint32 scriptID, int16 max, int16 value) const {
	if (strcmp(_gameDescription->gameId, "deja_vu")) {
		return value;
	}

	if (scriptID == kDejaVuSlotMachineScript &&
			max == kDejaVuSlotMachineRollMax &&
			ConfMan.hasKey("deja_vu_rig_slot_machine") &&
			ConfMan.getBool("deja_vu_rig_slot_machine")) {
#ifdef MACVENTURE_TRACE_BUILD
		const_cast<MacVentureEngine *>(this)->traceRuntime("cheat.rig_slot_machine", "script=%u max=%d requested=%d forced=%d",
			scriptID, max, value, kDejaVuSlotMachineWinningRoll);
#endif
		return kDejaVuSlotMachineWinningRoll;
	}

	if (scriptID == kDejaVuSewerAlligatorSpawnScript &&
			max == kDejaVuSewerAlligatorRollMax &&
			value == kDejaVuSewerAlligatorSpawnRoll &&
			ConfMan.hasKey("deja_vu_no_alligators") &&
			ConfMan.getBool("deja_vu_no_alligators")) {
#ifdef MACVENTURE_TRACE_BUILD
		const_cast<MacVentureEngine *>(this)->traceRuntime("cheat.no_alligators", "script=%u max=%d requested=%d forced=0",
			scriptID, max, value);
#endif
		return 0;
	}

	return value;
}

int16 MacVentureEngine::adjustQueuedScript(uint32 scriptID) const {
	if (strcmp(_gameDescription->gameId, "deja_vu") ||
			scriptID != kDejaVuSlotMachineFirstLossScript ||
			!ConfMan.hasKey("deja_vu_rig_slot_machine") ||
			!ConfMan.getBool("deja_vu_rig_slot_machine")) {
		return scriptID;
	}

#ifdef MACVENTURE_TRACE_BUILD
	const_cast<MacVentureEngine *>(this)->traceRuntime("cheat.rig_slot_machine.queue", "requestedScript=%u forcedScript=%u",
		scriptID, kDejaVuSlotMachineScript);
#endif

	return kDejaVuSlotMachineScript;
}

int16 MacVentureEngine::adjustScriptResult(uint32 scriptID, ControlAction action, ObjID source, ObjID destination, int16 result) const {
	if (strcmp(_gameDescription->gameId, "deja_vu") ||
			scriptID != kDejaVuMoveValidationScript ||
			action != kMoveObject ||
			destination != kDejaVuTrenchCoatObject ||
			result != kDejaVuMoveCapacityFailure ||
			!ConfMan.hasKey("deja_vu_unlimited_inventory") ||
			!ConfMan.getBool("deja_vu_unlimited_inventory")) {
		return result;
	}

#ifdef MACVENTURE_TRACE_BUILD
	const_cast<MacVentureEngine *>(this)->traceRuntime("cheat.unlimited_inventory", "script=%u src=%u dest=%u result=%d forced=0",
		scriptID, source, destination, result);
#endif

	return 0;
}

uint MacVentureEngine::skipScriptCallStackPopCount(uint32 currentScriptID, int16 targetScriptID) const {
	if (strcmp(_gameDescription->gameId, "deja_vu") ||
			currentScriptID != kDejaVuGunScript ||
			targetScriptID != kDejaVuGunConsumeAmmoScript ||
			!ConfMan.hasKey("deja_vu_unlimited_ammo") ||
			!ConfMan.getBool("deja_vu_unlimited_ammo")) {
		return 0;
	}

#ifdef MACVENTURE_TRACE_BUILD
	const_cast<MacVentureEngine *>(this)->traceRuntime("cheat.unlimited_ammo", "script=%u skippedCall=%d popArgs=%u",
		currentScriptID, targetScriptID, kDejaVuGunConsumeAmmoStackArgs);
#endif

	return kDejaVuGunConsumeAmmoStackArgs;
}

// Private engine methods
void MacVentureEngine::processEvents() {
	Common::Event event;

	while (_eventMan->pollEvent(event)) {
		if (_gui->processEvent(event))
			continue;

		switch (event.type) {
		case Common::EVENT_QUIT:
		case Common::EVENT_RETURN_TO_LAUNCHER:
			_gameState = kGameStateQuitting;
			break;
		default:
			break;
		}
	}
}

bool MacVenture::MacVentureEngine::runScriptEngine() {
	debugC(3, kMVDebugMain, "Running script engine");
	if (_haltedAtEnd) {
		_haltedAtEnd = false;
		if (_scriptEngine->resume(false)) {
			_haltedAtEnd = true;
			return true;
		}
		return false;
	}

	if (_haltedInSelection) {
		_haltedInSelection = false;
		if (_scriptEngine->resume(false)) {
			_haltedInSelection = true;
			return true;
		}
		updateState(true);
	}

	while (!_currentSelection.empty()) {
		ObjID obj = _currentSelection.front();
		_currentSelection.remove_at(0);
		if (isGameRunning() && _world->isObjActive(obj)) {
			if (_scriptEngine->runControl(_selectedControl, obj, _destObject, _deltaPoint)) {
				_haltedInSelection = true;
				return true;
			}
			updateState(true);
		}
	}
	if (_selectedControl == 1) {
		_gameChanged = false;
	} else if (isGameRunning()) {
		if (_scriptEngine->runControl(kTick, _selectedControl, _destObject, _deltaPoint)) {
			_haltedAtEnd = true;
			return true;
		}
	}
	return false;
}

void MacVentureEngine::endGame() {
	requestQuit();
}

void MacVentureEngine::updateState(bool pause) {
	_prepared = false;
	runObjQueue();
	printTexts();
	playSounds(pause);
}

void MacVentureEngine::revert() {
	_gui->invertWindowColors(kMainGameWindow);
	preparedToRun();
}

void MacVentureEngine::runObjQueue() {
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("queue.object.run.begin", "size=%u", (uint)_objQueue.size());
#endif
	while (!_objQueue.empty()) {
		uint32 biggest = 0;
		uint32 index = 0;
		uint32 temp;
		for (uint i = 0; i < _objQueue.size(); i++) {
			temp = _objQueue[i].id;
			if (temp > biggest) {
				biggest = temp;
				index = i;
			}
		}
		QueuedObject obj = _objQueue[index];
		_objQueue.remove_at(index);
#ifdef MACVENTURE_TRACE_BUILD
		traceRuntime("queue.object.run.item", "type=%d obj=%u target=%u parent=%u pos=(%u,%u) remaining=%u",
			obj.id, obj.object, obj.target, obj.parent, obj.x, obj.y, (uint)_objQueue.size());
#endif
		switch (obj.id) {
		case 0x2:
			focusObjectWindow(obj.object);
			break;
		case 0x3:
			openObject(obj.object);
			break;
		case 0x4:
			closeObject(obj.object);
			break;
		case 0x7:
			checkObject(obj);
			break;
		case 0x8:
			reflectSwap(obj.object, obj.target);
			break;
		case 0xc:
			_world->setObjAttr(_gui->getWindowData(kMainGameWindow).refcon, kAttrContainerOpen, 0);
			_world->setObjAttr(_world->getObjAttr(1, kAttrParentObject), kAttrContainerOpen, 1);
			break;
		case 0xd:
			toggleExits();
			break;
		case 0xe:
			zoomObject(obj.object);
			break;
		default:
			break;
		}
	}
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("queue.object.run.end", "size=%u", (uint)_objQueue.size());
#endif
}

void MacVentureEngine::printTexts() {
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("queue.text.run.begin", "size=%u", (uint)_textQueue.size());
#endif
	while (!_textQueue.empty()) {
		if (_consoleRowsSincePause >= _gui->getConsoleVisibleRows()) {
			clickToContinue();
			return;
		}
		QueuedText text = _textQueue.front();
		_textQueue.remove_at(0);
		switch (text.id) {
		case kTextNumber:
#ifdef MACVENTURE_TRACE_BUILD
			traceRuntime("queue.text.run.item", "type=number value=%u source=%u target=%u", text.asset, text.source, text.destination);
#endif
			_currentConsoleText += Common::String::format("%d", text.asset);
			gameChanged();
			break;
		case kTextNewLine: {
#ifdef MACVENTURE_TRACE_BUILD
			traceRuntime("queue.text.run.item", "type=newline buffered=\"%s\"", escapeTraceString(_currentConsoleText).c_str());
#endif
			uint rows = _gui->getConsoleRowCount();
			if (_consoleRowsSincePause == 0)
				_consolePageStartRow = rows > 0 ? rows - 1 : 0;
			_gui->printText(_currentConsoleText);
			_consoleRowsSincePause += _gui->getConsoleRowCount() - rows;
			_currentConsoleText.clear();
			gameChanged();
			break;
		}
		case kTextPlain:
			{
				Common::String renderedText = _world->getText(text.asset, text.source, text.destination);
#ifdef MACVENTURE_TRACE_BUILD
				traceRuntime("queue.text.run.item", "type=plain text=%u source=%u target=%u rendered=\"%s\"",
					text.asset, text.source, text.destination, escapeTraceString(renderedText).c_str());
#endif
				_currentConsoleText += renderedText;
			}
			gameChanged();
			break;
		default:
			break;
		}
	}

	if (_consoleRowsSincePause > _gui->getConsoleVisibleRows())
		clickToContinue();
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("queue.text.run.end", "size=%u rowsSincePause=%u pageStart=%u rowCount=%u visibleRows=%u",
		(uint)_textQueue.size(), _consoleRowsSincePause, _consolePageStartRow,
		_gui->getConsoleRowCount(), _gui->getConsoleVisibleRows());
#endif
}

void MacVentureEngine::playSounds(bool pause) {
	int delay = 0;
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("queue.sound.run.begin", "size=%u pause=%d", (uint)_soundQueue.size(), pause ? 1 : 0);
#endif
	while (!_soundQueue.empty()) {
		QueuedSound item = _soundQueue.front();
		_soundQueue.remove_at(0);
#ifdef MACVENTURE_TRACE_BUILD
		traceRuntime("queue.sound.run.item", "type=%d sound=%u remaining=%u", item.id, item.reference, (uint)_soundQueue.size());
#endif
		switch (item.id) {
		case kSoundPlay:
			_soundManager->playSound(item.reference);
			break;
		case kSoundPlayAndWait:
			delay = _soundManager->playSound(item.reference);
			break;
		case kSoundWait:
			// Empty in the original.
			break;
		default:
			break;
		}
	}
	if (pause && delay > 0) {
		warning("Sound pausing not yet tested. Pausing for %d", delay);
		g_system->delayMillis(delay);
		preparedToRun();
	}
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("queue.sound.run.end", "size=%u delay=%d", (uint)_soundQueue.size(), delay);
#endif
}

Item MacVentureEngine::removeOutlier(Layout &layout, bool flag, Common::Rect rect) {
	int max = flag ? 0x7fff : -0x8000;
	bool first = true;
	int outlier = -1;

	for (int i = 0; i < (int)layout.size(); i++) {
		Common::Rect childBounds = layout.at(i).bounds;
		bool oob = (childBounds.bottom > rect.bottom || childBounds.top > rect.top);
		if (flag)
			oob = !oob;
		if (first && oob) {
			first = false;
			max = flag ? 0x7fff : -0x8000;
		}
		if (first || oob) {
			int center = childBounds.width() / 2;
			bool over = false;
			if (flag) {
				over = (max >= center);
			} else {
				over = (max <= center);
			}
			if (over) {
				outlier = i;
				max = center;
			}
		}
	}

	return layout.remove(outlier);
}

void MacVentureEngine::cleanUp(WindowReference reference) {
	const WindowData &data = _gui->getWindowData(reference);
	Common::Rect innerDims = _gui->findWindow(reference)->getInnerDimensions();
	Common::Rect windowBounds(0, 0, innerDims.width(), innerDims.height());
	Common::Array<Item> items;

	Layout onScreen, offScreen;
	Layout line, overflow;

	for (int i = data.children.size() - 1; i >= 0; i--) {
		DrawableObject child = data.children[i];
		Common::Rect childBounds = getObjBounds(child.obj);
		if (childBounds.bottom > windowBounds.bottom || childBounds.top < windowBounds.top) {
			offScreen.append(Item{child.obj, childBounds});
		} else if (16 + childBounds.width() > windowBounds.width()) {
			offScreen.append(Item{child.obj, childBounds});
		} else {
			onScreen.append(Item{child.obj, childBounds});
		}
	}

	int y = windowBounds.top + 8;

	while (onScreen.size() || offScreen.size()) {
		int min = 0x7fff;
		int minIdx = -1;
		int height = 0;

		// Find highest element onscreen
		for (int i = onScreen.size() - 1; i >= 0; i--) {
			Item child = onScreen.at(i);
			if (child.bounds.top < min) {
				min = child.bounds.top;
				height = child.bounds.height();
				minIdx = i;
			}
		}

		if (minIdx != -1) {
			// Remove it and put it on line
			line.append(onScreen.remove(minIdx));
			// along with all elements in same line
			bool done;
			do {
				done = true;
				for (int i = onScreen.size() - 1; i >= 0; i--) {
					Item child = onScreen.at(i);
					if (child.bounds.top < min + height) {
						if (height < child.bounds.height()) {
							done = false;
							height = child.bounds.height();
						}
						line.append(onScreen.remove(i));
					}
				}
			} while (!done);
		}
		// Line is too long? Put items back onscreen
		while (line.size() && line.width() > windowBounds.width()) {
			onScreen.append(removeOutlier(line, false, windowBounds));
		}
		// Find line height
		height = 0;
		for (int i = line.size() - 1; i >= 0; i--) {
			Item child = line.at(i);
			if (height < child.bounds.height())
				height = child.bounds.height();
		}
		// While there's room, add offscreen items
		while (offScreen.size() && line.width() < windowBounds.width()) {
			Item outlier = removeOutlier(offScreen, true, windowBounds);

			if (onScreen.size() && outlier.bounds.height() > height) {
				overflow.append(outlier);
			} else if (line.width() + 8 + outlier.bounds.width() <= windowBounds.width()) {
				// Adjust line height
				if (height < outlier.bounds.height())
					height = outlier.bounds.height();
				line.append(outlier);
			} else {
				overflow.append(outlier);
			}
		}
		// Move all overflow back offscreen
		while (overflow.size()) {
			offScreen.append(overflow.remove(0));
		}
		// Is line empty? Put one offscreen item on there
		if (!line.size() && offScreen.size()) {
			Item offscreenItem = offScreen.remove(0);

			if (height < offscreenItem.bounds.height())
				height = offscreenItem.bounds.height();
			line.append(offscreenItem);
		}
		int x = windowBounds.left + 8;
		// Now add line to new positions
		while (line.size()) {
			Item outlier = removeOutlier(line, true, windowBounds);

			Item toAdd;
			toAdd.id = outlier.id;
			toAdd.bounds = Common::Rect(Common::Point(x, y + (height - outlier.bounds.height()) / 2),
										outlier.bounds.width(), outlier.bounds.height());
			items.push_back(toAdd);

			x += outlier.bounds.width() + 8;
		}

		y += height + 8;
	}

	moveItems(items, reference);
}

void MacVentureEngine::messUp(WindowReference reference) {
	const WindowData &data = _gui->getWindowData(reference);
	Common::Array<Item> items;

	for (auto &child : data.children) {
		Common::Point childMeasures = _gui->getObjMeasures(child.obj);
		int scale = data.bounds.height() - childMeasures.y;
		if (scale < 0)
			scale = 0;
		float f = randBetween(0, 10) / 10.0f;
		int y = (int)(f * scale) + data.bounds.top;

		scale = data.bounds.width() - childMeasures.x;
		if (scale < 0)
			scale = 0;
		f = randBetween(0, 10) / 10.0f;
		int x = (int)(f * scale) + data.bounds.left;

		items.push_back(Item{child.obj, Common::Rect(Common::Point(x, y), childMeasures.x, childMeasures.y)});
	}

	moveItems(items, reference);
}

void MacVentureEngine::moveItems(Common::Array<Item> &items, WindowReference reference) {
	for (auto &item : items) {
		Common::Point pt = _gui->getObjMeasures(item.id);
		if (pt.y != item.bounds.top || pt.x != item.bounds.left) {
			_world->setObjAttr(item.id, kAttrPosX, item.bounds.left);
			_world->setObjAttr(item.id, kAttrPosY, item.bounds.top);
		}
	}

	updateWindow(reference);
}

void MacVentureEngine::updateControls() {
	selectControl(kNoCommand);
	_gui->clearControls();
	toggleExits();
	resetVars();
}

void MacVentureEngine::resetVars() {
	selectControl(kNoCommand);
	_currentSelection.clear();
	_destObject = 0;
	setDeltaPoint(Common::Point(0, 0));
	_cmdReady = false;
}

void MacVentureEngine::unselectAll() {
	while (!_currentSelection.empty()) {
		unselectObject(_currentSelection.front());
	}
}

void MacVentureEngine::selectObject(ObjID objID) {
	if (!_currentSelection.empty()) {
		if (findParentWindow(objID) != findParentWindow(_currentSelection[0])) {
			// TODO: Needs further testing, but it doesn't seem necessary.
			//unselectAll();
		}
	}
	if (findObjectInArray(objID, _currentSelection) == -1) {
		_currentSelection.push_back(objID);
	}
	if (findObjectInArray(objID, _selectedObjs) == -1) {
		_selectedObjs.push_back(objID);
		highlightExit(objID);
	}
}

void MacVentureEngine::unselectObject(ObjID objID) {
	int idx = findObjectInArray(objID, _currentSelection);
	if (idx != -1) {
		_currentSelection.remove_at(idx);
	}
	if ((idx = findObjectInArray(objID, _selectedObjs)) != -1) {
		_selectedObjs.remove_at(idx);
		highlightExit(objID);
	}
}


void MacVentureEngine::updateExits() {
	_gui->clearExits();
	_gui->unselectExits();

	Common::Array<ObjID> exits = _world->getChildren(_world->getObjAttr(1, kAttrParentObject), true);
	for (uint i = 0; i < exits.size(); i++)
		_gui->updateExit(exits[i]);

	_gui->resetExitBackgroundPattern();
}

int MacVentureEngine::findObjectInArray(ObjID objID, const Common::Array<ObjID> &list) {
	// Find the object in the current selection
	bool found = false;
	uint i = 0;
	while (i < list.size() && !found) {
		if (list[i] == objID) {
			found = true;
		} else {
			i++;
		}
	}
	// HACK, should use iterator
	return found ? (int)i : -1;
}

uint MacVentureEngine::getPrefixNdx(ObjID obj) {
	return _world->getObjAttr(obj, kAttrPrefixes);
}

Common::String MacVentureEngine::getPrefixString(uint flag, ObjID obj) {
	uint ndx = getPrefixNdx(obj);
	ndx = ((ndx) >> flag) & 3;
	return _decodingNamingArticles->getString(ndx);
}

Common::String MacVentureEngine::getNoun(ObjID ndx) {
	return _decodingIndirectArticles->getString(ndx);
}

Common::String MacVentureEngine::getConsoleText() const {
	Common::String consoleText = _gui->getConsoleText();
	if (consoleText.size() > kMaxConsoleTextLength) {
		consoleText = consoleText.substr(consoleText.size() - kMaxConsoleTextLength);
	}
	return consoleText;
}

void MacVentureEngine::setConsoleText(const Common::String &text) {
	_gui->setConsoleText(text);
}

void MacVentureEngine::markConsoleTextRestored() {
	const uint rowCount = _gui->getConsoleRowCount();

	_consoleRowsSincePause = 0;
	_consolePageStartRow = rowCount;
	_clickToContinue = false;
	_enginePaused = false;
	_gui->scrollConsoleToRow(rowCount);

#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("console.restore", "rowCount=%u visibleRows=%u pageStart=%u rowsSincePause=%u",
		rowCount, _gui->getConsoleVisibleRows(), _consolePageStartRow, _consoleRowsSincePause);
#endif
}

void MacVentureEngine::highlightExit(ObjID objID) {
	_gui->highlightExitButton(objID);
}

void MacVentureEngine::selectPrimaryObject(ObjID objID) {
	if (objID == _destObject) {
		return;
	}
	int idx;
	debugC(4, kMVDebugMain, "Select primary object (%d)", objID);
	if (_destObject > 0 &&
		(idx = findObjectInArray(_destObject, _selectedObjs)) != -1 &&
		findObjectInArray(_destObject, _currentSelection) == -1) {
		_selectedObjs.remove_at(idx);
		highlightExit(_destObject);
	}
	_destObject = objID;
	if (findObjectInArray(_destObject, _selectedObjs) == -1) {
		_selectedObjs.push_back(_destObject);
		highlightExit(_destObject);
	}

	_cmdReady = true;
}

void MacVentureEngine::focusObjectWindow(ObjID objID) {
	if (objID) {
		WindowReference win = getObjWindow(objID);
		if (win)
			_gui->bringToFront(win);
	}
}

void MacVentureEngine::openObject(ObjID objID) {
	debugC(3, kMVDebugMain, "Open Object[%d] parent[%d] x[%d] y[%d]",
		objID,
		_world->getObjAttr(objID, kAttrParentObject),
		_world->getObjAttr(objID, kAttrPosX),
		_world->getObjAttr(objID, kAttrPosY));

	if (getObjWindow(objID)) {
		return;
	}
	if (objID == _world->getObjAttr(1, kAttrParentObject)) {
		_gui->updateWindowInfo(kMainGameWindow, objID, _world->getChildren(objID, true));
		_gui->updateWindow(kMainGameWindow, _world->getObjAttr(objID, kAttrContainerOpen));
		updateExits();
		_gui->setWindowTitle(kMainGameWindow, capitalize(_world->getText(objID, objID, objID))); // it ignores source and target in the original
	} else { // Open inventory window
		Common::Point p(_world->getObjAttr(objID, kAttrPosX), _world->getObjAttr(objID, kAttrPosY));
		WindowReference invID = _gui->createInventoryWindow(objID);
		Common::String title = _world->getText(objID, objID, objID);
		_gui->setWindowTitle(invID, title);
		_gui->updateWindowInfo(invID, objID, _world->getChildren(objID, true));
		_gui->updateWindow(invID, _world->getObjAttr(objID, kAttrContainerOpen));
	}
}

void MacVentureEngine::closeObject(ObjID objID) {
	warning("closeObject: not fully implemented");
	_gui->tryCloseWindow(getObjWindow(objID));
}

void MacVentureEngine::checkObject(QueuedObject old) {
	bool hasChanged = false;
	debugC(3, kMVDebugMain, "Check Object[%d] parent[%d] x[%d] y[%d]",
		old.object,
		old.parent,
		old.x,
		old.y);
	ObjID id = old.object;
	if (id == 1) {
		if (old.parent != _world->getObjAttr(id, kAttrParentObject)) {
			enqueueObject(kSetToPlayerParent, id);
		}
		if (old.offscreen != !!_world->getObjAttr(id, kAttrInvisible) ||
			old.invisible != !!_world->getObjAttr(id, kAttrUnclickable)) {
			updateWindow(findParentWindow(id));
		}
	} else if (old.parent != _world->getObjAttr(id, kAttrParentObject) ||
				old.x != _world->getObjAttr(id, kAttrPosX) ||
				old.y != _world->getObjAttr(id, kAttrPosY)) {
		WindowReference oldWin = getObjWindow(old.parent);
		if (oldWin) {
			_gui->removeChild(oldWin, id);
			hasChanged = true;
		}

		WindowReference newWin = findParentWindow(id);
		if (newWin) {
			_gui->addChild(newWin, id);
			hasChanged = true;
		}
	} else if (old.offscreen != !!_world->getObjAttr(id, kAttrInvisible) ||
				old.invisible != !!_world->getObjAttr(id, kAttrUnclickable)) {
		updateWindow(findParentWindow(id));
	}

	if (_world->getObjAttr(id, kAttrIsExit)) {
		if (hasChanged ||
			old.hidden != !!_world->getObjAttr(id, kAttrHiddenExit) ||
			old.exitx != _world->getObjAttr(id, kAttrExitX) ||
			old.exity != _world->getObjAttr(id, kAttrExitY))
			_gui->updateExit(id);
	}
	WindowReference win = getObjWindow(id);
	ObjID cur = id;
	ObjID root = _world->getObjAttr(1, kAttrParentObject);
	while (cur != root)	{
		if (cur == 0 || !_world->getObjAttr(cur, kAttrContainerOpen)) {
			break;
		}
		cur = _world->getObjAttr(cur, kAttrParentObject);
	}
	if (cur == root) {
		if (win) {
			return;
		}
		enqueueObject(kOpenWindow, id); //open
	} else {
		if (!win) {
			return;
		}
		enqueueObject(kCloseWindow, id); //close
	}

	// Update children
	Common::Array<ObjID> children = _world->getChildren(id, true);
	for (uint i = 0; i < children.size(); i++) {
		enqueueObject(kUpdateObject, children[i]);
	}
}

void MacVentureEngine::reflectSwap(ObjID fromID, ObjID toID) {
	WindowReference from = getObjWindow(fromID);
	WindowReference to = getObjWindow(toID);
	WindowReference tmp = to;
	debugC(3, kMVDebugMain, "Swap Object[%d] to Object[%d], from win[%d] to win[%d] ",
		fromID, toID, from, to);

	if (!to) {
		tmp = from;
	}
	if (tmp) {
		Common::String newTitle = _world->getText(toID, 0, 0); // Ignores src and targ in the original
		_gui->setWindowTitle(tmp, newTitle);
		_gui->updateWindowInfo(tmp, toID, _world->getChildren(toID, true));
		updateWindow(tmp);
	}
}

void MacVentureEngine::toggleExits() {
	while (!_selectedObjs.empty()) {
		ObjID obj = _selectedObjs.back();
		_selectedObjs.pop_back();
		highlightExit(obj);
		updateWindow(findParentWindow(obj));
	}
}

void MacVentureEngine::zoomObject(ObjID objID) {
	warning("zoomObject: unimplemented");
}

bool MacVentureEngine::isObjEnqueued(ObjID objID) {
	Common::Array<QueuedObject>::const_iterator it;
	for (it = _objQueue.begin(); it != _objQueue.end(); it++) {
		if (it->id == kUpdateObject && it->object == objID) {
			return true;
		}
	}
	return false;
}

bool MacVentureEngine::isGameRunning() {
	return (_gameState == kGameStateInit || _gameState == kGameStatePlaying);
}

ControlAction MacVenture::MacVentureEngine::referenceToAction(ControlType id) {
	switch (id) {
	case MacVenture::kControlExitBox:
		return kActivateObject;//?? Like this in the original
	case MacVenture::kControlExamine:
		return kExamine;
	case MacVenture::kControlOpen:
		return kOpen;
	case MacVenture::kControlClose:
		return kClose;
	case MacVenture::kControlSpeak:
		return kSpeak;
	case MacVenture::kControlOperate:
		return kOperate;
	case MacVenture::kControlGo:
		return kGo;
	case MacVenture::kControlHit:
		return kHit;
	case MacVenture::kControlConsume:
		return kConsume;
	default:
		return kNoCommand;
	}
}

// Data retrieval

bool MacVentureEngine::isPaused() {
	return _paused;
}

bool MacVentureEngine::needsClickToContinue() {
	return _clickToContinue;
}

Common::String MacVentureEngine::getCommandsPausedString() const {
	return Common::String("Click to continue");
}

Common::Path MacVentureEngine::getFilePath(FilePathID id) const {
	Common::Path path(_filenames->getString(id));

	if (!strcmp(_gameDescription->gameId, "deja_vu") && !Common::File::exists(path)) {
		switch (id) {
		case kTitlePathID:
			path = Common::Path("Deja Title");
			break;
		case kSubdirPathID:
			path = Common::Path("Deja Vu 2");
			break;
		case kObjectPathID:
			path = Common::Path("Deja Object");
			break;
		case kFilterPathID:
			path = Common::Path("Deja Filter");
			break;
		case kTextPathID:
			path = Common::Path("Deja Text");
			break;
		case kGraphicPathID:
			path = Common::Path("Deja Graphic");
			break;
		case kSoundPathID:
			path = Common::Path("Deja Sound");
			break;
		default:
			break;
		}
	}

	return path;
}

bool MacVentureEngine::isOldText() const {
	return _oldTextEncoding;
}

const HuffmanLists *MacVentureEngine::getDecodingHuffman() const {
	return _textHuffman;
}

uint32 MacVentureEngine::randBetween(uint32 min, uint32 max) {
	uint32 value = _rnd->getRandomNumber(max - min) + min;
#ifdef MACVENTURE_TRACE_BUILD
	traceRuntime("engine.random", "min=%u max=%u result=%u", min, max, value);
#endif
	return value;
}

uint32 MacVentureEngine::getInvolvedObjects() {
	// If there is no valid control selected, we return a number too big
	// to be useful. There is no control that uses that many objects.
	return (_selectedControl ? getGlobalSettings()._cmdArgCnts[_selectedControl - 1] : 3000);
}

Common::Point MacVentureEngine::getObjPosition(ObjID objID) {
	return Common::Point(_world->getObjAttr(objID, kAttrPosX), _world->getObjAttr(objID, kAttrPosY));
}

bool MacVentureEngine::isObjVisible(ObjID objID) {
	return _world->getObjAttr(objID, kAttrInvisible) == 0;
}

bool MacVentureEngine::isObjClickable(ObjID objID) {
	return _world->getObjAttr(objID, kAttrUnclickable) == 0;
}

bool MacVentureEngine::isObjDraggable(ObjID objID) {
	return _world->isObjDraggable(objID);
}

bool MacVentureEngine::isObjSelected(ObjID objID) {
	int idx = findObjectInArray(objID, _selectedObjs);
	return idx != -1;
}

bool MacVentureEngine::isObjExit(ObjID objID) {
	return _world->getObjAttr(objID, kAttrIsExit);
}

bool MacVentureEngine::isHiddenExit(ObjID objID) {
	return _world->getObjAttr(objID, kAttrHiddenExit);
}

Common::Point MacVentureEngine::getObjExitPosition(ObjID objID) {
	uint x = _world->getObjAttr(objID, kAttrExitX);
	uint y = _world->getObjAttr(objID, kAttrExitY);
	return Common::Point(x, y);
}

ObjID MacVentureEngine::getParent(ObjID objID) {
	return _world->getObjAttr(objID, kAttrParentObject);
}

Common::Rect MacVentureEngine::getObjBounds(ObjID objID) {
	Common::Point pos = getObjPosition(objID);

	Common::Point measures = _gui->getObjMeasures(objID);
	uint w = measures.x;
	uint h = measures.y;
	return Common::Rect(pos.x, pos.y, pos.x + w, pos.y + h);
}

uint MacVentureEngine::getOverlapPercent(ObjID one, ObjID other) {
	// If it's not the same parent, there's 0 overlap
	if (_world->getObjAttr(one, kAttrParentObject) !=
		_world->getObjAttr(other, kAttrParentObject))
		return 0;

	Common::Rect oneBounds = getObjBounds(one);
	Common::Rect otherBounds = getObjBounds(other);
	if (otherBounds.intersects(oneBounds) ||
		oneBounds.intersects(otherBounds)) {
		uint areaOne = oneBounds.width() * oneBounds.height();
		uint areaOther = otherBounds.width() * otherBounds.height();
		return (areaOne != 0) ? (areaOther * 100 / areaOne) : 0;
	}
	return 0;
}

WindowReference MacVentureEngine::getObjWindow(ObjID objID) {
	return _gui->getObjWindow(objID);
}

WindowReference MacVentureEngine::findParentWindow(ObjID objID) {
	if (objID == 1) {
		return kSelfWindow;
	}
	ObjID parent = _world->getObjAttr(objID, kAttrParentObject);
	if (parent == 0) {
		return kNoWindow;
	}
	return getObjWindow(parent);
}

Common::Point MacVentureEngine::getDeltaPoint() {
	return _deltaPoint;
}

ObjID MacVentureEngine::getDestObject() {
	return _destObject;
}

ControlAction MacVentureEngine::getSelectedControl() {
	return _selectedControl;
}

#ifdef MACVENTURE_TRACE_BUILD
void MacVentureEngine::traceRuntime(const char *event, const char *fmt, ...) {
	static Common::DumpFile logFile;
	static bool triedOpen = false;
	static bool didOpen = false;
	static uint traceSeq = 0;

	if (!triedOpen) {
		triedOpen = true;
		didOpen = logFile.open(Common::Path("macventure-trace.log", Common::Path::kNoSeparator));
		if (!didOpen) {
			warning("MACVENTURE_TRACE unable to open macventure-trace.log");
		} else {
			warning("MACVENTURE_TRACE opened macventure-trace.log");
		}
	}

	va_list va;
	va_start(va, fmt);
	Common::String detail = Common::String::vformat(fmt, va);
	va_end(va);

	Common::String line = Common::String::format(
		"MACVENTURE_TRACE %06u ms=%u event=%s state=%d selected=%d dest=%u delta=(%d,%d) detail=\"%s\"\n",
		traceSeq++, g_system ? g_system->getMillis() : 0, event, _gameState, _selectedControl,
		_destObject, _deltaPoint.x, _deltaPoint.y, escapeTraceString(detail).c_str());

	if (didOpen) {
		logFile.write(line.c_str(), line.size());
		logFile.flush();
	}

	warning("%s", line.c_str());
}
#endif

// Data loading

bool MacVentureEngine::loadGlobalSettings() {
	Common::MacResIDArray resArray;

	if ((resArray = _resourceManager->getResIDArray(MKTAG('G', 'N', 'R', 'L'))).size() == 0)
		return false;

	Common::SeekableReadStream *res;
	res = _resourceManager->getResource(MKTAG('G', 'N', 'R', 'L'), kGlobalSettingsID);
	if (res) {
		_globalSettings = new GlobalSettings();
		_globalSettings->loadSettings(res);
		delete res;
		return true;
	}
	return false;
}

bool MacVentureEngine::loadTextHuffman() {
	Common::MacResIDArray resArray;
	Common::SeekableReadStream *res;

	if ((resArray = _resourceManager->getResIDArray(MKTAG('G', 'N', 'R', 'L'))).size() == 0)
		return false;

	res = _resourceManager->getResource(MKTAG('G', 'N', 'R', 'L'), kTextHuffmanTableID);
	if (res) {
		uint32 numEntries = res->readUint16BE();
		res->readUint16BE(); // Skip

		uint32 *masks = new uint32[numEntries];
		for (uint i = 0; i < numEntries - 1; i++) {
			// For some reason there are one lass mask than entries
			masks[i] = res->readUint16BE();
		}
		// make sure array is fully initialized
		masks[numEntries - 1] = 0x10000;
		// by setting the 'last' enttry to 0x10000 (max 16 bit integer + 1) we make sure that the 
		// iteration in TextAsset::decodeHuffmann never fails. This iteration will search for a
		// 16 bit value < mask[i]. If array is not properly set up, we either get a random value for 
		// mask[numEntries - 1] (0 on optimized code). Depending on value, NO entry is found in 
		// huffman table resulting in an out of bounds index. That will ultimately result in assert 
		// failure when accessing an element in _textHuffman.

		uint32 *lengths = new uint32[numEntries];
		for (uint i = 0; i < numEntries; i++) {
			lengths[i] = res->readByte();
		}

		uint32 *values = new uint32[numEntries];
		for (uint i = 0; i < numEntries; i++) {
			values[i] = res->readByte();
		}

		_textHuffman = new HuffmanLists(numEntries, lengths, masks, values);
		debugC(4, kMVDebugMain, "Text is huffman-encoded");

		delete res;
		delete[] masks;
		delete[] lengths;
		delete[] values;
		return true;
	}
	return false;
}

Common::String MacVentureEngine::capitalize(const Common::String &str) const {
	Common::String out(str);
	bool shouldCapitalize = true;

	for (char &c : out) {
		if (shouldCapitalize) {
			c = toupper(c);
			shouldCapitalize = false;
		} else {
			if (c == ' ')
				shouldCapitalize = true;
		}
	}

	return out;
}

// Global Settings
GlobalSettings::GlobalSettings() {
}

GlobalSettings::~GlobalSettings() {

}

void GlobalSettings::loadSettings(Common::SeekableReadStream *dataStream) {
	_numObjects = dataStream->readUint16BE();
	_numGlobals = dataStream->readUint16BE();
	_numCommands = dataStream->readUint16BE();
	_numAttributes = dataStream->readUint16BE();
	_numGroups = dataStream->readUint16BE();
	dataStream->readUint16BE(); // unknown
	_invTop = dataStream->readUint16BE();
	_invLeft = dataStream->readUint16BE();
	_invHeight = dataStream->readUint16BE();
	_invWidth = dataStream->readUint16BE();
	_invOffsetY = dataStream->readUint16BE();
	_invOffsetX = dataStream->readSint16BE();
	_defaultFont = dataStream->readUint16BE();
	_defaultSize = dataStream->readUint16BE();

	uint8 *attrIndices = new uint8[_numAttributes];
	dataStream->read(attrIndices, _numAttributes);
	_attrIndices = Common::Array<uint8>(attrIndices, _numAttributes);
	delete[] attrIndices;

	for (int i = 0; i < _numAttributes; i++) {
		_attrMasks.push_back(dataStream->readUint16BE());
	}

	uint8 *attrShifts = new uint8[_numAttributes];
	dataStream->read(attrShifts, _numAttributes);
	_attrShifts = Common::Array<uint8>(attrShifts, _numAttributes);
	delete[] attrShifts;

	uint8 *cmdArgCnts = new uint8[_numCommands];
	dataStream->read(cmdArgCnts, _numCommands);
	_cmdArgCnts = Common::Array<uint8>(cmdArgCnts, _numCommands);
	delete[] cmdArgCnts;

	uint8 *commands = new uint8[_numCommands];
	dataStream->read(commands, _numCommands);
	_commands = Common::Array<uint8>(commands, _numCommands);
	delete[] commands;
}

} // End of namespace MacVenture
