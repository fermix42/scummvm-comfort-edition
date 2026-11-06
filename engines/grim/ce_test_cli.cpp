/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engines/grim/grim.h"
#include "engines/grim/set.h"
#include "engines/grim/actor.h"
#include "engines/grim/textobject.h"
#include "engines/grim/ce_emi_windows_saves.h"
#include "engines/grim/ce_ps2_saves.h"
#include "engines/grim/lua/lapi.h"
#include "engines/ce_test_cli.h"
#include "common/config-manager.h"
#include "common/events.h"

namespace Grim {
using namespace Common;
static lua_Object member(lua_Object table, const char *key) {
	if (!lua_istable(table)) return LUA_NOOBJECT;
	lua_pushobject(table); lua_pushstring(key); return lua_rawgettable();
}
// Bounded raw diagnostics for fixed native-menu tables only. Never invokes Lua.
static JSONValue *menuValue(const TObject *value, int depth, int &budget) {
	if (budget-- <= 0) return nullptr;
	if (value->ttype == LUA_T_NUMBER) return new JSONValue((double)nvalue(value));
	if (value->ttype == LUA_T_STRING) return new JSONValue(String(svalue(value)).substr(0, 1024));
	if (value->ttype != LUA_T_ARRAY || depth <= 0) return nullptr;
	JSONObject o;
	const Hash *table = avalue(value);
	for (int i = 0; i < table->nhash && i < 2048 && budget > 0; ++i) {
		const Node &entry = table->node[i]; String key;
		if (entry.ref.ttype == LUA_T_STRING) key = svalue(&entry.ref);
		else if (entry.ref.ttype == LUA_T_NUMBER) key = String::format("%g", (double)nvalue(&entry.ref));
		else continue;
		if (key.empty() || key.size() > 128) continue;
		JSONValue *child = menuValue(&entry.val, depth - 1, budget);
		if (child) o[key] = child;
	}
	return new JSONValue(o);
}
JSONValue *GrimEngine::testCLIState() {
	if (!CETestCLI::active() || _gameType != GType_MONKEY4) return CETestCLI::unsupported();
	JSONObject o;
	o["ok"] = new JSONValue(true);
	JSONArray capabilities;
	for (const char *name : {"scene", "actor", "native_menu_handler", "native_menu_fields", "logical_input", "native_slot_load", "native_save_menu", "frames"}) capabilities.push_back(new JSONValue(name));
	o["capabilities"] = new JSONValue(capabilities);
	o["mode"] = new JSONValue((long long)_mode);
	o["scene"] = _currSet ? new JSONValue(_currSet->getName()) : new JSONValue();
	o["save_pending"] = new JSONValue(_savegameSaveRequest);
	o["load_pending"] = new JSONValue(_savegameLoadRequest);
	o["renderer"] = new JSONValue(_softRenderer ? "tinygl" : "opengl");
	o["native_slots"] = new JSONValue((long long)(_gamePlatform == kPlatformPS2 ? 4 : 160));
	if (_selectedActor) {
		JSONObject actor; actor["name"] = new JSONValue(_selectedActor->getName());
		Math::Vector3d p = _selectedActor->getPos();
		actor["x"] = new JSONValue((double)p.x()); actor["y"] = new JSONValue((double)p.y()); actor["z"] = new JSONValue((double)p.z());
		actor["yaw"] = new JSONValue((double)_selectedActor->getYaw().getDegrees());
		o["actor"] = new JSONValue(actor);
	} else o["actor"] = new JSONValue();
	// Raw reads only: no Lua evaluation, callbacks, metamethods or writes.
	String menu = "unsupported";
	String inputHandler = "unsupported";
	String handlerKind = "uninitialized";
	JSONValue *cutscene = new JSONValue();
	JSONObject menus;
	JSONValue *nativeDialog = new JSONValue();
	if (lua_isopen()) {
		lua_beginblock();
		lua_Object handler = member(lua_rawgetglobal("system"), "buttonHandler");
		handlerKind = lua_istable(handler) ? "table" : lua_isfunction(handler) ? "function" : "missing";
		if (_mode == DrawMode && lua_istable(handler)) {
			int dialogBudget = 256;
			delete nativeDialog;
			nativeDialog = menuValue(luaA_Address(handler), 3, dialogBudget);
		}
		lua_Object level = lua_rawgetglobal("cutSceneLevel");
		if (lua_isnumber(level)) { delete cutscene; cutscene = new JSONValue((long long)lua_getnumber(level)); }
		const char *handlers[] = { "SampleButtonHandler", "anykeyButtonHandler" };
		for (const char *name : handlers) {
			lua_Object candidate = lua_rawgetglobal(name);
			if (lua_isfunction(handler) && lua_isfunction(candidate) && luaO_equalObj(luaA_Address(handler), luaA_Address(candidate))) inputHandler = name;
		}
		const char *names[] = { "main_menu", "saveload_menu", "settings_menu", "quit_menu" };
		int budget = 512;
		for (const char *name : names) {
			lua_Object owner = lua_rawgetglobal(name);
			if (lua_istable(owner)) {
				JSONValue *value = menuValue(luaA_Address(owner), 3, budget);
				if (value) menus[name] = value;
			}
			if (lua_istable(handler) && lua_istable(owner) && luaO_equalObj(luaA_Address(handler), luaA_Address(owner))) menu = name;
			lua_Object candidate = member(owner, "buttonHandler");
			if (lua_isfunction(handler) && lua_isfunction(candidate) && luaO_equalObj(luaA_Address(handler), luaA_Address(candidate))) menu = name;
		}
		lua_endblock();
	}
	o["menu_handler"] = new JSONValue(menu);
	o["input_handler"] = new JSONValue(inputHandler);
	o["button_handler_kind"] = new JSONValue(handlerKind);
	o["cutscene_level"] = cutscene;
	o["menus"] = new JSONValue(menus);
	o["native_dialog"] = nativeDialog;
	JSONArray texts;
	for (TextObject *text : TextObject::getPool()) {
		if (texts.size() >= 64) break;
		texts.push_back(new JSONValue(text->getName()));
	}
	o["text_objects"] = new JSONValue(texts);
	JSONObject controls;
	const int codes[] = { Common::KEYCODE_F1, KEYCODE_JOY1_START, Common::KEYCODE_UP, KEYCODE_JOY1_HUP, Common::KEYCODE_PERIOD };
	const char *labels[] = { "f1", "start", "keyboard_up", "dpad_up", "skip_line" };
	for (uint i = 0; i < ARRAYSIZE(codes); ++i) {
		JSONObject control; control["enabled"] = new JSONValue(_controlsEnabled[codes[i]]); control["held"] = new JSONValue(_controlsState[codes[i]]);
		controls[labels[i]] = new JSONValue(control);
	}
	o["controls"] = new JSONValue(controls);
	return new JSONValue(o);
}
JSONValue *GrimEngine::testCLICommand(JSONValue &request) {
	if (!CETestCLI::active() || _gameType != GType_MONKEY4) return CETestCLI::unsupported();
	if (CETestCLI::stringField(request, "cmd") == "input") {
		String action = CETestCLI::stringField(request, "action"), phase = CETestCLI::stringField(request, "phase");
		if (phase != "down" && phase != "up") return CETestCLI::error("invalid_phase");
		const bool ps2 = _gamePlatform == kPlatformPS2;
		int key = -1;
		if (action == "open_menu") key = ps2 ? KEYCODE_JOY1_START : Common::KEYCODE_F1;
		else if (action == "confirm") key = ps2 ? KEYCODE_JOY1_A : Common::KEYCODE_RETURN;
		else if (action == "cancel") key = ps2 ? KEYCODE_JOY1_B : Common::KEYCODE_ESCAPE;
		else if (action == "skip") key = Common::KEYCODE_ESCAPE;
		else if (action == "up") key = ps2 ? KEYCODE_JOY1_HUP : Common::KEYCODE_UP;
		else if (action == "down") key = ps2 ? KEYCODE_JOY1_HDOWN : Common::KEYCODE_DOWN;
		else if (action == "left") key = ps2 ? KEYCODE_JOY1_HLEFT : Common::KEYCODE_LEFT;
		else if (action == "right") key = ps2 ? KEYCODE_JOY1_HRIGHT : Common::KEYCODE_RIGHT;
		if (key < 0) return CETestCLI::unsupported();
		Common::Event event; event.type = phase == "down" ? Common::EVENT_KEYDOWN : Common::EVENT_KEYUP;
		event.kbd = Common::KeyState((Common::KeyCode)key, key < 128 ? key : 0);
		g_system->getEventManager()->pushEvent(event);
		return CETestCLI::ok();
	}
	if (_savegameLoadRequest || _savegameSaveRequest) return CETestCLI::error("busy");
	// MI4's bare saveGame() does not prepare native SUBS/PS2S metadata. Only
	// the game's own Save menu owns that setup. Never create incomplete files.
	if (CETestCLI::stringField(request, "cmd") == "save") return CETestCLI::error("native_save_menu_required");
	// Commands name a native slot in the active bank, never arbitrary filenames.
	int slot = CETestCLI::intField(request, "slot");
	if (slot < 0 || slot >= (_gamePlatform == kPlatformPS2 ? 4 : 160)) return CETestCLI::error("invalid_slot");
	const String session = CEAchievements().getCurrentPlaythroughForTarget(_targetName);
	if (session.empty()) return CETestCLI::error("session_required");
	String file = _gamePlatform == kPlatformPS2 ? String::format("efmi%03d.ps2", slot) : CEWindowsSaves::nativeFile(slot);
	{
		String resolved = resolveCESaveFilename(file);
		if (CEAchievements().getSavePlaythroughForFile(resolved) != session) return CETestCLI::error("save_not_owned");
		if (!g_system->getSavefileManager()->exists(resolved)) return CETestCLI::error("save_not_found");
		loadGame(file);
	}
	JSONObject result; result["ok"] = new JSONValue(true);
	result["status"] = new JSONValue("requested"); result["file"] = new JSONValue(resolveCESaveFilename(file));
	return new JSONValue(result);
}
}
