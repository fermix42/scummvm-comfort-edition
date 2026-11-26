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
#include "mohawk/cursors.h"

#include "common/events.h"
#include "common/system.h"
#include "common/textconsole.h"

namespace Mohawk {

void LBItem::appendDiagnosticState(Common::JSONObject &state) const {
	using namespace Common;
	state["resource"] = new JSONValue((long long)_resourceId);
	state["local_enabled"] = new JSONValue(_enabled);
	state["global_enabled"] = new JSONValue(_globalEnabled);
	state["global_visible"] = new JSONValue(_globalVisible);
	state["item_phase"] = new JSONValue((long long)_phase);
	state["timing_mode"] = new JSONValue((long long)_timingMode);
	state["control_mode"] = new JSONValue((long long)_controlMode);
	state["next_time"] = new JSONValue((long long)_nextTime);
	state["start_time"] = new JSONValue((long long)_startTime);
	state["loops"] = new JSONValue((long long)_loops);
	state["parent"] = new JSONValue((long long)(_parent ? _parent->_itemId : 0));
	state["script_entries"] = new JSONValue((long long)_scriptEntries.size());
	state["drag_rearmed_during_press"] = new JSONValue(_dragRearmedDuringPress);
}

LBItem::LBItem(MohawkEngine_LivingBooks *vm, LBPage *page, Common::Rect rect) : _vm(vm), _page(page), _rect(rect) {
	_parent = nullptr;
	_dragConfigured = _dragging = _rolloverEnabled = _rolloverUseHitTest = false;
	_tracking = _trackingInside = false;
	_dragRearmedDuringPress = false;
	_dragConstraintItem = 0;
	if (_vm->getGameType() == GType_LIVINGBOOKSV1 || _vm->getGameType() == GType_LIVINGBOOKSV2)
		_phase = kLBPhaseInit;
	else
		_phase = kLBPhaseLoad;

	_loopMode = 0;
	_delayMin = 0;
	_delayMax = 0;
	_timingMode = kLBAutoNone;
	_periodMin = 0;
	_periodMax = 0;
	_controlMode = kLBControlNone;
	_soundMode = 0;

	_loaded = false;
	_enabled = false;
	_visibility = 0;
	_playing = false;
	_globalEnabled = true;
	_globalVisible = true;
	_nextTime = 0;
	_startTime = 0;
	_loops = 0;

	_isAmbient = false;
	_doHitTest = true;
}

LBItem::~LBItem() {
	_vm->clearRollover(this, false);
	_vm->removeKeyEvents(this);
	setParent(nullptr);
	while (!_children.empty())
		_children.back()->setParent(nullptr);
	for (uint i = 0; i < _scriptEntries.size(); i++)
		delete _scriptEntries[i];
}

void LBItem::destroySelf() {
	if (!this->_itemId)
		error("destroySelf() on an item which was already dead");

	_vm->queueDelayedEvent(DelayedEvent(this, kLBDelayedEventDestroy));

	_itemId = 0;
}

void LBItem::setEnabled(bool enabled) {
	const bool wasEnabled = _loaded && isEnabled();
	if (enabled && !_loaded && !_playing) {
		if (_timingMode == kLBAutoUserIdle) {
			setNextTime(_periodMin, _periodMax);
			debug(2, "Enable time startup");
		}
	}

	_enabled = enabled;
	if (wasEnabled != (_loaded && isEnabled()))
		enabledChanged(!wasEnabled);
}

void LBItem::setGlobalEnabled(bool enabled) {
	const bool wasEnabled = _loaded && isEnabled();
	_globalEnabled = enabled;
	// A global pause must preserve the item's authored/local enabled state.
	if (wasEnabled != (_loaded && isEnabled()))
		enabledChanged(!wasEnabled);
}

bool LBItem::contains(Common::Point point) {
	if (!_loaded)
		return false;

	if (_playing && _loopMode == 0xFFFF)
		stop();

	if (!_playing && _timingMode == kLBAutoUserIdle)
		setNextTime(_periodMin, _periodMax);

	return isVisible() && _rect.contains(point);
}

void LBItem::update() {
	if (_phase != kLBPhaseNone && (!_loaded || !_enabled || !_globalEnabled))
		return;

	if (_nextTime == 0 || _nextTime > getTime())
		return;

	if (togglePlaying(_playing, true)) {
		_nextTime = 0;
	} else if (_loops == 0 && _timingMode == kLBAutoUserIdle) {
		debug(9, "Looping in update()");
		setNextTime(_periodMin, _periodMax);
	}
}

void LBItem::handleMouseDown(Common::Point pos) {
	if (!_loaded || !_enabled || !_globalEnabled)
		return;

	_vm->setFocus(this);
	_dragRearmedDuringPress = false;
	runScript(kLBEventMouseDown);
	_tracking = _trackingInside = true;
	runScript(kLBEventMouseTrackIn);
}

void LBItem::handleMouseMove(Common::Point pos) {
	if (!_tracking)
		return;
	bool inside = contains(pos);
	if (inside == _trackingInside) {
		runScript(kLBEventMouseTrackMove);
		runScript(inside ? kLBEventMouseTrackMoveIn : kLBEventMouseTrackMoveOut);
	} else {
		_trackingInside = inside;
		runScript(inside ? kLBEventMouseTrackIn : kLBEventMouseTrackOut);
	}
}

void LBItem::handleMouseUp(Common::Point pos) {
	const bool rearmedHover = _dragRearmedDuringPress;
	_tracking = false;
	_dragRearmedDuringPress = false;
	_vm->setFocus(nullptr);
	runScript(kLBEventMouseUp);
	// A newly restored hover pointer is not the hidden item belonging to the
	// original press. Preserve that press's outside-release cleanup even if
	// the replacement pointer has now been positioned under the mouse.
	runScript(!rearmedHover && contains(pos) ? kLBEventMouseUpIn : kLBEventMouseUpOut);
}

void LBItem::setDragParams(Common::Point checkOffset, const Common::Rect &bounds) {
	_dragCheckOffset = checkOffset;
	_dragBounds = bounds;
	_dragConstraintItem = 0;
	_dragConfigured = true;
}

void LBItem::beginDrag(Common::Point point) {
	if (!_dragConfigured || _dragging)
		return;
	if (_tracking && !isVisible())
		_dragRearmedDuringPress = true;
	_dragOffset = point - Common::Point(_rect.left, _rect.top);
	_dragging = true;
	runScript(kLBEventDragStart);
	// Rollover scripts can start a synthetic pointer drag after this mouse
	// event's normal drag update. Position it now so an immediate click hits
	// the pointer item, rather than the background at its old position.
	updateMouse(_vm->_system->getEventManager()->getMousePos());
}

void LBItem::endDrag() {
	if (!_dragging)
		return;
	_dragging = false;
	runScript(kLBEventDragEnd);
}

bool LBItem::dragAllows(Common::Point point) {
	point += _dragCheckOffset;
	if (_dragConstraintItem) {
		LBItem *constraint = _vm->getItemById(_dragConstraintItem);
		return constraint && constraint->contains(point);
	}
	// Rectangle drag limits include their right/bottom edges. Item hit tests
	// and the motion component use half-open rectangles instead.
	return point.x >= _dragBounds.left && point.x <= _dragBounds.right
		&& point.y >= _dragBounds.top && point.y <= _dragBounds.bottom;
}

void LBItem::updateMouse(Common::Point point) {
	if (!_loaded || !_enabled || !_globalEnabled)
		return;
	if (_dragging) {
		Common::Point position(_rect.left, _rect.top);
		Common::Point wanted = point - _dragOffset;
		// Test each step: an item constraint can have holes or narrow passages.
		bool moved;
		do {
			moved = false;
			if (position.x != wanted.x) {
				Common::Point next(position.x + (position.x < wanted.x ? 1 : -1), position.y);
				if (dragAllows(next)) { position = next; moved = true; }
			}
			if (position.y != wanted.y) {
				Common::Point next(position.x, position.y + (position.y < wanted.y ? 1 : -1));
				if (dragAllows(next)) { position = next; moved = true; }
			}
		} while (moved);
		if (position != Common::Point(_rect.left, _rect.top)) {
			moveTo(position);
			runScript(kLBEventDragMove);
		}
	}
}

bool LBItem::rolloverContains(Common::Point point) {
	if (!_rolloverEnabled || !_loaded || !isVisible() || !_rect.contains(point))
		return false;
	return !_rolloverUseHitTest || contains(point);
}

bool LBItem::togglePlaying(bool playing, bool restart) {
	if (playing) {
		_vm->queueDelayedEvent(DelayedEvent(this, kLBDelayedEventDone));
		return true;
	}
	if (((_loaded && _enabled && _globalEnabled) || _phase == kLBPhaseNone) && !_playing) {
		_playing = togglePlaying(true, restart);
		if (_playing) {
			_nextTime = 0;
			_startTime = getTime();

			if (_loopMode == 0xFFFF || _loopMode == 0xFFFE)
				_loops = 0xFFFF;
			else
				_loops = _loopMode;

			if (_controlMode >= kLBControlHideMouse) {
				debug(2, "Hiding cursor");
				_vm->_cursor->hideCursor();
				_vm->lockSound(this, true);

				if (_controlMode >= kLBControlPauseItems) {
					debug(2, "Disabling all");
					_vm->setEnableForAll(false, this);
				}
			}

			runScript(kLBEventStarted);
			notify(0, _itemId);
		}
	}
	return _playing;
}

void LBItem::done(bool onlyNotify) {
	if (onlyNotify) {
		if (_relocPoint.x || _relocPoint.y) {
			_rect.translate(_relocPoint.x, _relocPoint.y);
			// TODO: does drag box need adjusting?
		}

		if (_loops && --_loops) {
			debug(9, "Real looping (now 0x%04x left)", _loops);
			setNextTime(_delayMin, _delayMax, _startTime);
		} else
			done(false);

		return;
	}

	_playing = false;
	_loops = 0;
	_startTime = 0;

	if (_controlMode >= kLBControlHideMouse) {
		debug(2, "Showing cursor");
		_vm->_cursor->showCursor();
		_vm->lockSound(this, false);

		if (_controlMode >= kLBControlPauseItems) {
			debug(2, "Enabling all");
			_vm->setEnableForAll(true, this);
		}
	}

	if (_timingMode == kLBAutoUserIdle) {
		debug(9, "Looping in done() - %d to %d", _periodMin, _periodMax);
		setNextTime(_periodMin, _periodMax);
	}

	runScript(kLBEventDone);
	notify(0xFFFF, _itemId);
}

void LBItem::init() {
	runScript(kLBEventInit);
}

void LBItem::setVisible(bool visible) {
	bool wasVisible = isVisible();
	int16 before = _visibility;
	// v3 visibility is a signed balance, not a boolean. A temporary hide/show
	// pair must preserve an item's authored or script-established hidden state.
	if (_vm->getGameType() == GType_LIVINGBOOKSV3)
		_visibility += visible ? 1 : -1;
	else
		_visibility = visible ? 0 : -1;
	_vm->recordVisibilityEvent(_itemId, before, _visibility, false, visible);
	if (wasVisible != isVisible())
		_vm->_needsRedraw = true;
}

void LBItem::setGlobalVisible(bool visible) {
	bool wasEnabled = isVisible();
	bool before = _globalVisible;
	_globalVisible = visible;
	_vm->recordVisibilityEvent(_itemId, before, _globalVisible, true, visible);
	if (wasEnabled != isVisible())
		_vm->_needsRedraw = true;
}

void LBItem::startPhase(uint phase) {
	if (_phase == phase) {
		if (_phase != kLBPhaseNone) {
			setEnabled(true);
		}

		load();
	}

	switch (phase) {
	case kLBPhaseLoad:
		runScript(kLBEventListLoad);
		break;
	case kLBPhaseCreate:
		runScript(kLBEventPhaseCreate);
		if (_timingMode == kLBAutoCreate) {
			debug(2, "Phase create: time startup");
			setNextTime(_periodMin, _periodMax);
		}
		break;
	case kLBPhaseInit:
		runScript(kLBEventPhaseInit);
		if (_timingMode == kLBAutoInit) {
			debug(2, "Phase init: time startup");
			setNextTime(_periodMin, _periodMax);
		}
		break;
	case kLBPhaseIntro:
		runScript(kLBEventPhaseIntro);
		if (_timingMode == kLBAutoIntro || _timingMode == kLBAutoUserIdle) {
			debug(2, "Phase intro: time startup");
			setNextTime(_periodMin, _periodMax);
		}
		break;
	case kLBPhaseMain:
		runScript(kLBEventPhaseMain);
		if (_timingMode == kLBAutoUserIdle || _timingMode == kLBAutoMain) {
			debug(2, "Phase main: time startup");
			setNextTime(_periodMin, _periodMax);
		}
		break;
	default:
		break;
	}
}

void LBItem::stop() {
	if (!_playing)
		return;

	_loops = 0;
	seek(0xFFFF);
	done(true);
}

void LBItem::load() {
	if (_loaded)
		return;

	_loaded = true;

	// FIXME: events etc
	if (_timingMode == kLBAutoLoad) {
		debug(2, "Load: time startup");
		setNextTime(_periodMin, _periodMax);
	}
}

void LBItem::unload() {
	if (!_loaded)
		return;

	_loaded = false;
	_vm->clearRollover(this);
	endDrag();

	// FIXME: stuff
}

void LBItem::moveBy(const Common::Point &pos) {
	_rect.translate(pos.x, pos.y);
	for (uint i = 0; i < _children.size(); ++i)
		_children[i]->moveBy(pos);
	_vm->_needsRedraw = true;
}

void LBItem::moveTo(const Common::Point &pos) {
	moveBy(pos - Common::Point(_rect.left, _rect.top));
}

void LBItem::setParent(LBItem *parent) {
	// Reject cycles before changing either relationship.
	for (LBItem *ancestor = parent; ancestor; ancestor = ancestor->_parent)
		if (ancestor == this)
			return;
	if (_parent == parent)
		return;
	if (_parent) {
		for (uint i = 0; i < _parent->_children.size(); ++i) {
			if (_parent->_children[i] == this) {
				_parent->_children.remove_at(i);
				break;
			}
		}
	}
	_parent = parent;
	if (_parent)
		_parent->_children.push_back(this);
}

void LBItem::playFrom(uint32 position) {
	seekToTime(position);
	togglePlaying(false, false);
}

LBItem *LBItem::clone(uint16 newId, const Common::String &newName) {
	LBItem *item = createClone();

	item->_itemId = newId;
	item->_desc = newName;

	item->_resourceId = _resourceId;
	// FIXME: the rest

	_page->addClonedItem(item);
	// FIXME: zorder?
	return item;
}

LBItem *LBItem::createClone() {
	return new LBItem(_vm, _page, _rect);
}

void LBItem::setNextTime(uint16 min, uint16 max) {
	setNextTime(min, max, getTime());
}

uint32 LBItem::getTime() const {
	uint32 time = _vm->_system->getMillis();
	// The v3 interpreter uses timeGetTime for PlayInfo deadlines. Earlier
	// data expresses the same fields in ticks instead of milliseconds.
	return _vm->getGameType() == GType_LIVINGBOOKSV3 ? time : time / 16;
}

void LBItem::setNextTime(uint16 min, uint16 max, uint32 start) {
	_nextTime = start + _vm->_rnd->getRandomNumberRng((uint)min, (uint)max);
	debug(9, "nextTime is now %d time units away", _nextTime - getTime());
}

} // End of namespace Mohawk
