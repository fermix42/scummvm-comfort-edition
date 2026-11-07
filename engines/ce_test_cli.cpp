/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engines/ce_test_cli.h"
#include "engines/engine.h"
#include "engines/metaengine.h"
#include "engines/ce_achievements.h"
#include "common/config-manager.h"
#include "common/fs.h"
#include "common/events.h"
#include "common/system.h"
#include "common/stream.h"
#include "common/savefile.h"
#include "common/file.h"
#include "graphics/surface.h"
#include "gui/gui-manager.h"
#include "backends/keymapper/keymapper.h"
#include "backends/keymapper/action.h"

namespace CETestCLI {
using namespace Common;
static Read s_read = nullptr;
static Write s_write = nullptr;
static String s_root, s_buffer;
static String s_videoDriver;
static bool s_offscreen = false, s_eof = false, s_busy = false, s_launched = false;
static bool s_failed = false;
static bool s_quitSent = false;
static uint32 s_frame = 0, s_started = 0;
static JSONValue *s_pending = nullptr;
static uint32 s_deadline = 0;
static uint32 s_targetFrame = 0;

bool active() { return s_read != nullptr; }
bool offscreen() { return active() && s_offscreen; }
void setVideoDriver(const char *driver) { s_videoDriver = driver ? driver : "uninitialized"; }
void fatal(const char *message) {
	if (!active()) return;
	JSONObject o; o["ok"] = new JSONValue(false); o["error"] = new JSONValue("engine_fatal");
	o["message"] = new JSONValue(message);
	JSONValue out(o); String line = out.stringify() + "\n"; s_write(line.c_str(), line.size());
}
void activate(Read read, Write write, const String &root, bool offscreenMode) {
	s_read = read; s_write = write; s_root = root; s_offscreen = offscreenMode;
}
String stringField(JSONValue &v, const char *k) {
	return v.hasChild(k) && v.child(k)->isString() ? v.child(k)->asString() : String();
}
int intField(JSONValue &v, const char *k, int fallback) {
	if (!v.hasChild(k) || !v.child(k)->isIntegerNumber()) return fallback;
	const long long n = v.child(k)->asIntegerNumber();
	return n < -2147483647LL || n > 2147483647LL ? fallback : (int)n;
}
JSONValue *ok() { JSONObject o; o["ok"] = new JSONValue(true); return new JSONValue(o); }
JSONValue *error(const char *code) {
	JSONObject o; o["ok"] = new JSONValue(false); o["error"] = new JSONValue(code); return new JSONValue(o);
}
JSONValue *unsupported() { return error("unsupported"); }
static void put(JSONObject &o, const char *key, const String &v) { o[key] = new JSONValue(v); }
static void reply(JSONValue *request, JSONValue *response) {
	JSONObject o = response->asObject();
	// Deep copy: JSONValue owns its object children.
	JSONObject result;
	for (const auto &entry : o) result[entry._key] = new JSONValue(*entry._value);
	if (request && request->hasChild("id")) result["id"] = new JSONValue(*request->child("id"));
	JSONValue out(result);
	String line = out.stringify() + "\n";
	s_write(line.c_str(), line.size());
	delete response;
}
static JSONValue *next() {
	char buf[4096];
	int count = s_read(buf, sizeof(buf));
	if (count < 0) s_eof = true;
	else if (count) s_buffer += String(buf, (uint32)count);
	if (s_buffer.size() > 65536) {
		s_buffer.clear(); s_failed = true; reply(nullptr, error("line_too_long"));
		return nullptr;
	}
	size_t end = s_buffer.find('\n');
	if (end == String::npos) return nullptr;
	String line = s_buffer.substr(0, end); s_buffer.erase(0, end + 1);
	unsigned depth = 0; bool quoted = false, escaped = false, valid = true;
	for (char c : line) {
		if (c == '\0') valid = false;
		if (escaped) { escaped = false; continue; }
		if (quoted && c == '\\') { escaped = true; continue; }
		if (c == '"') { quoted = !quoted; continue; }
		if (!quoted && (c == '[' || c == '{') && ++depth > 32) valid = false;
		if (!quoted && (c == ']' || c == '}')) { if (!depth) valid = false; else --depth; }
	}
	JSONValue *v = valid ? JSON::parse(line) : nullptr;
	if (!v || !v->isObject() || stringField(*v, "cmd").empty()) {
		delete v; reply(nullptr, error("invalid_request")); return nullptr;
	}
	return v;
}
static JSONValue *snapshot() {
	JSONObject o;
	o["ok"] = new JSONValue(true);
	put(o, "status", g_engine ? "running" : "idle");
	put(o, "rendering", offscreen() ? "offscreen-opengl" : "headless-sdl-dummy");
	put(o, "video_driver", s_videoDriver);
	o["scummvm_dialog"] = g_engine ? new JSONValue(g_gui.isActive()) : new JSONValue();
	o["frame"] = new JSONValue((long long)s_frame);
	if (!g_engine) return new JSONValue(o);
	const String target = ConfMan.getActiveDomainName();
	put(o, "target", target); put(o, "engine", ConfMan.get("engineid"));
	put(o, "game", ConfMan.get("gameid")); put(o, "platform", ConfMan.get("platform"));
	auto &ce = CEAchievements();
	put(o, "session", ce.getCurrentPlaythroughForTarget(target));
	o["challenge"] = new JSONValue(ce.isChallengeModeEnabled());
	o["cheats"] = new JSONValue(ce.areCheatsEnabled());
	o["ce"] = ce.testCLIState();
		o["adapter"] = g_engine->testCLIState();
	JSONArray actions;
	for (const auto *map : g_system->getEventManager()->getKeymapper()->getKeymaps()) {
		if (!map->isEnabled() || map->getType() != Keymap::kKeymapTypeGame) continue;
		for (const auto *action : map->getActions()) {
			JSONObject a; put(a, "map", map->getId()); put(a, "id", action->id);
			actions.push_back(new JSONValue(a));
		}
	}
	o["actions"] = new JSONValue(actions);
	return new JSONValue(o);
}
static JSONValue *field(JSONValue *v, const String &path) {
	size_t begin = 0;
	while (v) {
		size_t end = path.find('.', begin);
		String name = end == String::npos ? path.substr(begin) : path.substr(begin, end - begin);
		if (!v->isObject() || !v->hasChild(name.c_str())) return nullptr;
		v = v->child(name.c_str());
		if (end == String::npos) return v;
		begin = end + 1;
	}
	return nullptr;
}
static bool matches(JSONValue &req, JSONValue *state) {
	JSONValue *actual = field(state, stringField(req, "field"));
	return actual && req.hasChild("equals") && actual->stringify() == req.child("equals")->stringify();
}
static JSONValue *input(JSONValue &req) {
	if (stringField(req, "kind") == "logical") return g_engine->testCLICommand(req);
	Event event;
	String kind = stringField(req, "kind"), phase = stringField(req, "phase");
	if (phase != "down" && phase != "up" && kind != "mouse_move" && kind != "axis") return error("invalid_phase");
	bool down = phase == "down";
	if (kind == "key") {
		int key = intField(req, "key"), flags = intField(req, "flags", 0), ascii = intField(req, "ascii", 0);
		if (key < 1 || key >= KEYCODE_LAST || flags < 0 || flags > 255 || ascii < 0 || ascii > 65535) return error("invalid_key");
		event.type = down ? EVENT_KEYDOWN : EVENT_KEYUP;
		event.kbd = KeyState((KeyCode)key, ascii, flags);
	} else if (kind == "action") {
		const Keymap *map = g_system->getEventManager()->getKeymapper()->getKeymap(stringField(req, "map"));
		if (!map || !map->isEnabled() || map->getType() != Keymap::kKeymapTypeGame) return unsupported();
		const Action *found = nullptr;
		for (const auto *a : map->getActions()) if (a->id == stringField(req, "action")) found = a;
		if (!found) return unsupported();
		event = found->event;
		// Axis actions need the incoming hardware magnitude supplied by
		// Keymapper::executeAction; an Action template alone has no such value.
		// Use physical axis input with map_physical for those bindings.
		if (event.type == EVENT_CUSTOM_ENGINE_ACTION_AXIS || event.type == EVENT_CUSTOM_BACKEND_ACTION_AXIS) return unsupported();
		if (isMouseEvent(event)) event.mouse = g_system->getEventManager()->getMousePos();
		if (!down) {
			switch (event.type) {
			case EVENT_KEYDOWN: event.type = EVENT_KEYUP; break;
			case EVENT_CUSTOM_ENGINE_ACTION_START: event.type = EVENT_CUSTOM_ENGINE_ACTION_END; break;
			case EVENT_LBUTTONDOWN: event.type = EVENT_LBUTTONUP; break;
			case EVENT_RBUTTONDOWN: event.type = EVENT_RBUTTONUP; break;
			default: return unsupported();
			}
		}
	} else if (kind == "button") {
		int button = intField(req, "button"); if (button < 0 || button > 31) return error("invalid_button");
		event.type = down ? EVENT_JOYBUTTON_DOWN : EVENT_JOYBUTTON_UP; event.joystick.button = button;
	} else if (kind == "axis") {
		int axis = intField(req, "axis"), position = intField(req, "position", 100000);
		// Six standard stick/trigger axes. Engines such as Grim allocate exactly
		// six entries; D-pad/hat input must use buttons or installed actions.
		if (axis < 0 || axis > 5 || position < -32768 || position > 32767) return error("invalid_axis");
		event.type = EVENT_JOYAXIS_MOTION; event.joystick.axis = axis; event.joystick.position = position;
	} else if (kind == "mouse_move" || kind == "mouse_button") {
		int x = intField(req, "x"), y = intField(req, "y");
		if (x < 0 || y < 0 || x >= g_system->getWidth() || y >= g_system->getHeight()) return error("invalid_coordinates");
		event.mouse = Point(x, y); event.type = EVENT_MOUSEMOVE;
		if (kind == "mouse_button") {
			int b = intField(req, "button", 1);
			if (b != 1 && b != 2) return unsupported();
			event.type = b == 1 ? (down ? EVENT_LBUTTONDOWN : EVENT_LBUTTONUP) : (down ? EVENT_RBUTTONDOWN : EVENT_RBUTTONUP);
		}
	} else return unsupported();
	// Keymapper actions use the installed Action's production output event;
	// physical event translation is optional and explicitly requested below.
	if (req.hasChild("map_physical") && req.child("map_physical")->isBool() && req.child("map_physical")->asBool()) {
		List<Event> events;
		if (!g_system->getEventManager()->getKeymapper()->mapEvent(event, events)) events.push_back(event);
		for (const auto &e : events) g_system->getEventManager()->pushEvent(e);
	} else g_system->getEventManager()->pushEvent(event);
	return ok();
}
static JSONValue *dispatch(JSONValue &req) {
	const String cmd = stringField(req, "cmd");
	if (cmd == "inspect" || cmd == "status") return snapshot();
	if (cmd == "capabilities") {
		JSONObject o; o["ok"] = new JSONValue(true);
		put(o, "protocol", "ce-test-cli/1");
		put(o, "lifecycle", "one game per process; fresh process for restart");
		put(o, "commands", "launch inspect status capabilities input advance wait assert saves save load session_create session_select storage screenshot storage_probe quit");
		o["engine_adapter"] = g_engine ? g_engine->testCLIState() : unsupported();
		return new JSONValue(o);
	}
	if (cmd == "quit") {
		s_eof = true; return ok();
	}
	if (cmd != "input" && cmd != "assert" && cmd != "session_create" && cmd != "session_select" && cmd != "saves" && cmd != "save" && cmd != "load" && cmd != "storage" && cmd != "screenshot" && cmd != "storage_probe") return error("unknown_command");
	if (!g_engine) return error("not_running");
	if (cmd == "input") return input(req);
	if (cmd == "assert") {
		JSONValue *state = snapshot(); bool match = matches(req, state); delete state;
		if (!match) s_failed = true;
		return match ? ok() : error("assertion_failed");
	}
	if (cmd == "session_create") {
		String title = stringField(req, "title");
		if (title.empty() || title.size() > 128) return error("invalid_title");
		if (!CEAchievements().startPlaythroughForTarget(ConfMan.getActiveDomainName(), ConfMan.get("gameid"), ConfMan.get("platform"), title)) return error("session_failed");
		CEAchievements().resetPopupState(); // A new session cannot inherit another save's progress/sidecar context.
		ConfMan.flushToDisk(); return snapshot();
	}
	if (cmd == "session_select") {
		if (!CEAchievements().selectPlaythroughForTarget(ConfMan.getActiveDomainName(), stringField(req, "session"))) return error("session_not_selectable");
		ConfMan.flushToDisk(); return snapshot();
	}
	if (cmd == "saves") {
		JSONObject o; o["ok"] = new JSONValue(true); JSONArray saves;
		const String target = ConfMan.getActiveDomainName();
		for (const auto &s : g_engine->getMetaEngine()->listSaves(target.c_str())) {
			JSONObject entry; entry["chooser_slot"] = new JSONValue((long long)s.getSaveSlot());
			put(entry, "description", s.getDescription());
			const String file = g_engine->getMetaEngine()->getSavegameFile(s.getSaveSlot(), target.c_str());
			put(entry, "file", file); put(entry, "session", CEAchievements().getSavePlaythroughForFile(file));
			SaveStateDescriptor metadata = g_engine->getMetaEngine()->querySaveMetaInfos(target.c_str(), s.getSaveSlot());
			const Graphics::Surface *thumbnail = metadata.getThumbnail();
			entry["thumbnail_width"] = new JSONValue((long long)(thumbnail ? thumbnail->w : 0));
			entry["thumbnail_height"] = new JSONValue((long long)(thumbnail ? thumbnail->h : 0));
			saves.push_back(new JSONValue(entry));
		}
		o["saves"] = new JSONValue(saves); return new JSONValue(o);
	}
	if (cmd == "storage") {
		JSONObject o; o["ok"] = new JSONValue(true);
		for (const char *key : {"savepath", "screenshotpath", "logfile", "iconspath", "dlcspath", "cachepath", "temppath"})
			put(o, key, g_system->resolveStoragePath(key, ConfMan.getPath(key)).toString(Path::kNativeSeparator));
		put(o, "generated_cache", g_system->resolveStoragePath("enginecache", Path("maps/example.ainodes")).toString(Path::kNativeSeparator));
		return new JSONValue(o);
	}
	if (cmd == "storage_probe") {
		// Fixed diagnostic names only, beneath the already owned/validated root.
		// No request field can supply a filename or output directory.
		const Path nested = ConfMan.getPath("dlcspath").join("ce-storage-probe/nested");
		if (!FSNode(nested).createDirectory()) return error("storage_probe_directory_failed");
		for (const char *key : {"cachepath", "temppath"}) {
			const Path directory = ConfMan.getPath(key);
			if (!FSNode(directory).exists() && !FSNode(directory).createDirectory()) return error("storage_probe_directory_failed");
			const Path filename = directory.join("ce-storage-probe.bin");
			if (FSNode(filename).exists()) return error("storage_probe_file_exists");
			DumpFile file;
			if (!file.open(filename)) return error("storage_probe_write_failed");
			file.writeString("CE storage diagnostic\n"); file.flush();
			if (file.err()) return error("storage_probe_write_failed");
		}
		return ok();
	}
	if (cmd == "screenshot") {
		g_system->saveScreenshot(); // Normal renderer and normal filename/path policy.
		return ok(); // Acknowledges the request; the client must verify its output.
	}
	if (cmd == "save" || cmd == "load") return g_engine->testCLICommand(req);
	return error("unknown_command");
}
bool prepare(StringArray &arguments) {
	// Only an empty disposable directory or one previously owned by this mode.
	FSNode root(Path(s_root, Path::kNativeSeparator)); FSList entries;
	if (!root.isDirectory() || !root.getChildren(entries, FSNode::kListAll)) { reply(nullptr, error("invalid_test_root")); return false; }
	bool owned = root.getChild("ce-test-cli.marker").exists();
	if (!entries.empty() && !owned) { reply(nullptr, error("test_root_not_empty")); return false; }
	if (!owned) {
		WriteStream *marker = root.getChild("ce-test-cli.marker").createWriteStream();
		if (!marker) return false;
		marker->writeString("ce-test-cli/1\n"); marker->finalize(); bool good = !marker->err(); delete marker; if (!good) return false;
	}
	if (!root.getChild("saves").exists() && !root.getChild("saves").createDirectory()) return false;
	JSONObject ready; ready["ok"] = new JSONValue(true); put(ready, "event", "ready"); put(ready, "protocol", "ce-test-cli/1");
	reply(nullptr, new JSONValue(ready));
	while (!s_eof) {
		JSONValue *req = next();
		if (!req) { g_system->delayMillis(5); continue; }
		if (stringField(*req, "cmd") != "launch") { reply(req, dispatch(*req)); delete req; continue; }
		String path = stringField(*req, "path"), game = stringField(*req, "game");
		String platform = stringField(*req, "platform"), engine = stringField(*req, "engine");
		bool safe = !path.empty() && path.findFirstOf("\r\n") == String::npos;
		// Canonicalize caller-relative assets before the entry point anchors
		// diagnostic output to the owned root. Absolute identities stay intact.
		if (safe && path[0] != '/' && path[0] != '\\' && (path.size() < 2 || path[1] != ':'))
			path = FSNode(Path(".")).getPath().join(Path(path, Path::kNativeSeparator)).normalize().toString(Path::kNativeSeparator);
		for (const auto &id : {game, platform, engine}) {
			if (id.empty() || id.size() > 40) safe = false;
			for (char c : id) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) safe = false;
		}
		if (!safe || !FSNode(Path(path, Path::kNativeSeparator)).isDirectory()) { reply(req, error("invalid_launch")); delete req; continue; }
		const String config = s_root + "/profile.ini";
		if (!root.getChild("profile.ini").exists()) {
			WriteStream *stream = root.getChild("profile.ini").createWriteStream();
			if (!stream) { reply(req, error("config_failed")); delete req; continue; }
			stream->writeString("[scummvm]\nenable_unsupported_game_warning=false\nconsole=false\nconfirm_exit=false\ngui_return_to_launcher_at_exit=false\nautosave_period=0\nce_achievements_popups_enabled=false\nce_link_token=\n\n[cli-game]\nengineid=" + engine + "\ngameid=" + game + "\nplatform=" + platform + "\npath=" + path + "\nce_playthrough_tracking=true\ncheck_gamedata=false\n");
			stream->finalize(); bool good = !stream->err(); delete stream; if (!good) { delete req; return false; }
		} else {
			// Existing owned profiles may persist sessions, but cannot change asset identity.
			ConfMan.loadConfigFile(Path(config, Path::kNativeSeparator), Path());
			if (ConfMan.get("path", "cli-game") != path || ConfMan.get("gameid", "cli-game") != game || ConfMan.get("platform", "cli-game") != platform || ConfMan.get("engineid", "cli-game") != engine) {
				reply(req, error("profile_identity_mismatch")); delete req; continue;
			}
		}
		arguments.push_back("--config=" + config);
		arguments.push_back("--savepath=" + s_root + "/saves");
		arguments.push_back("--logfile=" + s_root + "/engine.log");
		arguments.push_back(offscreen() ? "--renderer=opengl" : "--renderer=software");
		arguments.push_back(offscreen() ? "--gfx-mode=opengl" : "--gfx-mode=surfacesdl"); arguments.push_back("--no-fullscreen");
		arguments.push_back("--no-console"); arguments.push_back("--music-driver=null");
		arguments.push_back("cli-game");
		s_pending = req; s_started = g_system->getMillis(); s_deadline = s_started + 120000; s_launched = true;
		return true;
	}
	return false;
}
void pump() {
	if (!active() || s_busy || !s_launched) return;
	s_busy = true;
	if (s_pending) {
		const String cmd = stringField(*s_pending, "cmd");
		JSONValue *state = snapshot();
		bool done = cmd == "launch" ? (g_engine && (ConfMan.get("gameid") != "monkey4" || s_frame > 0)) : cmd == "advance" ? s_frame >= s_targetFrame : matches(*s_pending, state);
		if (done || (int32)(g_system->getMillis() - s_deadline) >= 0) {
			if (!done) { s_failed = true; reply(s_pending, error("timeout")); delete state; }
			else reply(s_pending, state);
			delete s_pending; s_pending = nullptr;
		} else delete state;
	}
	if (s_pending) {
		JSONValue *request = next();
		if (request) {
			const String cmd = stringField(*request, "cmd");
			if (cmd == "quit") {
				reply(s_pending, error("cancelled")); delete s_pending; s_pending = nullptr;
				reply(request, dispatch(*request));
			} else if (cmd == "inspect" || cmd == "status" || cmd == "capabilities") reply(request, dispatch(*request));
			else reply(request, error("busy"));
			delete request;
		}
	}
	if (!s_pending) {
		JSONValue *req = next();
		if (req) {
			if (stringField(*req, "cmd") == "advance") {
				int frames = intField(*req, "frames");
				if (!s_frame) { reply(req, unsupported()); delete req; }
				else if (frames < 1 || frames > 3000) { reply(req, error("invalid_frames")); delete req; }
				else { s_pending = req; s_targetFrame = s_frame + frames; s_deadline = g_system->getMillis() + 120000; }
			} else if (stringField(*req, "cmd") == "wait") {
				int timeout = intField(*req, "timeout_ms", 10000);
				if (timeout < 1 || timeout > 120000 || stringField(*req, "field").empty() || !req->hasChild("equals")) { reply(req, error("invalid_wait")); delete req; }
				else { s_pending = req; s_deadline = g_system->getMillis() + timeout; }
			} else { reply(req, dispatch(*req)); delete req; }
		}
	}
	if (s_eof && !s_quitSent) {
		Event quit; quit.type = EVENT_QUIT;
		g_system->getEventManager()->pushEvent(quit); s_quitSent = true;
	}
	s_busy = false;
}
void frame() { if (active()) { ++s_frame; pump(); } }
int finish(int result) {
	if (s_pending) { reply(s_pending, error("engine_exited")); delete s_pending; s_pending = nullptr; s_failed = true; }
	if (!s_launched && s_eof && result == 64) result = 0;
	return result ? result : s_failed ? 2 : 0;
}
}
