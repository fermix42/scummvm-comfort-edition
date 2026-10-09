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

#include "engines/ce_achievements.h"
#include "engines/achievements.h"

#include "common/compression/unzip.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/formats/json.h"
#include "common/fs.h"
#include "common/savefile.h"
#include "common/system.h"
#include "common/translation.h"
#include "common/util.h"

#ifdef USE_HTTP
#include "backends/networking/http/connectionmanager.h"
#include "backends/networking/http/httprequest.h"
#include "backends/networking/http/networkreadstream.h"
#endif

namespace Common {

static const char *const kCEApiBaseUrl = "https://achievements.divineramen.net/api/v1";
static const char *const kCEDataFile = "ce-achievements.dat";
static const char *const kCEQueueFile = "ce-achievements-queue.json";
static const char *const kCECatalogFile = "ce-achievements-catalog.json";
static const char *const kCELinkTokenKey = "ce_link_token";
static const char *const kCEChallengeModeKey = "ce_achievements_challenge_mode";
static const char *const kCECheatsEnabledKey = "ce_cheats_enabled";
static const char *const kCERepeatPopupsKey = "ce_achievements_repeat_popups";
static const char *const kCEInstallSecretKey = "ce_achievements_install_secret";
static const char *const kCELastSyncStatusKey = "ce_achievements_last_sync_status";

static const char *const kCEOperationStealthKeys[] = {
	"passport_to_trouble",
	"shave_and_a_briefing",
	"say_it_with_flowers",
	"safe_deposit_unsafe_withdrawal",
	"escape_clause",
	"two_for_the_surface",
	"palace_intrigue",
	"a_safe_bet",
	"making_waves",
	"under_new_management",
	"pen_mightier_than_piranhas",
	"rats_all_folks",
	"dressed_to_infiltrate",
	"fingerprint_fiction",
	"officially_unofficial",
	"virus_successfully_installed",
	"order_of_the_banana",
	"well_read_intruder",
	"corporate_espionage",
	nullptr
};

static String ceAchievementDisplayName(const String &achievement) {
	String result;
	bool capitalizeNext = true;
	for (uint i = 0; i < achievement.size(); ++i) {
		char c = achievement[i];
		if (c == '_' || c == '-') {
			result += ' ';
			capitalizeNext = true;
			continue;
		}
		if (capitalizeNext && c >= 'a' && c <= 'z')
			c = c - 'a' + 'A';
		result += c;
		capitalizeNext = false;
	}
	return result.empty() ? achievement : result;
}

static void ceDisplayAchievementUnlockedOSD(const String &title) {
	if (ConfMan.getBool("disable_achievement_unlocked_osd") || !g_system)
		return;

	U32String msg = Common::U32String::format("%S\n%S",
		_("Achievement unlocked!").c_str(),
		Common::U32String(title).c_str()
	);
	g_system->displayMessageOnOSD(msg);
}

static String ceEscapeJson(const String &input) {
	JSONValue value(input);
	return JSON::stringify(&value);
}

static String ceStringFromJson(const JSONObject &obj, const char *key, const String &fallback = String()) {
	if (!obj.contains(key) || !obj[key]->isString())
		return fallback;
	return obj[key]->asString();
}

static bool ceBoolFromJson(const JSONObject &obj, const char *key, bool fallback = false) {
	if (!obj.contains(key) || !obj[key]->isBool())
		return fallback;
	return obj[key]->asBool();
}

static uint32 ceUIntFromJson(const JSONObject &obj, const char *key, uint32 fallback = 0) {
	if (!obj.contains(key) || !obj[key]->isIntegerNumber())
		return fallback;
	return (uint32)obj[key]->asIntegerNumber();
}

static bool ceParseCatalogDefinition(const JSONObject &obj, CEAchievementDefinition &definition) {
	definition.game = ceStringFromJson(obj, "game");
	definition.variant = ceStringFromJson(obj, "variant", "*");
	definition.id = ceStringFromJson(obj, "id");
	definition.title = ceStringFromJson(obj, "title");
	definition.description = ceStringFromJson(obj, "description");
	definition.challengeEligible = ceBoolFromJson(obj, "challenge_eligible", true);
	definition.hidden = ceBoolFromJson(obj, "hidden", false);

	if (definition.game.empty() || definition.id.empty())
		return false;
	if (definition.title.empty())
		definition.title = ceAchievementDisplayName(definition.id);
	return true;
}

static bool ceParsePackAchievement(const String &game, const JSONObject &obj, CEAchievementDefinition &definition) {
	definition.game = game;
	definition.variant = ceStringFromJson(obj, "variant", "*");
	definition.id = ceStringFromJson(obj, "key");
	definition.title = ceStringFromJson(obj, "title");
	definition.description = ceStringFromJson(obj, "public_description",
		ceStringFromJson(obj, "locked_description", ceStringFromJson(obj, "description")));
	definition.challengeEligible = ceUIntFromJson(obj, "challenge_points", 0) > 0;
	definition.hidden = ceBoolFromJson(obj, "spoiler", false);

	if (definition.game.empty() || definition.id.empty())
		return false;
	if (definition.title.empty())
		definition.title = ceAchievementDisplayName(definition.id);
	return true;
}

static bool ceParseCatalogRule(const JSONObject &obj, CEAchievementRule &rule) {
	rule.game = ceStringFromJson(obj, "game");
	rule.variant = ceStringFromJson(obj, "variant", "*");
	rule.event = ceStringFromJson(obj, "event");
	rule.achievement = ceStringFromJson(obj, "achievement");

	return !rule.game.empty() && !rule.event.empty() && !rule.achievement.empty();
}

static const uint32 kCESha256K[64] = {
	0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
	0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
	0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
	0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
	0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
	0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
	0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
	0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

static uint32 ceRotR(uint32 value, uint bits) {
	return (value >> bits) | (value << (32 - bits));
}

static void ceSha256Transform(uint32 state[8], const byte block[64]) {
	uint32 w[64];
	for (uint i = 0; i < 16; ++i) {
		w[i] = ((uint32)block[i * 4] << 24) | ((uint32)block[i * 4 + 1] << 16) |
		       ((uint32)block[i * 4 + 2] << 8) | (uint32)block[i * 4 + 3];
	}
	for (uint i = 16; i < 64; ++i) {
		const uint32 s0 = ceRotR(w[i - 15], 7) ^ ceRotR(w[i - 15], 18) ^ (w[i - 15] >> 3);
		const uint32 s1 = ceRotR(w[i - 2], 17) ^ ceRotR(w[i - 2], 19) ^ (w[i - 2] >> 10);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}

	uint32 a = state[0], b = state[1], c = state[2], d = state[3];
	uint32 e = state[4], f = state[5], g = state[6], h = state[7];
	for (uint i = 0; i < 64; ++i) {
		const uint32 s1 = ceRotR(e, 6) ^ ceRotR(e, 11) ^ ceRotR(e, 25);
		const uint32 ch = (e & f) ^ (~e & g);
		const uint32 temp1 = h + s1 + ch + kCESha256K[i] + w[i];
		const uint32 s0 = ceRotR(a, 2) ^ ceRotR(a, 13) ^ ceRotR(a, 22);
		const uint32 maj = (a & b) ^ (a & c) ^ (b & c);
		const uint32 temp2 = s0 + maj;
		h = g;
		g = f;
		f = e;
		e = d + temp1;
		d = c;
		c = b;
		b = a;
		a = temp1 + temp2;
	}

	state[0] += a; state[1] += b; state[2] += c; state[3] += d;
	state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

static void ceAppendStringBytes(Array<byte> &bytes, const String &value) {
	for (uint i = 0; i < value.size(); ++i)
		bytes.push_back((byte)value[i]);
}

static Array<byte> ceSha256(const Array<byte> &input) {
	Array<byte> data = input;
	const uint64 bitLen = (uint64)data.size() * 8;
	data.push_back(0x80);
	while ((data.size() % 64) != 56)
		data.push_back(0);
	for (int i = 7; i >= 0; --i)
		data.push_back((byte)((bitLen >> (i * 8)) & 0xff));

	uint32 state[8] = {
		0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
		0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
	};
	for (uint i = 0; i < data.size(); i += 64)
		ceSha256Transform(state, &data[i]);

	Array<byte> digest;
	for (uint i = 0; i < 8; ++i) {
		digest.push_back((byte)((state[i] >> 24) & 0xff));
		digest.push_back((byte)((state[i] >> 16) & 0xff));
		digest.push_back((byte)((state[i] >> 8) & 0xff));
		digest.push_back((byte)(state[i] & 0xff));
	}
	return digest;
}

static String ceHexDigest(const Array<byte> &bytes) {
	String result;
	for (uint i = 0; i < bytes.size(); ++i)
		result += String::format("%02x", bytes[i]);
	return result;
}

static String ceHmacSha256(const String &secret, const String &payload) {
	Array<byte> key;
	ceAppendStringBytes(key, secret);
	if (key.size() > 64)
		key = ceSha256(key);
	while (key.size() < 64)
		key.push_back(0);

	Array<byte> inner;
	Array<byte> outer;
	for (uint i = 0; i < 64; ++i) {
		inner.push_back(key[i] ^ 0x36);
		outer.push_back(key[i] ^ 0x5c);
	}
	ceAppendStringBytes(inner, payload);
	Array<byte> innerHash = ceSha256(inner);
	for (uint i = 0; i < innerHash.size(); ++i)
		outer.push_back(innerHash[i]);
	return ceHexDigest(ceSha256(outer));
}

#ifdef USE_HTTP
class CEHttpRequest final : public Networking::HttpRequest {
	CEAchievementService *_service;
	Common::MemoryWriteStreamDynamic _contentsStream;
public:
	CEHttpRequest(CEAchievementService *service, const Common::String &url)
		: Networking::HttpRequest(nullptr, nullptr, url), _service(service),
		  _contentsStream(DisposeAfterUse::YES) {}

	void handle() override {
		if (!_stream)
			_stream = makeStream();

		if (_stream) {
			char buffer[512];
			uint32 bytes = _stream->read(buffer, sizeof(buffer));
			if (bytes != 0 && _contentsStream.write(buffer, bytes) != bytes)
				warning("CEHttpRequest: unable to write all response bytes");

			if (!_stream->eos())
				return;

			if (_stream->hasError()) {
				Networking::ErrorResponse error(this, false, true,
					Common::String::format("Stream is in error: %s", _stream->getError()), -1);
				finishError(error);
				return;
			}

			const long code = _stream->httpResponseCode();
			if (code != 200 && code != 201) {
				char *contents = Common::JSON::zeroTerminateContents(_contentsStream);
				Common::String response(contents);
				Networking::ErrorResponse error(this, false, true,
					response.empty() ? "HTTP response code is not successful" : response, code);
				finishError(error);
				return;
			}
			finishSuccess();
		}
	}

	void restart() override {
		Networking::HttpRequest::restart();
		_contentsStream = Common::MemoryWriteStreamDynamic(DisposeAfterUse::YES);
	}

	void finishSuccess() override {
		long httpCode = 0;
		if (getNetworkReadStream())
			httpCode = getNetworkReadStream()->httpResponseCode();
		char *contents = Common::JSON::zeroTerminateContents(_contentsStream);
		Common::String response(contents);
		_service->requestSucceeded(response, httpCode);
		Networking::HttpRequest::finishSuccess();
	}

	void finishError(const Networking::ErrorResponse &error, Networking::RequestState state = Networking::FINISHED) override {
		_service->requestFailed(error.response, error.httpResponseCode);
		Networking::HttpRequest::finishError(error, state);
	}
};
#endif

CEAchievementService::CEAchievementService() {
	_syncAction = kCEAchievementSyncIdle;
	_syncQueueIndex = -1;
	_catalogLoaded = false;
}

CEAchievementService &CEAchievements() {
	static CEAchievementService service;
	return service;
}

bool CEAchievementService::unlock(AchievementsManager &achievements, const String &game, const String &variant, const String &achievement) {
	const bool nativeReady = achievements.isReady();
	const bool nativeAlreadyAchieved = nativeReady && achievements.isAchieved(achievement);
	bool localSet = true;
	if (nativeReady)
		localSet = achievements.setAchievement(achievement);

	const bool queuedNewEvent = queueAchievementEvent(game, variant, achievement);
	if ((!nativeReady && queuedNewEvent) || (isRepeatPopupsEnabled() && (!nativeReady || nativeAlreadyAchieved)))
		ceDisplayAchievementUnlockedOSD(getAchievementTitle(game, variant, achievement));

	retryQueuedEvents();
	return localSet;
}

bool CEAchievementService::noteGameEvent(AchievementsManager &achievements, const String &game, const String &variant, const String &event) {
	const CEAchievementRule *rule = findRule(game, variant, event);
	if (!rule)
		return false;

	return unlock(achievements, game, variant, rule->achievement);
}

void CEAchievementService::loadCatalogIfNeeded() const {
	if (_catalogLoaded)
		return;

	_catalogLoaded = true;
	addBuiltinCatalog();
	addBuiltinRules();
	loadBundledCatalog();
	loadExternalCatalog();
}

void CEAchievementService::addBuiltinCatalog() const {
	struct BuiltinDefinition {
		const char *id;
		const char *title;
		const char *description;
	};

	static const BuiltinDefinition operationStealth[] = {
		{ "passport_to_trouble", "Passport to Trouble", "Get the customs agent to accept the correct passport." },
		{ "shave_and_a_briefing", "Shave and a Briefing", "Hear the razor recording." },
		{ "say_it_with_flowers", "Say It with Flowers", "Meet the park contact and escape with his information." },
		{ "safe_deposit_unsafe_withdrawal", "Safe Deposit, Unsafe Withdrawal", "Retrieve the bank device and reach the ambush." },
		{ "escape_clause", "Escape Clause", "Escape the mine and flooded tunnels." },
		{ "two_for_the_surface", "Two for the Surface", "Rescue Julia underwater and surface together." },
		{ "palace_intrigue", "Palace Intrigue", "Reach the office through the palace mazes." },
		{ "a_safe_bet", "A Safe Bet", "Open the palace safe and recover its envelope." },
		{ "making_waves", "Making Waves", "Complete the entire jet-ski chase." },
		{ "under_new_management", "Under New Management", "Enter the hidden underwater base." },
		{ "pen_mightier_than_piranhas", "The Pen Is Mightier Than the Piranhas", "Escape the cage using the pen and watch." },
		{ "rats_all_folks", "Rats All, Folks!", "Clear the rat mazes inside the hidden underwater base." },
		{ "dressed_to_infiltrate", "Dressed to Infiltrate", "Acquire the soldier disguise." },
		{ "fingerprint_fiction", "Fingerprint Fiction", "" },
		{ "officially_unofficial", "Officially Unofficial", "" },
		{ "virus_successfully_installed", "Virus Successfully Installed", "" },
		{ "order_of_the_banana", "Order of the Banana", "" },
		{ "well_read_intruder", "Well-Read Intruder", "Read all five humorous palace book titles." },
		{ "corporate_espionage", "Corporate Espionage", "" },
		{ nullptr, nullptr, nullptr }
	};

	for (const BuiltinDefinition *i = operationStealth; i->id; ++i) {
		CEAchievementDefinition definition;
		definition.game = "operation-stealth";
		definition.variant = "*";
		definition.id = i->id;
		definition.title = i->title;
		definition.description = i->description;
		definition.challengeEligible = true;
		definition.hidden = false;
		addCatalogDefinition(definition);
	}
}

void CEAchievementService::addBuiltinRules() const {
	CEAchievementRule rule;
	rule.game = "operation-stealth";
	rule.variant = "*";
	rule.event = "customs_passport_accepted";
	rule.achievement = "passport_to_trouble";
	addCatalogRule(rule);

	rule.event = "razor_recording_heard";
	rule.achievement = "shave_and_a_briefing";
	addCatalogRule(rule);

	rule.event = "park_contact_info_obtained";
	rule.achievement = "say_it_with_flowers";
	addCatalogRule(rule);

	rule.event = "bank_ambush_reached";
	rule.achievement = "safe_deposit_unsafe_withdrawal";
	addCatalogRule(rule);

	rule.event = "mine_tunnels_escaped";
	rule.achievement = "escape_clause";
	addCatalogRule(rule);

	rule.event = "julia_rescued_underwater";
	rule.achievement = "two_for_the_surface";
	addCatalogRule(rule);

	rule.event = "palace_office_reached";
	rule.achievement = "palace_intrigue";
	addCatalogRule(rule);

	rule.event = "palace_safe_envelope_recovered";
	rule.achievement = "a_safe_bet";
	addCatalogRule(rule);

	rule.event = "jetski_chase_completed";
	rule.achievement = "making_waves";
	addCatalogRule(rule);

	rule.event = "underwater_base_entered";
	rule.achievement = "under_new_management";
	addCatalogRule(rule);

	rule.event = "piranha_cage_escaped";
	rule.achievement = "pen_mightier_than_piranhas";
	addCatalogRule(rule);

	rule.event = "rat_mazes_cleared";
	rule.achievement = "rats_all_folks";
	addCatalogRule(rule);

	rule.event = "soldier_disguise_acquired";
	rule.achievement = "dressed_to_infiltrate";
	addCatalogRule(rule);

	rule.event = "fingerprint_door_passed";
	rule.achievement = "fingerprint_fiction";
	addCatalogRule(rule);

	rule.event = "authorized_mission_submitted";
	rule.achievement = "officially_unofficial";
	addCatalogRule(rule);

	rule.event = "stealth_virus_installed";
	rule.achievement = "virus_successfully_installed";
	addCatalogRule(rule);

	rule.event = "banana_order_awarded";
	rule.achievement = "order_of_the_banana";
	addCatalogRule(rule);

	rule.event = "humorous_book_read";
	rule.achievement = "well_read_intruder";
	addCatalogRule(rule);

	rule.event = "mission_order_read";
	rule.achievement = "corporate_espionage";
	addCatalogRule(rule);
}

void CEAchievementService::loadBundledCatalog() const {
	Common::Archive *pack = nullptr;

	if (!pack && ConfMan.hasKey("extrapath")) {
		Common::FSDirectory extrapath(ConfMan.getPath("extrapath"));
		pack = Common::makeZipArchive(extrapath.createReadStreamForMember(kCEDataFile));
	}

	if (!pack)
		pack = Common::makeZipArchive(kCEDataFile);

	if (!pack)
		return;

	Common::SeekableReadStream *manifest = pack->createReadStreamForMember("manifest.json");
	if (!manifest) {
		delete pack;
		warning("manifest.json is not found in %s", kCEDataFile);
		return;
	}

	String data = manifest->readString(0, manifest->size());
	delete manifest;

	JSONValue *root = JSON::parse(data);
	if (!root || !root->isObject()) {
		delete root;
		delete pack;
		warning("manifest.json in %s is not valid JSON", kCEDataFile);
		return;
	}

	const JSONObject &obj = root->asObject();
	if (!obj.contains("games") || !obj["games"]->isArray()) {
		delete root;
		delete pack;
		warning("manifest.json in %s does not contain games", kCEDataFile);
		return;
	}

	const JSONArray &games = obj["games"]->asArray();
	for (uint i = 0; i < games.size(); ++i) {
		if (!games[i]->isObject())
			continue;
		const JSONObject &game = games[i]->asObject();
		const String gameKey = ceStringFromJson(game, "key");
		if (gameKey.empty() || !game.contains("achievements") || !game["achievements"]->isArray())
			continue;

		const JSONArray &achievements = game["achievements"]->asArray();
		for (uint j = 0; j < achievements.size(); ++j) {
			if (!achievements[j]->isObject())
				continue;
			CEAchievementDefinition definition;
			if (ceParsePackAchievement(gameKey, achievements[j]->asObject(), definition))
				addCatalogDefinition(definition);
		}
	}

	delete root;
	delete pack;
}

void CEAchievementService::loadExternalCatalog() const {
	Common::InSaveFile *in = g_system->getSavefileManager()->openForLoading(kCECatalogFile);
	if (!in)
		return;

	String data = in->readString(0, in->size());
	delete in;

	JSONValue *root = JSON::parse(data);
	if (!root) {
		warning("CE achievements catalog '%s' is not valid JSON", kCECatalogFile);
		return;
	}

	const JSONArray *definitions = nullptr;
	const JSONArray *rules = nullptr;
	if (root->isArray()) {
		definitions = &root->asArray();
	} else if (root->isObject()) {
		const JSONObject &obj = root->asObject();
		if (obj.contains("achievements") && obj["achievements"]->isArray())
			definitions = &obj["achievements"]->asArray();
		if (obj.contains("rules") && obj["rules"]->isArray())
			rules = &obj["rules"]->asArray();
	}

	if (!definitions && !rules) {
		warning("CE achievements catalog '%s' must be an array or contain achievements and/or rules arrays", kCECatalogFile);
		delete root;
		return;
	}

	if (definitions) {
		for (uint i = 0; i < definitions->size(); ++i) {
			if (!(*definitions)[i]->isObject())
				continue;
			CEAchievementDefinition definition;
			if (ceParseCatalogDefinition((*definitions)[i]->asObject(), definition))
				addCatalogDefinition(definition);
		}
	}

	if (rules) {
		for (uint i = 0; i < rules->size(); ++i) {
			if (!(*rules)[i]->isObject())
				continue;
			CEAchievementRule rule;
			if (ceParseCatalogRule((*rules)[i]->asObject(), rule))
				addCatalogRule(rule);
		}
	}
	delete root;
}

void CEAchievementService::addCatalogDefinition(const CEAchievementDefinition &definition) const {
	for (uint i = 0; i < _catalog.size(); ++i) {
		if (_catalog[i].game == definition.game && _catalog[i].variant == definition.variant && _catalog[i].id == definition.id) {
			_catalog[i] = definition;
			return;
		}
	}
	_catalog.push_back(definition);
}

const CEAchievementDefinition *CEAchievementService::findDefinition(const String &game, const String &variant, const String &achievement) const {
	loadCatalogIfNeeded();

	const CEAchievementDefinition *fallback = nullptr;
	for (uint i = 0; i < _catalog.size(); ++i) {
		if (_catalog[i].game != game || _catalog[i].id != achievement)
			continue;
		if (_catalog[i].variant == variant)
			return &_catalog[i];
		if (_catalog[i].variant.empty() || _catalog[i].variant == "*")
			fallback = &_catalog[i];
	}
	return fallback;
}

void CEAchievementService::addCatalogRule(const CEAchievementRule &rule) const {
	for (uint i = 0; i < _rules.size(); ++i) {
		if (_rules[i].game == rule.game && _rules[i].variant == rule.variant && _rules[i].event == rule.event) {
			_rules[i] = rule;
			return;
		}
	}
	_rules.push_back(rule);
}

const CEAchievementRule *CEAchievementService::findRule(const String &game, const String &variant, const String &event) const {
	loadCatalogIfNeeded();

	const CEAchievementRule *fallback = nullptr;
	for (uint i = 0; i < _rules.size(); ++i) {
		if (_rules[i].game != game || _rules[i].event != event)
			continue;
		if (_rules[i].variant == variant)
			return &_rules[i];
		if (_rules[i].variant.empty() || _rules[i].variant == "*")
			fallback = &_rules[i];
	}
	return fallback;
}

String CEAchievementService::getAchievementId(const String &game, uint index) const {
	loadCatalogIfNeeded();

	uint matched = 0;
	for (uint i = 0; i < _catalog.size(); ++i) {
		if (_catalog[i].game != game)
			continue;
		if (!_catalog[i].variant.empty() && _catalog[i].variant != "*")
			continue;
		if (matched == index)
			return _catalog[i].id;
		++matched;
	}
	return String();
}

String CEAchievementService::getAchievementTitle(const String &game, const String &variant, const String &achievement) const {
	const CEAchievementDefinition *definition = findDefinition(game, variant, achievement);
	return definition ? definition->title : ceAchievementDisplayName(achievement);
}

bool CEAchievementService::isChallengeModeEnabled() const {
	return ConfMan.getBool(kCEChallengeModeKey, ConfigManager::kApplicationDomain);
}

void CEAchievementService::setChallengeModeEnabled(bool enabled) {
	if (enabled) {
		for (ConfigManager::DomainMap::iterator d = ConfMan.beginGameDomains(); d != ConfMan.endGameDomains(); ++d)
			ConfMan.setBool(kCECheatsEnabledKey, false, d->_key);
	}
	ConfMan.setBool(kCEChallengeModeKey, enabled, ConfigManager::kApplicationDomain);
	ConfMan.flushToDisk();
}

bool CEAchievementService::areCheatsEnabled(const String &domain) const {
	if (isChallengeModeEnabled())
		return false;

	const String activeDomain = domain.empty() ? ConfMan.getActiveDomainName() : domain;
	return !activeDomain.empty() && ConfMan.hasKey(kCECheatsEnabledKey, activeDomain) &&
		ConfMan.getBool(kCECheatsEnabledKey, activeDomain);
}

void CEAchievementService::setCheatsEnabled(bool enabled, const String &domain) {
	const String activeDomain = domain.empty() ? ConfMan.getActiveDomainName() : domain;
	if (activeDomain.empty())
		return;

	if (enabled && isChallengeModeEnabled())
		setChallengeModeEnabled(false);

	ConfMan.setBool(kCECheatsEnabledKey, enabled, activeDomain);
	ConfMan.flushToDisk();
}

bool CEAchievementService::shouldApplyCheat(const String &key, const String &domain) const {
	const String activeDomain = domain.empty() ? ConfMan.getActiveDomainName() : domain;
	return areCheatsEnabled(activeDomain) && ConfMan.hasKey(key, activeDomain) &&
		ConfMan.getBool(key, activeDomain);
}

bool CEAchievementService::isRepeatPopupsEnabled() const {
	return ConfMan.getBool(kCERepeatPopupsKey, ConfigManager::kApplicationDomain);
}

void CEAchievementService::setRepeatPopupsEnabled(bool enabled) {
	ConfMan.setBool(kCERepeatPopupsKey, enabled, ConfigManager::kApplicationDomain);
	ConfMan.flushToDisk();
}

bool CEAchievementService::isDisqualifyingAssistanceActive(const String &domain) const {
	return areCheatsEnabled(domain);
}

void CEAchievementService::disableDisqualifyingAssistance() {
	for (ConfigManager::DomainMap::iterator d = ConfMan.beginGameDomains(); d != ConfMan.endGameDomains(); ++d) {
		ConfMan.setBool(kCECheatsEnabledKey, false, d->_key);
	}
	ConfMan.flushToDisk();
}

void CEAchievementService::noteDisqualifyingAssistance(const String &flag) {
	String flags = ConfMan.hasKey("ce_achievements_assistance_flags", ConfigManager::kApplicationDomain) ?
		ConfMan.get("ce_achievements_assistance_flags", ConfigManager::kApplicationDomain) : String();
	if (!flags.contains(flag)) {
		if (!flags.empty())
			flags += ",";
		flags += flag;
		ConfMan.set("ce_achievements_assistance_flags", flags, ConfigManager::kApplicationDomain);
		ConfMan.flushToDisk();
	}
}

Array<String> CEAchievementService::getAssistanceFlags(const String &domain) const {
	Array<String> flags;
	if (areCheatsEnabled(domain))
		flags.push_back("cheats.enabled");

	String stored = ConfMan.hasKey("ce_achievements_assistance_flags", ConfigManager::kApplicationDomain) ?
		ConfMan.get("ce_achievements_assistance_flags", ConfigManager::kApplicationDomain) : String();
	while (!stored.empty()) {
		int comma = stored.find(',');
		String flag = comma >= 0 ? stored.substr(0, comma) : stored;
		if (!flag.empty())
			flags.push_back(flag);
		if (comma < 0)
			break;
		stored = stored.substr(comma + 1);
	}
	return flags;
}

String CEAchievementService::getLinkToken() const {
	return ConfMan.hasKey(kCELinkTokenKey, ConfigManager::kApplicationDomain) ?
		ConfMan.get(kCELinkTokenKey, ConfigManager::kApplicationDomain) : String();
}

void CEAchievementService::setLinkToken(const String &token) {
	if (token.empty())
		ConfMan.removeKey(kCELinkTokenKey, ConfigManager::kApplicationDomain);
	else
		ConfMan.set(kCELinkTokenKey, token, ConfigManager::kApplicationDomain);
	ConfMan.flushToDisk();
}

String CEAchievementService::currentTimeString() const {
	TimeDate td;
	g_system->getTimeAndDate(td, true);
	return String::format("%04d-%02d-%02dT%02d:%02d:%02dZ",
		td.tm_year + 1900, td.tm_mon + 1, td.tm_mday, td.tm_hour, td.tm_min, td.tm_sec);
}

String CEAchievementService::getOrCreateInstallSecret() const {
	if (ConfMan.hasKey(kCEInstallSecretKey, ConfigManager::kApplicationDomain))
		return ConfMan.get(kCEInstallSecretKey, ConfigManager::kApplicationDomain);

	String secret = String::format("%s-%u-%u", currentTimeString().c_str(), g_system->getMillis(true), (uint32)(uintptr)g_system);
	ConfMan.set(kCEInstallSecretKey, ceHmacSha256(secret, secret), ConfigManager::kApplicationDomain);
	ConfMan.flushToDisk();
	return ConfMan.get(kCEInstallSecretKey, ConfigManager::kApplicationDomain);
}

String CEAchievementService::canonicalizeEvent(const CEAchievementEvent &event) const {
	String flags = "[";
	for (uint i = 0; i < event.assistanceFlags.size(); ++i) {
		if (i)
			flags += ",";
		flags += ceEscapeJson(event.assistanceFlags[i]);
	}
	flags += "]";
	return String::format("{\"achievement\":%s,\"assistance_flags\":%s,\"assisted\":%s,\"event_id\":%s,\"game\":%s,\"history_status\":%s,\"occurred_at\":%s,\"variant\":%s}",
		ceEscapeJson(event.achievement).c_str(), flags.c_str(), event.assisted ? "true" : "false",
		ceEscapeJson(event.eventId).c_str(), ceEscapeJson(event.game).c_str(),
		ceEscapeJson(event.historyStatus).c_str(), ceEscapeJson(event.occurredAt).c_str(),
		ceEscapeJson(event.variant).c_str());
}

String CEAchievementService::signEvent(const CEAchievementEvent &event) const {
	return ceHmacSha256(getOrCreateInstallSecret(), canonicalizeEvent(event));
}

bool CEAchievementService::verifyEventSignature(const CEAchievementEvent &event) const {
	return event.signature == signEvent(event);
}

String CEAchievementService::makeEventId(const String &game, const String &variant, const String &achievement, bool assisted) const {
	String profile = getOrCreateInstallSecret().substr(0, 16);
	return String::format("%s:%s:%s:%s:%s", profile.c_str(), game.c_str(), variant.c_str(), achievement.c_str(), assisted ? "completion" : "challenge");
}

Array<CEAchievementEvent> CEAchievementService::loadQueue() const {
	Array<CEAchievementEvent> events;
	Common::InSaveFile *in = g_system->getSavefileManager()->openForLoading(kCEQueueFile);
	if (!in)
		return events;

	String data = in->readString(0, in->size());
	delete in;

	JSONValue *root = JSON::parse(data);
	if (!root || !root->isArray()) {
		delete root;
		return events;
	}

	const JSONArray &array = root->asArray();
	for (uint i = 0; i < array.size(); ++i) {
		if (!array[i]->isObject())
			continue;
		const JSONObject &obj = array[i]->asObject();
		CEAchievementEvent event;
		event.eventId = ceStringFromJson(obj, "event_id");
		event.game = ceStringFromJson(obj, "game");
		event.variant = ceStringFromJson(obj, "variant");
		event.achievement = ceStringFromJson(obj, "achievement");
		event.assisted = ceBoolFromJson(obj, "assisted");
		event.historyStatus = ceStringFromJson(obj, "history_status");
		event.occurredAt = ceStringFromJson(obj, "occurred_at");
		event.attemptCount = ceUIntFromJson(obj, "attempt_count");
		event.lastAttemptAt = ceStringFromJson(obj, "last_attempt_at");
		event.lastError = ceStringFromJson(obj, "last_error");
		event.state = ceStringFromJson(obj, "state", "pending");
		event.signature = ceStringFromJson(obj, "signature");
		if (obj.contains("assistance_flags") && obj["assistance_flags"]->isArray()) {
			const JSONArray &flags = obj["assistance_flags"]->asArray();
			for (uint f = 0; f < flags.size(); ++f) {
				if (flags[f]->isString())
					event.assistanceFlags.push_back(flags[f]->asString());
			}
		}
		if (!event.eventId.empty())
			events.push_back(event);
	}
	delete root;
	return events;
}

bool CEAchievementService::saveQueue(const Array<CEAchievementEvent> &events) const {
	Common::OutSaveFile *out = g_system->getSavefileManager()->openForSaving(kCEQueueFile, false);
	if (!out)
		return false;

	String json = "[";
	for (uint i = 0; i < events.size(); ++i) {
		const CEAchievementEvent &event = events[i];
		if (i)
			json += ",";
		String flags = "[";
		for (uint f = 0; f < event.assistanceFlags.size(); ++f) {
			if (f)
				flags += ",";
			flags += ceEscapeJson(event.assistanceFlags[f]);
		}
		flags += "]";
		json += String::format("{\"event_id\":%s,\"game\":%s,\"variant\":%s,\"achievement\":%s,\"assisted\":%s,\"history_status\":%s,\"assistance_flags\":%s,\"occurred_at\":%s,\"attempt_count\":%u,\"last_attempt_at\":%s,\"last_error\":%s,\"state\":%s,\"signature\":%s}",
			ceEscapeJson(event.eventId).c_str(), ceEscapeJson(event.game).c_str(),
			ceEscapeJson(event.variant).c_str(), ceEscapeJson(event.achievement).c_str(),
			event.assisted ? "true" : "false", ceEscapeJson(event.historyStatus).c_str(),
			flags.c_str(), ceEscapeJson(event.occurredAt).c_str(), event.attemptCount,
			ceEscapeJson(event.lastAttemptAt).c_str(), ceEscapeJson(event.lastError).c_str(),
			ceEscapeJson(event.state).c_str(), ceEscapeJson(event.signature).c_str());
	}
	json += "]";
	out->writeString(json);
	out->finalize();
	delete out;
	return true;
}

bool CEAchievementService::queueAchievementEvent(const String &game, const String &variant, const String &achievement) {
	const String activeDomain = ConfMan.getActiveDomainName();
	const bool assisted = !isChallengeModeEnabled() || isDisqualifyingAssistanceActive(activeDomain);
	CEAchievementEvent event;
	event.game = game;
	event.variant = variant;
	event.achievement = achievement;
	event.assisted = assisted;
	event.historyStatus = assisted ? "assisted" : "verified";
	event.assistanceFlags = getAssistanceFlags(activeDomain);
	event.occurredAt = currentTimeString();
	event.attemptCount = 0;
	event.state = "pending";
	event.eventId = makeEventId(game, variant, achievement, assisted);
	event.signature = signEvent(event);

	Array<CEAchievementEvent> events = loadQueue();
	for (uint i = 0; i < events.size(); ++i) {
		if (events[i].eventId == event.eventId)
			return false;
	}
	events.push_back(event);
	return saveQueue(events);
}

uint32 CEAchievementService::getPendingEventCount() const {
	uint32 count = 0;
	Array<CEAchievementEvent> events = loadQueue();
	for (uint i = 0; i < events.size(); ++i) {
		if (events[i].state != "synced" && events[i].state != "permanent_failure")
			++count;
	}
	return count;
}

String CEAchievementService::getLastSyncStatus() const {
	return ConfMan.hasKey(kCELastSyncStatusKey, ConfigManager::kApplicationDomain) ?
		ConfMan.get(kCELastSyncStatusKey, ConfigManager::kApplicationDomain) : String("Not synced yet");
}

void CEAchievementService::finishRequest(const String &status) {
	ConfMan.set(kCELastSyncStatusKey, status, ConfigManager::kApplicationDomain);
	ConfMan.flushToDisk();
	_syncAction = kCEAchievementSyncIdle;
	_syncQueueIndex = -1;
}

void CEAchievementService::startRequest(CEAchievementSyncAction action, const String &url, const String &body, int queueIndex) {
#ifdef USE_HTTP
	String token = getLinkToken();
	if (token.empty()) {
		finishRequest("CE Link token is not configured.");
		return;
	}
	if (_syncAction != kCEAchievementSyncIdle)
		return;

	CEHttpRequest *request = new CEHttpRequest(this, url);
	request->addHeader("Accept: application/json");
	request->addHeader(String::format("Authorization: Bearer %s", token.c_str()));
	if (!body.empty()) {
		request->addHeader("Content-Type: application/json");
		byte *buffer = new byte[body.size()];
		memcpy(buffer, body.c_str(), body.size());
		request->setBuffer(buffer, body.size());
	}
	_syncAction = action;
	_syncQueueIndex = queueIndex;
	ConnMan.addRequest(request);
#else
	finishRequest("HTTP support is not available in this build.");
#endif
}

void CEAchievementService::testLinkToken() {
	startRequest(kCEAchievementSyncTestToken, String(kCEApiBaseUrl) + "/link");
}

void CEAchievementService::retryQueuedEvents() {
	submitNextQueuedEvent();
}

void CEAchievementService::submitNextQueuedEvent() {
	if (_syncAction != kCEAchievementSyncIdle)
		return;
	Array<CEAchievementEvent> events = loadQueue();
	for (uint i = 0; i < events.size(); ++i) {
		if (events[i].state == "synced" || events[i].state == "permanent_failure")
			continue;
		if (!verifyEventSignature(events[i])) {
			events[i].assisted = true;
			events[i].historyStatus = "assisted";
			events[i].assistanceFlags.push_back("queue_signature_mismatch");
			events[i].eventId = makeEventId(events[i].game, events[i].variant, events[i].achievement, true);
			events[i].lastError = "Queue signature mismatch; downgraded to assisted.";
			events[i].signature = signEvent(events[i]);
			saveQueue(events);
		}

		events[i].attemptCount++;
		events[i].lastAttemptAt = currentTimeString();
		saveQueue(events);
		startRequest(kCEAchievementSyncQueue, String(kCEApiBaseUrl) + "/events", canonicalizeEvent(events[i]), i);
		return;
	}
	finishRequest("Achievement queue is empty.");
}

void CEAchievementService::requestSucceeded(const String &response, long httpCode) {
	if (_syncAction == kCEAchievementSyncTestToken) {
		if (httpCode == 200)
			finishRequest("CE Link token is valid.");
		else
			finishRequest(String::format("CE Link token test failed (HTTP %ld).", httpCode));
		return;
	}

	if (_syncAction != kCEAchievementSyncQueue)
		return;

	Array<CEAchievementEvent> events = loadQueue();
	if (_syncQueueIndex >= 0 && _syncQueueIndex < (int)events.size()) {
		if (httpCode == 200 || httpCode == 201) {
			events[_syncQueueIndex].state = "synced";
			events[_syncQueueIndex].lastError.clear();
			saveQueue(events);
			finishRequest("Achievement event synced.");
			submitNextQueuedEvent();
			return;
		}

		events[_syncQueueIndex].lastError = response.empty() ? String::format("HTTP %ld", httpCode) : response;
		if (httpCode == 401 || httpCode == 409 || httpCode == 422)
			events[_syncQueueIndex].state = "permanent_failure";
		saveQueue(events);
	}
	finishRequest(String::format("Achievement sync failed (HTTP %ld).", httpCode));
}

void CEAchievementService::requestFailed(const String &response, long httpCode) {
	if (_syncAction == kCEAchievementSyncTestToken) {
		if (httpCode < 0 && !response.empty())
			finishRequest(String::format("CE Link token test failed: %s", response.c_str()));
		else
		finishRequest(String::format("CE Link token test failed (HTTP %ld).", httpCode));
		return;
	}
	Array<CEAchievementEvent> events = loadQueue();
	if (_syncQueueIndex >= 0 && _syncQueueIndex < (int)events.size()) {
		events[_syncQueueIndex].lastError = response.empty() ? String::format("HTTP %ld", httpCode) : response;
		if (httpCode == 401 || httpCode == 409 || httpCode == 422)
			events[_syncQueueIndex].state = "permanent_failure";
		saveQueue(events);
	}
	if (httpCode < 0 && !response.empty())
		finishRequest(String::format("Achievement sync failed: %s", response.c_str()));
	else
		finishRequest(String::format("Achievement sync failed (HTTP %ld).", httpCode));
}

const char *const *CEAchievementService::getOperationStealthAchievementKeys() const {
	return kCEOperationStealthKeys;
}

} // End of namespace Common
