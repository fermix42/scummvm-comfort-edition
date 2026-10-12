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

#include "audio/audiostream.h"
#include "audio/decoders/wave.h"
#include "audio/mixer.h"

#include "common/compression/unzip.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/formats/json.h"
#include "common/fs.h"
#include "common/osd_message_queue.h"
#include "common/savefile.h"
#include "common/system.h"
#include "common/translation.h"
#include "common/util.h"
#include "engines/engine.h"
#include "graphics/font.h"
#include "graphics/fontman.h"
#include "graphics/surface.h"
#include "image/png.h"

#ifdef USE_HTTP
#include "backends/networking/http/connectionmanager.h"
#include "backends/networking/http/httprequest.h"
#include "backends/networking/http/networkreadstream.h"
#endif

namespace Common {

static const char *const kCEApiBaseUrl = "https://achievements.divineramen.net/api/v1";
static const char *const kCEDataFile = "ce-achievements.dat";
static const char *const kCEQueueFile = "ce-achievements-queue.json";
static const char *const kCEPlaythroughRegistryFile = "ce-playthroughs.json";
static const char *const kCECatalogFile = "ce-achievements-catalog.json";
static const char *const kCEApiBaseUrlKey = "ce_achievements_api_base_url";
static const char *const kCELinkTokenKey = "ce_link_token";
static const char *const kCEChallengeModeKey = "ce_achievements_challenge_mode";
static const char *const kCECheatsEnabledKey = "ce_cheats_enabled";
static const char *const kCEPopupsEnabledKey = "ce_achievements_popups_enabled";
static const char *const kCEPopupLocationKey = "ce_achievements_popup_location";
static const char *const kCERepeatPopupsKey = "ce_achievements_repeat_popups";
static const char *const kCEPopupSoundEnabledKey = "ce_achievements_popup_sound_enabled";
static const char *const kCEInstallSecretKey = "ce_achievements_install_secret";
static const char *const kCELastSyncStatusKey = "ce_achievements_last_sync_status";
static const char *const kCEActivePlaythroughTargetKey = "ce_active_playthrough_target";
static const char *const kCEActivePlaythroughGameKey = "ce_active_playthrough_game";
static const char *const kCEActivePlaythroughVariantKey = "ce_active_playthrough_variant";
static const char *const kCEActiveClientPlaythroughIdKey = "ce_active_client_playthrough_id";
static const char *const kCEActivePlaythroughIdKey = "ce_active_playthrough_id";
static const char *const kCEActivePlaythroughStatusKey = "ce_active_playthrough_status";
static const char *const kCEActivePlaythroughCasualKey = "ce_active_playthrough_casual";
static const char *const kCEActivePlaythroughSyncEligibleKey = "ce_active_playthrough_sync_eligible";
static const char *const kCEPlaythroughEventCounterKey = "ce_playthrough_event_counter";
static const char *const kCEAchievementSoundMember = "sounds/achievement-sultry-clean.wav";
static const uint32 kCEAchievementPopupDuration = 5 * 1000;

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

static Common::Archive *ceOpenDataPack() {
	Common::Archive *pack = nullptr;

	if (!pack && ConfMan.hasKey("extrapath")) {
		Common::FSDirectory extrapath(ConfMan.getPath("extrapath"));
		pack = Common::makeZipArchive(extrapath.createReadStreamForMember(kCEDataFile));
	}

	if (!pack)
		pack = Common::makeZipArchive(kCEDataFile);

	return pack;
}

static bool ceAchievementPopupAllowed() {
	return CEAchievements().arePopupsEnabled() && !ConfMan.getBool("disable_achievement_unlocked_osd") && g_system;
}

static const char *cePopupLocationToConfig(CEAchievementPopupLocation location) {
	switch (location) {
	case kCEAchievementPopupUpperLeft:
		return "upper_left";
	case kCEAchievementPopupUpperRight:
		return "upper_right";
	case kCEAchievementPopupTopCenter:
		return "top_center";
	case kCEAchievementPopupBottomCenter:
		return "bottom_center";
	case kCEAchievementPopupLowerRight:
	default:
		return "lower_right";
	}
}

static CEAchievementPopupLocation cePopupLocationFromConfig(const String &location) {
	if (location == "upper_left")
		return kCEAchievementPopupUpperLeft;
	if (location == "upper_right")
		return kCEAchievementPopupUpperRight;
	if (location == "top_center")
		return kCEAchievementPopupTopCenter;
	if (location == "bottom_center")
		return kCEAchievementPopupBottomCenter;
	return kCEAchievementPopupLowerRight;
}

static void ceBlitScaled(const Graphics::Surface &src, Graphics::Surface &dst, int dstX, int dstY, int dstW, int dstH) {
	if (!src.getPixels() || !dst.getPixels() || dstW <= 0 || dstH <= 0)
		return;

	for (int y = 0; y < dstH; ++y) {
		const int srcY = y * src.h / dstH;
		for (int x = 0; x < dstW; ++x) {
			const int srcX = x * src.w / dstW;
			uint32 color = 0;
			const byte *srcPixel = (const byte *)src.getBasePtr(srcX, srcY);
			if (src.format.bytesPerPixel == 2)
				color = READ_UINT16(srcPixel);
			else if (src.format.bytesPerPixel == 3)
				color = READ_UINT24(srcPixel);
			else if (src.format.bytesPerPixel == 4)
				color = READ_UINT32(srcPixel);

			byte a, r, g, b;
			src.format.colorToARGB(color, a, r, g, b);
			if (a == 0)
				continue;
			dst.setPixel(dstX + x, dstY + y, dst.format.ARGBToColor(a, r, g, b));
		}
	}
}

static Graphics::Surface *ceLoadAchievementIcon(Common::Archive *pack, const CEAchievementDefinition *definition) {
	if (!pack || !definition || definition->iconPath.empty())
		return nullptr;

	Common::SeekableReadStream *stream = pack->createReadStreamForMember(Common::Path(definition->iconPath));
	if (!stream)
		return nullptr;

	Image::PNGDecoder decoder;
	const bool loaded = decoder.loadStream(*stream);
	delete stream;
	if (!loaded || !decoder.getSurface())
		return nullptr;

	return decoder.getSurface()->convertTo(Graphics::PixelFormat::createFormatRGBA32(),
		decoder.getPalette().data(), decoder.getPalette().size());
}

static Common::Rect ceAchievementCardRect(int screenW, int screenH, int cardW, int cardH) {
	const int margin = 14;
	switch (CEAchievements().getPopupLocation()) {
	case kCEAchievementPopupUpperLeft:
		return Common::Rect(margin, margin, margin + cardW, margin + cardH);
	case kCEAchievementPopupUpperRight:
		return Common::Rect(screenW - cardW - margin, margin, screenW - margin, margin + cardH);
	case kCEAchievementPopupTopCenter:
		return Common::Rect((screenW - cardW) / 2, margin, (screenW + cardW) / 2, margin + cardH);
	case kCEAchievementPopupBottomCenter:
		return Common::Rect((screenW - cardW) / 2, screenH - cardH - margin, (screenW + cardW) / 2, screenH - margin);
	case kCEAchievementPopupLowerRight:
	default:
		return Common::Rect(screenW - cardW - margin, screenH - cardH - margin, screenW - margin, screenH - margin);
	}
}

static bool ceDisplayAchievementUnlockedOSD(const String &title, const CEAchievementDefinition *definition, bool challengeStyle) {
	if (!ceAchievementPopupAllowed())
		return false;

	Common::Archive *pack = ceOpenDataPack();
	Graphics::Surface *icon = ceLoadAchievementIcon(pack, definition);
	delete pack;

	const int screenW = g_system->getOverlayWidth();
	const int screenH = g_system->getOverlayHeight();
	const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kLocalizedFont);
	if (icon && screenW > 0 && screenH > 0 && font) {
		const Graphics::PixelFormat format = Graphics::PixelFormat::createFormatRGBA32();
		Graphics::Surface popup;
		popup.create(screenW, screenH, format);
		popup.fillRect(Common::Rect(0, 0, screenW, screenH), format.ARGBToColor(0, 0, 0, 0));

		const int iconSize = 48;
		const Common::U32String titleText(title);
		const int cardPaddingW = 18 + iconSize + 14 + 14;
		const int availableCardW = MAX(1, screenW - 12);
		const int desiredCardW = cardPaddingW + font->getStringWidth(titleText);
		const int cardW = MIN(availableCardW, MAX(250, MIN(420, desiredCardW)));
		const int cardH = 72;
		const Common::Rect card = ceAchievementCardRect(screenW, screenH, cardW, cardH);
		const uint32 bg = format.ARGBToColor(220, 24, 24, 28);
		const uint32 border = format.ARGBToColor(238, 236, 236, 236);
		const uint32 accent = challengeStyle ? format.ARGBToColor(255, 232, 184, 64) : format.ARGBToColor(255, 64, 176, 220);
		const uint32 text = format.ARGBToColor(255, 255, 255, 255);

		popup.fillRect(card, bg);
		popup.fillRect(Common::Rect(card.left, card.top, card.right, card.top + 2), border);
		popup.fillRect(Common::Rect(card.left, card.bottom - 2, card.right, card.bottom), border);
		popup.fillRect(Common::Rect(card.left, card.top, card.left + 2, card.bottom), border);
		popup.fillRect(Common::Rect(card.right - 2, card.top, card.right, card.bottom), border);
		popup.fillRect(Common::Rect(card.left, card.top, card.left + 6, card.bottom), accent);

		const int iconX = card.left + 18;
		const int iconY = card.top + (card.height() - iconSize) / 2;
		ceBlitScaled(*icon, popup, iconX, iconY, iconSize, iconSize);

		const int titleX = iconX + iconSize + 14;
		const int titleY = card.top + (card.height() - font->getFontHeight()) / 2;
		const int titleW = MAX(1, card.right - titleX - 14);
		font->drawString(&popup, titleText, titleX, titleY, titleW, text, Graphics::kTextAlignLeft, 0, true);

		Common::OSDMessageQueue::instance().addImage(&popup, kCEAchievementPopupDuration);
		popup.free();
		icon->free();
		delete icon;
		return true;
	}

	if (icon) {
		icon->free();
		delete icon;
	}

	U32String msg = Common::U32String::format("%S\n%S",
		_("Achievement unlocked!").c_str(),
		Common::U32String(title).c_str()
	);
	g_system->displayMessageOnOSD(msg);
	return true;
}

static void cePlayAchievementUnlockedSound() {
	if (!CEAchievements().isPopupSoundEnabled() || !ceAchievementPopupAllowed() || !g_system->getMixer())
		return;

	Common::Archive *pack = ceOpenDataPack();
	if (!pack)
		return;

	Common::SeekableReadStream *stream = pack->createReadStreamForMember(kCEAchievementSoundMember);
	delete pack;

	if (!stream) {
		warning("CE achievement sound '%s' is missing from '%s'", kCEAchievementSoundMember, kCEDataFile);
		return;
	}

	Audio::AudioStream *audio = Audio::makeWAVStream(stream, DisposeAfterUse::YES);
	if (!audio) {
		delete stream;
		warning("Could not decode CE achievement sound '%s'", kCEAchievementSoundMember);
		return;
	}

	Audio::SoundHandle handle;
	g_system->getMixer()->playStream(Audio::Mixer::kSFXSoundType, &handle, audio, -1,
		Audio::Mixer::kMaxChannelVolume, 0, DisposeAfterUse::YES);
}

static String ceEscapeJson(const String &input) {
	JSONValue value(input);
	return JSON::stringify(&value);
}

static String ceStringArrayJson(const Array<String> &strings) {
	String json = "[";
	for (uint i = 0; i < strings.size(); ++i) {
		if (i)
			json += ",";
		json += ceEscapeJson(strings[i]);
	}
	json += "]";
	return json;
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
	definition.iconPath = ceStringFromJson(obj, "icon");
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
	definition.iconPath = ceStringFromJson(obj, "icon");
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

static String ceShortHash(const String &input) {
	Array<byte> bytes;
	ceAppendStringBytes(bytes, input);
	return ceHexDigest(ceSha256(bytes)).substr(0, 16);
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
	const String activeDomain = ConfMan.getActiveDomainName();
	const bool assisted = !isChallengeModeEnabled() || isDisqualifyingAssistanceActive(activeDomain);
	const String popupSeenKey = makePopupSeenKey(game, variant, achievement, assisted);
	const bool popupAlreadySeen = hasPopupBeenSeen(popupSeenKey);
	bool localSet = true;
	if (nativeReady)
		localSet = achievements.setAchievement(achievement, false);

	const bool queuedNewEvent = queueAchievementEvent(game, variant, achievement);
	const bool localNewAchievement = nativeReady && !nativeAlreadyAchieved && localSet;
	const bool cePopupNeeded = !popupAlreadySeen && (localNewAchievement || nativeAlreadyAchieved || !nativeReady || queuedNewEvent);
	const CEAchievementDefinition *definition = findDefinition(game, variant, achievement);
	const bool challengeStyle = isChallengeModeEnabled() && definition && definition->challengeEligible && !isDisqualifyingAssistanceActive();
	bool cePopupShown = false;
	if (cePopupNeeded)
		cePopupShown = ceDisplayAchievementUnlockedOSD(definition ? definition->title : getAchievementTitle(game, variant, achievement), definition, challengeStyle);
	if (cePopupShown) {
		markPopupSeen(popupSeenKey);
		cePlayAchievementUnlockedSound();
	}

	retryQueuedEvents();
	return localSet;
}

bool CEAchievementService::noteGameEvent(AchievementsManager &achievements, const String &game, const String &variant, const String &event) {
	const CEAchievementRule *rule = findRule(game, variant, event);
	if (!rule)
		return false;

	return unlock(achievements, game, variant, rule->achievement);
}

void CEAchievementService::resetPopupState() {
	_popupSidecarName.clear();
	_popupSeenKeys.clear();
	_popupProgress.clear();
}

void CEAchievementService::loadPopupStateForSave(const String &saveName) {
	_popupSidecarName = makePopupSidecarName(saveName);
	if (!loadPopupSidecar(_popupSidecarName)) {
		_popupSeenKeys.clear();
		_popupProgress.clear();
	}
}

void CEAchievementService::savePopupStateForSave(const String &saveName) {
	_popupSidecarName = makePopupSidecarName(saveName);
	savePopupSidecar(_popupSidecarName);
}

uint32 CEAchievementService::getPopupProgress(const String &key) const {
	if (key.empty())
		return 0;

	return _popupProgress.getValOrDefault(key, 0);
}

void CEAchievementService::setPopupProgress(const String &key, uint32 value) {
	if (key.empty())
		return;

	_popupProgress.setVal(key, value);
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

	struct BuiltinRuleDefinition {
		const char *event;
		const char *achievement;
	};

	static const BuiltinRuleDefinition dejaVuRules[] = {
		{ "bathroom_stall_awakened", "rude_awakening" },
		{ "outfit_recovered", "dressed_for_trouble" },
		{ "body_found", "body_of_evidence" },
		{ "chemical_memory_uncovered", "chemical_memory" },
		{ "slot_machine_played", "house_money" },
		{ "glove_compartment_searched", "mrs_sternwoods_shadow" },
		{ "west_end_reached", "fare_to_west_end" },
		{ "penthouse_lead", "penthouse_lead" },
		{ "vickers_file_found", "vickers_file" },
		{ "memory_blocking_drug_identified", "diagnosis_murder" },
		{ "memory_recovered", "clear_headed" },
		{ "siegel_safe_opened", "dead_mans_safe" },
		{ "mercedes_trunk_opened", "trunk_discovery" },
		{ "auburn_road_reached", "auburn_road" },
		{ "notepad_shaded", "hard_copy" },
		{ "planted_evidence_disposed", "clean_frame" },
		{ "case_cleared", "citizen_of_the_week" },
		{ "slot_jackpot_won", "one_armed_bandit" },
		{ "alligator_survived", "sewer_survivor" },
		{ "mugger_survived", "mugger_management" },
		{ "both_cabs_ridden", "cab_hopper" },
		{ "gun_palace_shopped", "gun_palace_regular" },
		{ "office_files_read", "case_file_curious" },
		{ "pharmacy_files_read", "pharmacology_major" },
		{ "wrong_drug_used", "wrong_medicine" },
		{ "reckless_shot_fired", "bad_shot" },
		{ "bad_case_closed", "case_closed_badly" },
		{ "memory_loss_game_over", "vegetable_status" },
		{ "murder_weapon_conviction", "electric_goodbye" },
		{ "whirlpool_item_lost", "down_the_whirlpool" },
		{ nullptr, nullptr }
	};

	rule.game = "deja-vu";
	rule.variant = "macintosh-floppy";
	for (const BuiltinRuleDefinition *i = dejaVuRules; i->event; ++i) {
		rule.event = i->event;
		rule.achievement = i->achievement;
		addCatalogRule(rule);
	}
}

void CEAchievementService::loadBundledCatalog() const {
	Common::Archive *pack = ceOpenDataPack();
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

bool CEAchievementService::arePopupsEnabled() const {
	return !ConfMan.hasKey(kCEPopupsEnabledKey, ConfigManager::kApplicationDomain) ||
		ConfMan.getBool(kCEPopupsEnabledKey, ConfigManager::kApplicationDomain);
}

void CEAchievementService::setPopupsEnabled(bool enabled) {
	ConfMan.setBool(kCEPopupsEnabledKey, enabled, ConfigManager::kApplicationDomain);
	ConfMan.flushToDisk();
}

CEAchievementPopupLocation CEAchievementService::getPopupLocation() const {
	if (!ConfMan.hasKey(kCEPopupLocationKey, ConfigManager::kApplicationDomain))
		return kCEAchievementPopupLowerRight;
	return cePopupLocationFromConfig(ConfMan.get(kCEPopupLocationKey, ConfigManager::kApplicationDomain));
}

void CEAchievementService::setPopupLocation(CEAchievementPopupLocation location) {
	ConfMan.set(kCEPopupLocationKey, cePopupLocationToConfig(location), ConfigManager::kApplicationDomain);
	ConfMan.flushToDisk();
}

bool CEAchievementService::isPopupSoundEnabled() const {
	return !ConfMan.hasKey(kCEPopupSoundEnabledKey, ConfigManager::kApplicationDomain) ||
		ConfMan.getBool(kCEPopupSoundEnabledKey, ConfigManager::kApplicationDomain);
}

void CEAchievementService::setPopupSoundEnabled(bool enabled) {
	ConfMan.setBool(kCEPopupSoundEnabledKey, enabled, ConfigManager::kApplicationDomain);
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

	Array<String> auditFlags;
	auditFlags.push_back(flag);
	markActivePlaythroughCasual(auditFlags);
	queuePlaythroughAuditEvent("cheat_enabled", true, "assisted", auditFlags,
		String::format("{\"flag\":%s}", ceEscapeJson(flag).c_str()));
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

String CEAchievementService::getApiBaseUrl() const {
	if (ConfMan.hasKey(kCEApiBaseUrlKey, ConfigManager::kApplicationDomain))
		return ConfMan.get(kCEApiBaseUrlKey, ConfigManager::kApplicationDomain);
	return kCEApiBaseUrl;
}

String CEAchievementService::getActivePlaythroughId() const {
	return ConfMan.hasKey(kCEActivePlaythroughIdKey, ConfigManager::kApplicationDomain) ?
		ConfMan.get(kCEActivePlaythroughIdKey, ConfigManager::kApplicationDomain) : String();
}

String CEAchievementService::getActiveClientPlaythroughId() const {
	return ConfMan.hasKey(kCEActiveClientPlaythroughIdKey, ConfigManager::kApplicationDomain) ?
		ConfMan.get(kCEActiveClientPlaythroughIdKey, ConfigManager::kApplicationDomain) : String();
}

String CEAchievementService::getActivePlaythroughTarget() const {
	return ConfMan.hasKey(kCEActivePlaythroughTargetKey, ConfigManager::kApplicationDomain) ?
		ConfMan.get(kCEActivePlaythroughTargetKey, ConfigManager::kApplicationDomain) : String();
}

String CEAchievementService::getActivePlaythroughGame() const {
	return ConfMan.hasKey(kCEActivePlaythroughGameKey, ConfigManager::kApplicationDomain) ?
		ConfMan.get(kCEActivePlaythroughGameKey, ConfigManager::kApplicationDomain) : String();
}

String CEAchievementService::getActivePlaythroughVariant() const {
	return ConfMan.hasKey(kCEActivePlaythroughVariantKey, ConfigManager::kApplicationDomain) ?
		ConfMan.get(kCEActivePlaythroughVariantKey, ConfigManager::kApplicationDomain) : String();
}

String CEAchievementService::getActivePlaythroughStatus() const {
	return ConfMan.hasKey(kCEActivePlaythroughStatusKey, ConfigManager::kApplicationDomain) ?
		ConfMan.get(kCEActivePlaythroughStatusKey, ConfigManager::kApplicationDomain) : String();
}

bool CEAchievementService::isActivePlaythroughCasual() const {
	return ConfMan.getBool(kCEActivePlaythroughCasualKey, ConfigManager::kApplicationDomain);
}

void CEAchievementService::setActivePlaythrough(const String &target, const String &game, const String &variant, const String &clientPlaythroughId, const String &playthroughId, const String &status, bool casual, bool syncEligible) const {
	ConfMan.set(kCEActivePlaythroughTargetKey, target, ConfigManager::kApplicationDomain);
	ConfMan.set(kCEActivePlaythroughGameKey, game, ConfigManager::kApplicationDomain);
	ConfMan.set(kCEActivePlaythroughVariantKey, variant, ConfigManager::kApplicationDomain);
	ConfMan.set(kCEActiveClientPlaythroughIdKey, clientPlaythroughId, ConfigManager::kApplicationDomain);
	if (playthroughId.empty())
		ConfMan.removeKey(kCEActivePlaythroughIdKey, ConfigManager::kApplicationDomain);
	else
		ConfMan.set(kCEActivePlaythroughIdKey, playthroughId, ConfigManager::kApplicationDomain);
	ConfMan.set(kCEActivePlaythroughStatusKey, status, ConfigManager::kApplicationDomain);
	ConfMan.setBool(kCEActivePlaythroughCasualKey, casual, ConfigManager::kApplicationDomain);
	ConfMan.setBool(kCEActivePlaythroughSyncEligibleKey, syncEligible, ConfigManager::kApplicationDomain);
	ConfMan.flushToDisk();
}

Array<CEPlaythroughRecord> CEAchievementService::loadPlaythroughRegistry() const {
	Array<CEPlaythroughRecord> records;
	if (!g_system)
		return records;

	Common::InSaveFile *in = g_system->getSavefileManager()->openForLoading(kCEPlaythroughRegistryFile);
	if (!in)
		return records;

	String data = in->readString(0, in->size());
	delete in;

	JSONValue *root = JSON::parse(data);
	if (!root || !root->isObject()) {
		delete root;
		return records;
	}

	const JSONObject &obj = root->asObject();
	if (!obj.contains("playthroughs") || !obj["playthroughs"]->isArray()) {
		delete root;
		return records;
	}

	const JSONArray &items = obj["playthroughs"]->asArray();
	for (uint i = 0; i < items.size(); ++i) {
		if (!items[i]->isObject())
			continue;

		const JSONObject &item = items[i]->asObject();
		CEPlaythroughRecord record;
		record.target = ceStringFromJson(item, "target");
		record.game = ceStringFromJson(item, "game");
		record.variant = ceStringFromJson(item, "variant");
		record.clientPlaythroughId = ceStringFromJson(item, "client_playthrough_id");
		record.playthroughId = ceStringFromJson(item, "playthrough");
		record.title = ceStringFromJson(item, "title");
		record.status = ceStringFromJson(item, "status");
		record.archivedFromStatus = ceStringFromJson(item, "archived_from_status");
		record.startedAt = ceStringFromJson(item, "started_at");
		record.endedAt = ceStringFromJson(item, "ended_at");
		record.casual = ceBoolFromJson(item, "casual");
		const bool legacyArchived = ceBoolFromJson(item, "archived");
		if (legacyArchived && record.status != "archived") {
			if (record.archivedFromStatus.empty())
				record.archivedFromStatus = record.status.empty() || record.status == "active" ? "abandoned" : record.status;
			record.status = "archived";
		}
		if (record.status == "archived" && record.archivedFromStatus.empty())
			record.archivedFromStatus = "abandoned";
		record.archived = record.status == "archived";
		record.syncEligible = ceBoolFromJson(item, "sync_eligible");
		if (!record.target.empty() && !record.clientPlaythroughId.empty())
			records.push_back(record);
	}

	delete root;
	return records;
}

bool CEAchievementService::savePlaythroughRegistry(const Array<CEPlaythroughRecord> &records) const {
	if (!g_system)
		return false;

	Common::OutSaveFile *out = g_system->getSavefileManager()->openForSaving(kCEPlaythroughRegistryFile, false);
	if (!out)
		return false;

	String json = "{\"version\":1,\"playthroughs\":[";
	for (uint i = 0; i < records.size(); ++i) {
		if (i)
			json += ",";
		const CEPlaythroughRecord &record = records[i];
		const bool archived = record.status == "archived";
		json += String::format("{\"target\":%s,\"game\":%s,\"variant\":%s,\"client_playthrough_id\":%s,\"playthrough\":%s,\"title\":%s,\"status\":%s,\"archived_from_status\":%s,\"started_at\":%s,\"ended_at\":%s,\"casual\":%s,\"archived\":%s,\"sync_eligible\":%s}",
			ceEscapeJson(record.target).c_str(), ceEscapeJson(record.game).c_str(),
			ceEscapeJson(record.variant).c_str(), ceEscapeJson(record.clientPlaythroughId).c_str(),
			ceEscapeJson(record.playthroughId).c_str(), ceEscapeJson(record.title).c_str(),
			ceEscapeJson(record.status).c_str(), ceEscapeJson(record.archivedFromStatus).c_str(),
			ceEscapeJson(record.startedAt).c_str(), ceEscapeJson(record.endedAt).c_str(),
			record.casual ? "true" : "false", archived ? "true" : "false",
			record.syncEligible ? "true" : "false");
	}
	json += "]}";

	out->writeString(json);
	out->finalize();
	delete out;
	return true;
}

void CEAchievementService::upsertPlaythroughRecord(const CEPlaythroughRecord &record) const {
	if (record.target.empty() || record.clientPlaythroughId.empty())
		return;

	Array<CEPlaythroughRecord> records = loadPlaythroughRegistry();
	for (uint i = 0; i < records.size(); ++i) {
		if (records[i].clientPlaythroughId == record.clientPlaythroughId) {
			records[i] = record;
			savePlaythroughRegistry(records);
			return;
		}
	}

	records.push_back(record);
	savePlaythroughRegistry(records);
}

void CEAchievementService::updatePlaythroughRecord(const String &clientPlaythroughId, const String &playthroughId, const String &status, bool casual, bool syncEligible, const String &endedAt) const {
	if (clientPlaythroughId.empty())
		return;

	Array<CEPlaythroughRecord> records = loadPlaythroughRegistry();
	for (uint i = 0; i < records.size(); ++i) {
		if (records[i].clientPlaythroughId != clientPlaythroughId)
			continue;

		if (!playthroughId.empty())
			records[i].playthroughId = playthroughId;
		if (!status.empty() && !(status == "active" && records[i].status != "active"))
			records[i].status = status;
		if (!endedAt.empty())
			records[i].endedAt = endedAt;
		records[i].casual = casual;
		records[i].syncEligible = syncEligible;
		savePlaythroughRegistry(records);
		return;
	}
}

void CEAchievementService::clearActivePlaythrough() const {
	ConfMan.removeKey(kCEActivePlaythroughTargetKey, ConfigManager::kApplicationDomain);
	ConfMan.removeKey(kCEActivePlaythroughGameKey, ConfigManager::kApplicationDomain);
	ConfMan.removeKey(kCEActivePlaythroughVariantKey, ConfigManager::kApplicationDomain);
	ConfMan.removeKey(kCEActiveClientPlaythroughIdKey, ConfigManager::kApplicationDomain);
	ConfMan.removeKey(kCEActivePlaythroughIdKey, ConfigManager::kApplicationDomain);
	ConfMan.removeKey(kCEActivePlaythroughStatusKey, ConfigManager::kApplicationDomain);
	ConfMan.removeKey(kCEActivePlaythroughCasualKey, ConfigManager::kApplicationDomain);
	ConfMan.removeKey(kCEActivePlaythroughSyncEligibleKey, ConfigManager::kApplicationDomain);
	ConfMan.flushToDisk();
}

void CEAchievementService::markActivePlaythroughCasual(const Array<String> &flags) const {
	if (getActiveClientPlaythroughId().empty())
		return;

	ConfMan.setBool(kCEActivePlaythroughCasualKey, true, ConfigManager::kApplicationDomain);
	String stored = ConfMan.hasKey("ce_achievements_assistance_flags", ConfigManager::kApplicationDomain) ?
		ConfMan.get("ce_achievements_assistance_flags", ConfigManager::kApplicationDomain) : String();
	for (uint i = 0; i < flags.size(); ++i) {
		if (!stored.contains(flags[i])) {
			if (!stored.empty())
				stored += ",";
			stored += flags[i];
		}
	}
	if (!stored.empty())
		ConfMan.set("ce_achievements_assistance_flags", stored, ConfigManager::kApplicationDomain);
	ConfMan.flushToDisk();
	updatePlaythroughRecord(getActiveClientPlaythroughId(), getActivePlaythroughId(), getActivePlaythroughStatus(), true, ConfMan.getBool(kCEActivePlaythroughSyncEligibleKey, ConfigManager::kApplicationDomain));
}

String CEAchievementService::makePlaythroughId(const String &target) const {
	String profile = getOrCreateInstallSecret().substr(0, 16);
	return String::format("%s:%s:run:%s:%u", profile.c_str(), target.c_str(), currentTimeString().c_str(), g_system->getMillis(true));
}

String CEAchievementService::makePlaythroughEventId(const String &type) const {
	uint32 counter = ConfMan.getInt(kCEPlaythroughEventCounterKey, ConfigManager::kApplicationDomain) + 1;
	ConfMan.setInt(kCEPlaythroughEventCounterKey, counter, ConfigManager::kApplicationDomain);
	ConfMan.flushToDisk();
	return String::format("%s:event:%s:%u", getActiveClientPlaythroughId().c_str(), type.c_str(), counter);
}

String CEAchievementService::canonicalizeEvent(const CEAchievementEvent &event) const {
	String flags = ceStringArrayJson(event.assistanceFlags);
	if (event.requestType == "playthrough_start") {
		String body = String::format("{\"client_playthrough_id\":%s,\"game\":%s,\"started_at\":%s",
			ceEscapeJson(event.clientPlaythroughId).c_str(),
			ceEscapeJson(event.game).c_str(), ceEscapeJson(event.occurredAt).c_str());
		if (!event.title.empty())
			body += String::format(",\"title\":%s", ceEscapeJson(event.title).c_str());
		body += String::format(",\"variant\":%s}", ceEscapeJson(event.variant).c_str());
		return body;
	}

	if (event.requestType == "playthrough_end") {
		return String::format("{\"ended_at\":%s,\"status\":%s}",
			ceEscapeJson(event.occurredAt).c_str(), ceEscapeJson(event.status).c_str());
	}

	if (event.requestType == "playthrough_save") {
		String body = String::format("{\"assistance_flags\":%s,\"client_save_id\":%s,\"created_at_client\":%s,\"label\":%s,\"save_hash\":%s,\"slot\":%s,\"tainted\":%s",
			flags.c_str(), ceEscapeJson(event.clientSaveId).c_str(), ceEscapeJson(event.occurredAt).c_str(),
			ceEscapeJson(event.label).c_str(), ceEscapeJson(event.saveHash).c_str(),
			ceEscapeJson(event.slot).c_str(), event.tainted ? "true" : "false");
		if (event.lastLoadedAt.empty())
			body += ",\"last_loaded_at\":null}";
		else
			body += String::format(",\"last_loaded_at\":%s}", ceEscapeJson(event.lastLoadedAt).c_str());
		return body;
	}

	if (event.requestType == "playthrough_event") {
		return String::format("{\"assistance_flags\":%s,\"assisted\":%s,\"event_id\":%s,\"history_status\":%s,\"metadata\":%s,\"occurred_at\":%s,\"type\":%s}",
			flags.c_str(), event.assisted ? "true" : "false", ceEscapeJson(event.eventId).c_str(),
			ceEscapeJson(event.historyStatus).c_str(), event.metadata.empty() ? "{}" : event.metadata.c_str(),
			ceEscapeJson(event.occurredAt).c_str(), ceEscapeJson(event.achievement).c_str());
	}

	String body = String::format("{\"achievement\":%s,\"assistance_flags\":%s,\"assisted\":%s,\"event_id\":%s,\"game\":%s,\"history_status\":%s,\"occurred_at\":%s",
		ceEscapeJson(event.achievement).c_str(), flags.c_str(), event.assisted ? "true" : "false",
		ceEscapeJson(event.eventId).c_str(), ceEscapeJson(event.game).c_str(),
		ceEscapeJson(event.historyStatus).c_str(), ceEscapeJson(event.occurredAt).c_str());
	if (!event.playthrough.empty())
		body += String::format(",\"playthrough\":%s", ceEscapeJson(event.playthrough).c_str());
	body += String::format(",\"variant\":%s}", ceEscapeJson(event.variant).c_str());
	return body;
}

String CEAchievementService::signEvent(const CEAchievementEvent &event) const {
	return ceHmacSha256(getOrCreateInstallSecret(), event.method + "\n" + event.path + "\n" + canonicalizeEvent(event));
}

bool CEAchievementService::verifyEventSignature(const CEAchievementEvent &event) const {
	return event.signature == signEvent(event);
}

String CEAchievementService::makeEventId(const String &game, const String &variant, const String &achievement, bool assisted, const String &clientPlaythroughId) const {
	String profile = getOrCreateInstallSecret().substr(0, 16);
	if (!clientPlaythroughId.empty()) {
		const String runHash = ceShortHash(clientPlaythroughId);
		const String achievementHash = ceShortHash(game + ":" + variant + ":" + achievement);
		return String::format("%s:%s:%s:%s", profile.c_str(), runHash.c_str(), achievementHash.c_str(), assisted ? "completion" : "challenge");
	}
	return String::format("%s:%s:%s:%s:%s", profile.c_str(), game.c_str(), variant.c_str(), achievement.c_str(), assisted ? "completion" : "challenge");
}

String CEAchievementService::makePopupSeenKey(const String &game, const String &variant, const String &achievement, bool assisted) const {
	return String::format("%s:%s:%s:%s", assisted ? "completion" : "challenge", game.c_str(), variant.c_str(), achievement.c_str());
}

String CEAchievementService::makePopupSidecarName(const String &saveName) const {
	return saveName + ".ce-achievements.json";
}

String CEAchievementService::hashSaveFile(const String &saveName) const {
	if (saveName.empty() || !g_system)
		return String();

	Common::InSaveFile *in = g_system->getSavefileManager()->openForLoading(saveName);
	if (!in)
		return String();

	Array<byte> bytes;
	while (!in->eos()) {
		byte buffer[1024];
		uint32 read = in->read(buffer, sizeof(buffer));
		for (uint32 i = 0; i < read; ++i)
			bytes.push_back(buffer[i]);
		if (read == 0)
			break;
	}
	delete in;
	return ceHexDigest(ceSha256(bytes));
}

bool CEAchievementService::loadPlaythroughSidecar(const String &sidecarName, bool &tainted, String &clientPlaythroughId, String &playthroughId) const {
	tainted = false;
	clientPlaythroughId.clear();
	playthroughId.clear();
	if (sidecarName.empty() || !g_system)
		return false;

	Common::InSaveFile *in = g_system->getSavefileManager()->openForLoading(sidecarName);
	if (!in)
		return false;

	String data = in->readString(0, in->size());
	delete in;

	JSONValue *root = JSON::parse(data);
	if (!root || !root->isObject()) {
		delete root;
		return false;
	}
	const JSONObject &obj = root->asObject();
	tainted = ceBoolFromJson(obj, "tainted");
	clientPlaythroughId = ceStringFromJson(obj, "client_playthrough_id");
	playthroughId = ceStringFromJson(obj, "playthrough");
	delete root;
	return true;
}

bool CEAchievementService::savePlaythroughSidecar(const String &sidecarName, bool tainted, const String &clientPlaythroughId, const String &playthroughId) const {
	if (sidecarName.empty() || !g_system)
		return false;

	Common::OutSaveFile *out = g_system->getSavefileManager()->openForSaving(sidecarName, false);
	if (!out)
		return false;

	String json = String::format("{\"version\":1,\"client_playthrough_id\":%s,\"playthrough\":%s,\"tainted\":%s,\"popup_seen\":[",
		ceEscapeJson(clientPlaythroughId).c_str(), ceEscapeJson(playthroughId).c_str(), tainted ? "true" : "false");
	for (uint i = 0; i < _popupSeenKeys.size(); ++i) {
		if (i)
			json += ",";
		json += ceEscapeJson(_popupSeenKeys[i]);
	}
	json += "],\"progress\":{";
	bool firstProgress = true;
	for (HashMap<String, uint32>::const_iterator it = _popupProgress.begin(); it != _popupProgress.end(); ++it) {
		if (!firstProgress)
			json += ",";
		firstProgress = false;
		json += ceEscapeJson(it->_key);
		json += ":";
		json += String::format("%u", it->_value);
	}
	json += "}}";

	out->writeString(json);
	out->finalize();
	delete out;
	return true;
}

bool CEAchievementService::hasPopupBeenSeen(const String &key) const {
	for (uint i = 0; i < _popupSeenKeys.size(); ++i) {
		if (_popupSeenKeys[i] == key)
			return true;
	}
	return false;
}

void CEAchievementService::markPopupSeen(const String &key) {
	if (key.empty() || hasPopupBeenSeen(key))
		return;

	_popupSeenKeys.push_back(key);
	if (!_popupSidecarName.empty())
		savePopupSidecar(_popupSidecarName);
}

bool CEAchievementService::loadPopupSidecar(const String &sidecarName) {
	_popupSeenKeys.clear();
	_popupProgress.clear();
	if (sidecarName.empty() || !g_system)
		return false;

	Common::InSaveFile *in = g_system->getSavefileManager()->openForLoading(sidecarName);
	if (!in)
		return false;

	String data = in->readString(0, in->size());
	delete in;

	JSONValue *root = JSON::parse(data);
	if (!root || !root->isObject()) {
		delete root;
		warning("Ignoring invalid CE achievement popup sidecar '%s'.", sidecarName.c_str());
		return false;
	}

	const JSONObject &obj = root->asObject();
	if (obj.contains("popup_seen") && obj["popup_seen"]->isArray()) {
		const JSONArray &seen = obj["popup_seen"]->asArray();
		for (uint i = 0; i < seen.size(); ++i) {
			if (seen[i]->isString()) {
				const String key = seen[i]->asString();
				if (!hasPopupBeenSeen(key))
					_popupSeenKeys.push_back(key);
			}
		}
	}

	if (obj.contains("progress") && obj["progress"]->isObject()) {
		const JSONObject &progress = obj["progress"]->asObject();
		for (JSONObject::const_iterator it = progress.begin(); it != progress.end(); ++it) {
			if (!it->_value || !it->_value->isIntegerNumber())
				continue;

			const long long int value = it->_value->asIntegerNumber();
			if (value < 0 || value > 0xffffffffLL)
				continue;

			_popupProgress.setVal(it->_key, (uint32)value);
		}
	}

	delete root;
	return true;
}

bool CEAchievementService::savePopupSidecar(const String &sidecarName) const {
	if (sidecarName.empty() || !g_system)
		return false;

	Common::OutSaveFile *out = g_system->getSavefileManager()->openForSaving(sidecarName, false);
	if (!out) {
		warning("Unable to open CE achievement popup sidecar '%s' for saving.", sidecarName.c_str());
		return false;
	}

	String json = String::format("{\"version\":1,\"client_playthrough_id\":%s,\"playthrough\":%s,\"tainted\":%s,\"popup_seen\":[",
		ceEscapeJson(getActiveClientPlaythroughId()).c_str(), ceEscapeJson(getActivePlaythroughId()).c_str(),
		isActivePlaythroughCasual() ? "true" : "false");
	for (uint i = 0; i < _popupSeenKeys.size(); ++i) {
		if (i)
			json += ",";
		json += ceEscapeJson(_popupSeenKeys[i]);
	}
	json += "],\"progress\":{";
	bool firstProgress = true;
	for (HashMap<String, uint32>::const_iterator it = _popupProgress.begin(); it != _popupProgress.end(); ++it) {
		if (!firstProgress)
			json += ",";
		firstProgress = false;
		json += ceEscapeJson(it->_key);
		json += ":";
		json += String::format("%u", it->_value);
	}
	json += "}}";

	out->writeString(json);
	out->finalize();
	if (out->err())
		warning("Could not save CE achievement popup sidecar '%s' correctly.", sidecarName.c_str());
	delete out;
	return true;
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
		event.requestType = ceStringFromJson(obj, "request_type", "achievement");
		event.method = ceStringFromJson(obj, "method", "POST");
		event.path = ceStringFromJson(obj, "path");
		if (event.path.empty())
			event.path = "/events";
		event.eventId = ceStringFromJson(obj, "event_id");
		event.game = ceStringFromJson(obj, "game");
		event.variant = ceStringFromJson(obj, "variant");
		event.achievement = ceStringFromJson(obj, "achievement");
		event.playthrough = ceStringFromJson(obj, "playthrough");
		event.clientPlaythroughId = ceStringFromJson(obj, "client_playthrough_id");
		event.title = ceStringFromJson(obj, "title");
		event.status = ceStringFromJson(obj, "status");
		event.clientSaveId = ceStringFromJson(obj, "client_save_id");
		event.slot = ceStringFromJson(obj, "slot");
		event.label = ceStringFromJson(obj, "label");
		event.saveHash = ceStringFromJson(obj, "save_hash");
		event.metadata = ceStringFromJson(obj, "metadata");
		if (event.metadata.empty() && obj.contains("metadata") && obj["metadata"])
			event.metadata = JSON::stringify(obj["metadata"]);
		event.assisted = ceBoolFromJson(obj, "assisted");
		event.tainted = ceBoolFromJson(obj, "tainted");
		event.historyStatus = ceStringFromJson(obj, "history_status");
		event.occurredAt = ceStringFromJson(obj, "occurred_at");
		event.lastLoadedAt = ceStringFromJson(obj, "last_loaded_at");
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
		String flags = ceStringArrayJson(event.assistanceFlags);
		json += String::format("{\"request_type\":%s,\"method\":%s,\"path\":%s,\"event_id\":%s,\"game\":%s,\"variant\":%s,\"achievement\":%s,\"playthrough\":%s,\"client_playthrough_id\":%s,\"title\":%s,\"status\":%s,\"client_save_id\":%s,\"slot\":%s,\"label\":%s,\"save_hash\":%s,\"metadata\":%s,\"assisted\":%s,\"tainted\":%s,\"history_status\":%s,\"assistance_flags\":%s,\"occurred_at\":%s,\"last_loaded_at\":%s,\"attempt_count\":%u,\"last_attempt_at\":%s,\"last_error\":%s,\"state\":%s,\"signature\":%s}",
			ceEscapeJson(event.requestType).c_str(), ceEscapeJson(event.method).c_str(), ceEscapeJson(event.path).c_str(),
			ceEscapeJson(event.eventId).c_str(), ceEscapeJson(event.game).c_str(),
			ceEscapeJson(event.variant).c_str(), ceEscapeJson(event.achievement).c_str(),
			ceEscapeJson(event.playthrough).c_str(), ceEscapeJson(event.clientPlaythroughId).c_str(),
			ceEscapeJson(event.title).c_str(), ceEscapeJson(event.status).c_str(),
			ceEscapeJson(event.clientSaveId).c_str(), ceEscapeJson(event.slot).c_str(),
			ceEscapeJson(event.label).c_str(), ceEscapeJson(event.saveHash).c_str(),
			event.metadata.empty() ? "null" : event.metadata.c_str(),
			event.assisted ? "true" : "false", event.tainted ? "true" : "false",
			ceEscapeJson(event.historyStatus).c_str(), flags.c_str(),
			ceEscapeJson(event.occurredAt).c_str(), ceEscapeJson(event.lastLoadedAt).c_str(), event.attemptCount,
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
	if (!ConfMan.getBool(kCEActivePlaythroughSyncEligibleKey, ConfigManager::kApplicationDomain))
		return false;
	if (getActivePlaythroughStatus() != "active")
		return false;
	if (!game.empty() && getActivePlaythroughGame() != game)
		return false;
	if (!variant.empty() && getActivePlaythroughVariant() != variant)
		return false;
	const String clientPlaythroughId = getActiveClientPlaythroughId();
	if (clientPlaythroughId.empty())
		return false;

	const String activeDomain = ConfMan.getActiveDomainName();
	const bool assisted = !isChallengeModeEnabled() || isDisqualifyingAssistanceActive(activeDomain) || isActivePlaythroughCasual();
	CEAchievementEvent event;
	event.requestType = "achievement";
	event.method = "POST";
	event.path = "/events";
	event.game = game;
	event.variant = variant;
	event.achievement = achievement;
	event.playthrough = getActivePlaythroughId();
	event.clientPlaythroughId = clientPlaythroughId;
	event.assisted = assisted;
	event.historyStatus = assisted ? "assisted" : "verified";
	event.assistanceFlags = getAssistanceFlags(activeDomain);
	event.occurredAt = currentTimeString();
	event.attemptCount = 0;
	event.state = "pending";
	event.eventId = makeEventId(game, variant, achievement, assisted, clientPlaythroughId);
	event.signature = signEvent(event);

	return queueEvent(event);
}

bool CEAchievementService::queueEvent(const CEAchievementEvent &event) {
	Array<CEAchievementEvent> events = loadQueue();
	for (uint i = 0; i < events.size(); ++i) {
		if (events[i].eventId == event.eventId && events[i].requestType == event.requestType) {
			if (events[i].signature != event.signature) {
				warning("CE achievement event id reused with a changed payload: %s", event.eventId.c_str());
				ConfMan.set(kCELastSyncStatusKey, "CE achievement queue bug: event id reused with a changed payload.", ConfigManager::kApplicationDomain);
				ConfMan.flushToDisk();
			}
			return false;
		}
	}
	events.push_back(event);
	return saveQueue(events);
}

bool CEAchievementService::resolveQueuedAchievementPlaythroughs(Array<CEAchievementEvent> &events) const {
	bool changed = false;
	Array<CEPlaythroughRecord> records = loadPlaythroughRegistry();

	for (uint i = 0; i < events.size(); ++i) {
		CEAchievementEvent &event = events[i];
		if (event.requestType != "achievement" || !event.playthrough.empty() || event.clientPlaythroughId.empty())
			continue;
		if (event.state == "synced" || event.state == "permanent_failure")
			continue;

		for (uint j = 0; j < records.size(); ++j) {
			if (records[j].clientPlaythroughId != event.clientPlaythroughId)
				continue;
			if (records[j].playthroughId.empty())
				break;
			event.playthrough = records[j].playthroughId;
			event.signature = signEvent(event);
			changed = true;
			break;
		}
	}

	return changed;
}

bool CEAchievementService::queuePlaythroughAuditEvent(const String &type, bool assisted, const String &historyStatus, const Array<String> &flags, const String &metadata) {
	if (!isActivePlaythroughSyncEligible())
		return false;

	CEAchievementEvent event;
	event.requestType = "playthrough_event";
	event.method = "POST";
	event.path = "/playthroughs/" + getActivePlaythroughId() + "/events";
	event.eventId = makePlaythroughEventId(type);
	event.playthrough = getActivePlaythroughId();
	event.achievement = type;
	event.assisted = assisted;
	event.historyStatus = historyStatus;
	event.assistanceFlags = flags;
	event.metadata = metadata;
	event.occurredAt = currentTimeString();
	event.attemptCount = 0;
	event.state = "pending";
	event.signature = signEvent(event);
	const bool queued = queueEvent(event);
	retryQueuedEvents();
	return queued;
}

bool CEAchievementService::startPlaythroughForTarget(const String &target, const String &game, const String &variant, const String &title) {
	if (target.empty())
		return false;

	const String clientPlaythroughId = makePlaythroughId(target);
	const String startedAt = currentTimeString();
	const bool syncEligible = !game.empty() && !variant.empty() && !getLinkToken().empty();
	setActivePlaythrough(target, game, variant, clientPlaythroughId, String(), "active", false, syncEligible);

	CEPlaythroughRecord record;
	record.target = target;
	record.game = game;
	record.variant = variant;
	record.clientPlaythroughId = clientPlaythroughId;
	record.title = title;
	record.status = "active";
	record.startedAt = startedAt;
	record.casual = false;
	record.syncEligible = syncEligible;
	upsertPlaythroughRecord(record);

	if (!syncEligible)
		return true;

	CEAchievementEvent event;
	event.requestType = "playthrough_start";
	event.method = "POST";
	event.path = "/playthroughs";
	event.eventId = clientPlaythroughId;
	event.clientPlaythroughId = clientPlaythroughId;
	event.game = game;
	event.variant = variant;
	event.title = title;
	event.occurredAt = startedAt;
	event.attemptCount = 0;
	event.state = "pending";
	event.signature = signEvent(event);
	const bool queued = queueEvent(event);
	retryQueuedEvents();
	return queued;
}

bool CEAchievementService::hasAchievementSupport(const String &game, const String &variant) const {
	loadCatalogIfNeeded();
	for (uint i = 0; i < _catalog.size(); ++i) {
		if (_catalog[i].game != game)
			continue;
		if (_catalog[i].variant == variant || _catalog[i].variant.empty() || _catalog[i].variant == "*")
			return true;
	}
	return false;
}

String CEAchievementService::getCurrentPlaythroughForTarget(const String &target) const {
	if (!target.empty() && getActivePlaythroughTarget() == target)
		return getActiveClientPlaythroughId();
	return String();
}

Array<CEPlaythroughRecord> CEAchievementService::listPlaythroughsForTarget(const String &target) const {
	Array<CEPlaythroughRecord> result;
	Array<CEPlaythroughRecord> records = loadPlaythroughRegistry();
	for (uint i = 0; i < records.size(); ++i) {
		if (records[i].target == target)
			result.push_back(records[i]);
	}
	return result;
}

String CEAchievementService::getDefaultLoadPlaythroughForTarget(const String &target) const {
	String clientPlaythroughId;
	String startedAt;
	Array<CEPlaythroughRecord> records = listPlaythroughsForTarget(target);
	for (uint i = 0; i < records.size(); ++i) {
		if (!clientPlaythroughId.empty() && records[i].startedAt <= startedAt)
			continue;
		clientPlaythroughId = records[i].clientPlaythroughId;
		startedAt = records[i].startedAt;
	}
	return clientPlaythroughId;
}

String CEAchievementService::getSavePlaythroughForTargetSlot(const String &target, int slot) const {
	Array<String> saveNames;
	if (g_engine && ConfMan.getActiveDomainName() == target)
		saveNames.push_back(g_engine->getSaveStateName(slot));
	saveNames.push_back(String::format("%s.%03d", target.c_str(), slot));
	saveNames.push_back(String::format("%s.%d", target.c_str(), slot));

	for (uint i = 0; i < saveNames.size(); ++i) {
		bool tainted = false;
		String clientPlaythroughId;
		String playthroughId;
		if (loadPlaythroughSidecar(makePopupSidecarName(saveNames[i]), tainted, clientPlaythroughId, playthroughId))
			return clientPlaythroughId;
	}
	return String();
}

bool CEAchievementService::setPlaythroughStatusForTarget(const String &target, const String &clientPlaythroughId, const String &status) {
	if (target.empty() || clientPlaythroughId.empty() || status.empty() || status == "active" || status == "untracked")
		return false;

	Array<CEPlaythroughRecord> records = loadPlaythroughRegistry();
	CEPlaythroughRecord record;
	bool found = false;
	for (uint i = 0; i < records.size(); ++i) {
		if (records[i].target != target || records[i].clientPlaythroughId != clientPlaythroughId)
			continue;
		if (records[i].status == status)
			return false;
		if (records[i].status == "completed" && status != "archived")
			return false;
		if (records[i].status == "active" && status == "archived")
			return false;
		if (records[i].status == "archived" && status != "completed" && status != "abandoned")
			return false;

		const String endedAt = (records[i].status == "active" || records[i].endedAt.empty()) ? currentTimeString() : records[i].endedAt;
		if (status == "archived")
			records[i].archivedFromStatus = records[i].status == "completed" ? "completed" : "abandoned";
		else
			records[i].archivedFromStatus.clear();
		records[i].status = status;
		records[i].archived = status == "archived";
		records[i].endedAt = endedAt;
		records[i].syncEligible = false;
		record = records[i];
		found = true;
		break;
	}

	if (!found)
		return false;

	savePlaythroughRegistry(records);
	if (getActivePlaythroughTarget() == target && getActiveClientPlaythroughId() == clientPlaythroughId)
		setActivePlaythrough(record.target, record.game, record.variant, record.clientPlaythroughId,
			record.playthroughId, record.status, record.casual, false);

	if (record.playthroughId.empty())
		return true;

	CEAchievementEvent event;
	event.requestType = "playthrough_end";
	event.method = "PATCH";
	event.path = "/playthroughs/" + record.playthroughId;
	event.eventId = makePlaythroughEventId(String("status-") + status);
	event.playthrough = record.playthroughId;
	event.status = status;
	event.occurredAt = record.endedAt;
	event.attemptCount = 0;
	event.state = "pending";
	event.signature = signEvent(event);
	const bool queued = queueEvent(event);
	retryQueuedEvents();
	return queued;
}

bool CEAchievementService::setPlaythroughArchivedForTarget(const String &target, const String &clientPlaythroughId, bool archived) {
	if (target.empty() || clientPlaythroughId.empty())
		return false;

	Array<CEPlaythroughRecord> records = loadPlaythroughRegistry();
	for (uint i = 0; i < records.size(); ++i) {
		if (records[i].target != target || records[i].clientPlaythroughId != clientPlaythroughId)
			continue;

		if (archived)
			return setPlaythroughStatusForTarget(target, clientPlaythroughId, "archived");

		if (records[i].status != "archived")
			return false;

		const String restoredStatus = records[i].archivedFromStatus == "completed" ? "completed" : "abandoned";
		return setPlaythroughStatusForTarget(target, clientPlaythroughId, restoredStatus);
	}

	return false;
}

bool CEAchievementService::deletePlaythroughForTarget(const String &target, const String &clientPlaythroughId) const {
	if (target.empty() || clientPlaythroughId.empty())
		return false;

	Array<CEPlaythroughRecord> records = loadPlaythroughRegistry();
	bool removed = false;
	for (int i = (int)records.size() - 1; i >= 0; --i) {
		if (records[i].target == target && records[i].clientPlaythroughId == clientPlaythroughId) {
			records.remove_at(i);
			removed = true;
		}
	}
	if (!removed)
		return false;

	savePlaythroughRegistry(records);
	if (getActivePlaythroughTarget() == target && getActiveClientPlaythroughId() == clientPlaythroughId)
		clearActivePlaythrough();
	return true;
}

bool CEAchievementService::clearSavePlaythroughForTargetSlot(const String &target, int slot, const String &clientPlaythroughId) const {
	if (target.empty() || clientPlaythroughId.empty() || !g_system)
		return false;

	Array<String> saveNames;
	if (g_engine && ConfMan.getActiveDomainName() == target)
		saveNames.push_back(g_engine->getSaveStateName(slot));
	saveNames.push_back(String::format("%s.%03d", target.c_str(), slot));
	saveNames.push_back(String::format("%s.%d", target.c_str(), slot));

	bool removed = false;
	for (uint i = 0; i < saveNames.size(); ++i) {
		bool tainted = false;
		String sidecarClientPlaythroughId;
		String playthroughId;
		const String sidecarName = makePopupSidecarName(saveNames[i]);
		if (loadPlaythroughSidecar(sidecarName, tainted, sidecarClientPlaythroughId, playthroughId) &&
				sidecarClientPlaythroughId == clientPlaythroughId) {
			g_system->getSavefileManager()->removeSavefile(sidecarName);
			removed = true;
		}
	}
	return removed;
}

bool CEAchievementService::endActivePlaythrough(const String &status) {
	const String currentStatus = getActivePlaythroughStatus();
	if (getActiveClientPlaythroughId().empty() || currentStatus == "completed")
		return false;

	const String nextStatus = status.empty() ? "completed" : status;
	if (nextStatus == "active")
		return false;

	const String playthroughId = getActivePlaythroughId();
	const String endedAt = currentTimeString();
	setActivePlaythrough(getActivePlaythroughTarget(), getActivePlaythroughGame(), getActivePlaythroughVariant(),
		getActiveClientPlaythroughId(), playthroughId, nextStatus, isActivePlaythroughCasual(), false);
	updatePlaythroughRecord(getActiveClientPlaythroughId(), playthroughId, nextStatus, isActivePlaythroughCasual(), false, endedAt);

	if (playthroughId.empty())
		return true;

	CEAchievementEvent event;
	event.requestType = "playthrough_end";
	event.method = "PATCH";
	event.path = "/playthroughs/" + playthroughId;
	event.eventId = makePlaythroughEventId("end");
	event.playthrough = playthroughId;
	event.status = nextStatus;
	event.occurredAt = endedAt;
	event.attemptCount = 0;
	event.state = "pending";
	event.signature = signEvent(event);
	const bool queued = queueEvent(event);
	retryQueuedEvents();
	return queued;
}

bool CEAchievementService::hasActivePlaythroughInProgress() const {
	return !getActiveClientPlaythroughId().empty() && getActivePlaythroughStatus() == "active";
}

bool CEAchievementService::isActivePlaythroughSyncEligible(const String &game, const String &variant) const {
	if (!ConfMan.getBool(kCEActivePlaythroughSyncEligibleKey, ConfigManager::kApplicationDomain))
		return false;
	if (getActivePlaythroughStatus() != "active")
		return false;
	if (getActivePlaythroughId().empty())
		return false;
	if (!game.empty() && getActivePlaythroughGame() != game)
		return false;
	if (!variant.empty() && getActivePlaythroughVariant() != variant)
		return false;
	return true;
}

bool CEAchievementService::activatePlaythroughRecordForLoadedSave(const String &clientPlaythroughId, const String &playthroughId) const {
	if (clientPlaythroughId.empty()) {
		clearActivePlaythrough();
		return false;
	}

	Array<CEPlaythroughRecord> records = loadPlaythroughRegistry();
	for (uint i = 0; i < records.size(); ++i) {
		if (records[i].clientPlaythroughId != clientPlaythroughId)
			continue;

		const bool syncEligible = records[i].syncEligible && records[i].status == "active" && !records[i].playthroughId.empty() &&
			!records[i].game.empty() && !records[i].variant.empty();
		setActivePlaythrough(records[i].target, records[i].game, records[i].variant,
			records[i].clientPlaythroughId, records[i].playthroughId.empty() ? playthroughId : records[i].playthroughId,
			records[i].status, records[i].casual, syncEligible);
		return true;
	}

	clearActivePlaythrough();
	return false;
}

void CEAchievementService::recordSaveCreated(const String &saveName, int slot, const String &label) {
	if (getActiveClientPlaythroughId().empty())
		return;

	const bool tainted = isActivePlaythroughCasual() || getActivePlaythroughStatus() != "active";
	savePlaythroughSidecar(makePopupSidecarName(saveName), tainted, getActiveClientPlaythroughId(), getActivePlaythroughId());
	if (!isActivePlaythroughSyncEligible())
		return;

	CEAchievementEvent event;
	event.requestType = "playthrough_save";
	event.method = "POST";
	event.path = "/playthroughs/" + getActivePlaythroughId() + "/saves";
	event.eventId = makePlaythroughEventId("save");
	event.playthrough = getActivePlaythroughId();
	event.clientSaveId = String::format("%s:slot:%d", getActiveClientPlaythroughId().c_str(), slot);
	event.slot = String::format("%d", slot);
	event.label = label;
	event.saveHash = hashSaveFile(saveName);
	event.tainted = tainted;
	event.assistanceFlags = tainted ? getAssistanceFlags() : Array<String>();
	event.occurredAt = currentTimeString();
	event.attemptCount = 0;
	event.state = "pending";
	event.signature = signEvent(event);
	queueEvent(event);
	retryQueuedEvents();
}

void CEAchievementService::recordSaveLoaded(const String &saveName, int slot) {
	bool tainted = false;
	String clientPlaythroughId;
	String playthroughId;
	if (!loadPlaythroughSidecar(makePopupSidecarName(saveName), tainted, clientPlaythroughId, playthroughId) ||
			clientPlaythroughId.empty()) {
		clearActivePlaythrough();
		return;
	}

	activatePlaythroughRecordForLoadedSave(clientPlaythroughId, playthroughId);

	if (tainted) {
		Array<String> flags;
		flags.push_back("tainted_save_loaded");
		markActivePlaythroughCasual(flags);
		queuePlaythroughAuditEvent("tainted_save_loaded", true, "assisted", flags, "{\"tainted\":true}");
	}
	if (!isActivePlaythroughSyncEligible() || playthroughId != getActivePlaythroughId())
		return;

	CEAchievementEvent event;
	event.requestType = "playthrough_save";
	event.method = "POST";
	event.path = "/playthroughs/" + getActivePlaythroughId() + "/saves";
	event.eventId = makePlaythroughEventId("save_load");
	event.playthrough = getActivePlaythroughId();
	event.clientSaveId = String::format("%s:slot:%d", clientPlaythroughId.c_str(), slot);
	event.slot = String::format("%d", slot);
	event.label = saveName;
	event.saveHash = hashSaveFile(saveName);
	event.tainted = tainted;
	event.assistanceFlags = tainted ? getAssistanceFlags() : Array<String>();
	event.occurredAt = currentTimeString();
	event.lastLoadedAt = event.occurredAt;
	event.attemptCount = 0;
	event.state = "pending";
	event.signature = signEvent(event);
	queueEvent(event);
	retryQueuedEvents();
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
	if (action == kCEAchievementSyncQueue) {
		Array<CEAchievementEvent> events = loadQueue();
		if (queueIndex >= 0 && queueIndex < (int)events.size() && events[queueIndex].method == "PATCH")
			request->usePatch();
	}
	_syncAction = action;
	_syncQueueIndex = queueIndex;
	ConnMan.addRequest(request);
#else
	finishRequest("HTTP support is not available in this build.");
#endif
}

void CEAchievementService::testLinkToken() {
	startRequest(kCEAchievementSyncTestToken, getApiBaseUrl() + "/link");
}

void CEAchievementService::retryQueuedEvents() {
	submitNextQueuedEvent();
}

void CEAchievementService::submitNextQueuedEvent() {
	if (_syncAction != kCEAchievementSyncIdle)
		return;
	Array<CEAchievementEvent> events = loadQueue();
	if (resolveQueuedAchievementPlaythroughs(events))
		saveQueue(events);
	bool waitingForPlaythrough = false;
	for (uint i = 0; i < events.size(); ++i) {
		if (events[i].state == "synced" || events[i].state == "permanent_failure")
			continue;
		if (events[i].requestType == "achievement" && !events[i].clientPlaythroughId.empty() && events[i].playthrough.empty()) {
			waitingForPlaythrough = true;
			continue;
		}
		if (!verifyEventSignature(events[i])) {
			if (events[i].requestType != "achievement") {
				events[i].state = "permanent_failure";
				events[i].lastError = "Queue signature mismatch.";
			} else {
				events[i].assisted = true;
				events[i].historyStatus = "assisted";
				events[i].assistanceFlags.push_back("queue_signature_mismatch");
				events[i].eventId = makeEventId(events[i].game, events[i].variant, events[i].achievement, true, events[i].clientPlaythroughId);
				events[i].lastError = "Queue signature mismatch; downgraded to assisted.";
				events[i].signature = signEvent(events[i]);
			}
			saveQueue(events);
			if (events[i].state == "permanent_failure")
				continue;
		}

		events[i].attemptCount++;
		events[i].lastAttemptAt = currentTimeString();
		saveQueue(events);
		startRequest(kCEAchievementSyncQueue, getApiBaseUrl() + events[i].path, canonicalizeEvent(events[i]), i);
		return;
	}
	finishRequest(waitingForPlaythrough ? "Achievement event is waiting for playthrough sync." : "Achievement queue is empty.");
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
			if (events[_syncQueueIndex].requestType == "playthrough_start") {
				JSONValue *root = JSON::parse(response);
				if (root && root->isObject()) {
					const JSONObject &obj = root->asObject();
					if (obj.contains("playthrough") && obj["playthrough"]->isObject()) {
						const JSONObject &playthrough = obj["playthrough"]->asObject();
						const String playthroughId = ceStringFromJson(playthrough, "id");
						const String modeStatus = ceStringFromJson(playthrough, "mode_status");
						if (!playthroughId.empty()) {
							events[_syncQueueIndex].playthrough = playthroughId;
							const bool stillActive = events[_syncQueueIndex].clientPlaythroughId == getActiveClientPlaythroughId() &&
								getActivePlaythroughStatus() == "active";
							if (stillActive) {
								setActivePlaythrough(getActivePlaythroughTarget(), events[_syncQueueIndex].game, events[_syncQueueIndex].variant,
									events[_syncQueueIndex].clientPlaythroughId, playthroughId, "active", modeStatus == "casual", true);
							}
							updatePlaythroughRecord(events[_syncQueueIndex].clientPlaythroughId, playthroughId, stillActive ? "active" : String(), modeStatus == "casual", stillActive);
						}
					}
				}
				delete root;
				resolveQueuedAchievementPlaythroughs(events);
			}
			events[_syncQueueIndex].state = "synced";
			events[_syncQueueIndex].lastError.clear();
			saveQueue(events);
			finishRequest(events[_syncQueueIndex].requestType == "achievement" ? "Achievement event synced." : "CE playthrough event synced.");
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
