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


#include "common/config-manager.h"
#include "common/debug.h"
#include "common/stream.h"
#include "common/system.h"
#include "common/translation.h"
#include "common/compression/unzip.h"

#include "engines/achievements.h"
#include "engines/ce_achievements.h"

namespace Common {

DECLARE_SINGLETON(AchievementsManager);

AchievementsManager::AchievementsManager() {
	_iniFile = nullptr;
	unsetActiveDomain();
}


AchievementsManager::~AchievementsManager() {
}

bool AchievementsManager::setActiveDomain(const AchievementsInfo &info) {
	if (info.appId.empty()) {
		unsetActiveDomain();
		return false;
	}

	const char* platform = info.platform == STEAM_ACHIEVEMENTS ? "steam" :
					info.platform == GALAXY_ACHIEVEMENTS ? "galaxy" :
					"achman";

	String iniFileName = String::format("%s-%s.dat", platform, info.appId.c_str());

	if (_iniFileName == iniFileName) {
		return true;
	}

	if (isReady()) {
		unsetActiveDomain();
	}

	debug(2, "AchievementsManager::setActiveDomain(): '%s'", iniFileName.c_str());

	_iniFileName = iniFileName;

	_iniFile = new Common::INIFile();
	_iniFile->loadFromSaveFile(_iniFileName); // missing file is OK

	loadAchievementsData(platform, info.appId.c_str());

	for (uint32 i = 0; i < _stats.size(); i++) {
		if (!(_iniFile->hasKey(_stats[i].id, "statistics"))) {
			_iniFile->setKey(_stats[i].id, "statistics", _stats[i].start);
		}
	}

	setSpecialString("platform", platform);
	setSpecialString("gameId", info.appId);

	return true;
}


String AchievementsManager::getCurrentLang() const {
#ifdef USE_TRANSLATION
	String uiLang = TransMan.getCurrentLanguage().c_str();
	if (_achievements.contains(uiLang)) {
		return uiLang;
	}
#endif

	return "en";
}


bool AchievementsManager::loadAchievementsData(const char *platform, const char *appId) {
	Archive *cfgZip = nullptr;

	if (!cfgZip && ConfMan.hasKey("extrapath")) {
		Common::FSDirectory extrapath(ConfMan.getPath("extrapath"));
		cfgZip = Common::makeZipArchive(extrapath.createReadStreamForMember("achievements.dat"));
	}

	if (!cfgZip) {
		cfgZip = Common::makeZipArchive("achievements.dat");
	}

	if (!cfgZip) {
		warning("achievements.dat is not found. Achievements messages are unavailable");
		return false;
	}

	SeekableReadStream *verStream = cfgZip->createReadStreamForMember("VERSION");
	if (!verStream) {
		delete cfgZip;
		warning("VERSION file is not found in achievements.dat. Achievements messages are unavailable");
		return false;
	}

	String version = verStream->readLine();
	delete verStream;

	if (version != "1") {
		delete cfgZip;
		warning("Incompatible VERSION file in achievements.dat. Achievements messages are unavailable");
		return false;
	}

	String cfgFileName = String::format("%s-%s.ini", platform, appId);
	SeekableReadStream *stream = cfgZip->createReadStreamForMember(Common::Path(cfgFileName));
	if (!stream) {
		delete cfgZip;
		warning("%s is not found in achievements.dat. Achievements messages are unavailable", cfgFileName.c_str());
		return false;
	}

	INIFile cfgFile;
	if (!cfgFile.loadFromStream(*stream)) {
		delete stream;
		delete cfgZip;
		warning("%s is corrupted in achievements.dat. Achievements messages are unavailable", cfgFileName.c_str());
		return false;
	}

	_achievements.clear();
	INIFile::SectionList sections = cfgFile.getSections();
	for (const auto &section : sections) {
		if (!(section.name.hasPrefix("achievements:"))) {
			continue;
		}

		String lang = section.name.substr(13); //strlen("achievements:")

		for (int i = 0; i < 256; i++) {
			String prefix = String::format("item_%d", i);

			String id      = section.getKey(prefix + "_id")      ? section.getKey(prefix + "_id")->value      : "";
			String title   = section.getKey(prefix + "_title")   ? section.getKey(prefix + "_title")->value   : "";
			String comment = section.getKey(prefix + "_comment") ? section.getKey(prefix + "_comment")->value : "";
			String hidden  = section.getKey(prefix + "_hidden")  ? section.getKey(prefix + "_hidden")->value  : "";

			if (id.empty()) {
				break;
			} else {
				AchievementDescription desc = { id, title, comment, !hidden.empty() };
				_achievements[lang].push_back(desc);
			}
		}
	}

	_stats.clear();
	for (int i = 0; i < 256; i++) {
		String prefix = String::format("item_%d", i);

		String id, comment, start;
		cfgFile.getKey(prefix + "_id", "stats:en", id);
		cfgFile.getKey(prefix + "_comment", "stats:en", comment);
		cfgFile.getKey(prefix + "_start", "stats:en", start);

		if (id.empty()) {
			break;
		} else {
			StatDescription desc = {id, comment, start};
			_stats.push_back(desc);
		}
	}

	delete stream;
	delete cfgZip;
	return true;
}


bool AchievementsManager::unsetActiveDomain() {
	debug(2, "AchievementsManager::unsetActiveDomain()");

	_iniFileName = "";

	delete _iniFile;
	_iniFile = nullptr;

	_achievements.clear();
	_stats.clear();

	return true;
}


bool AchievementsManager::setAchievement(const String &id) {
	return setAchievement(id, true);
}

bool AchievementsManager::setAchievement(const String &id, bool showOsd) {
	if (!isReady()) {
		warning("AchievementsManager::setAchievement('%s'): AchMan not ready, did you forget to call setActiveDomain()?", id.c_str());
		return false;
	}
	if (isAchieved(id)) {
		return true;
	}

	const String &lang = getCurrentLang();

	String displayedMessage = id;
	if (_achievements.contains(lang)) {
		for (uint32 i = 0; i < _achievements[lang].size(); i++) {
			if (_achievements[lang][i].id == id) {
				displayedMessage = _achievements[lang][i].title;
				break;
			}
		}
	}

	debug(2, "AchievementsManager::setAchievement('%s'): '%s'", id.c_str(), displayedMessage.c_str());

	_iniFile->setKey(id, "achievements", "true");
	_iniFile->saveToSaveFile(_iniFileName);

	if (showOsd && !ConfMan.getBool("disable_achievement_unlocked_osd") && !displayedMessage.empty() && g_system) {
		U32String msg;
		msg = Common::U32String::format("%S\n%S",
			_("Achievement unlocked!").c_str(),
			Common::U32String(displayedMessage).c_str()
		);
		g_system->displayMessageOnOSD(msg);
	}

	return true;
}


bool AchievementsManager::isAchieved(const String &id) const {
	if (!isReady()) {
		return false;
	}

	return _iniFile->hasKey(id, "achievements");
}


bool AchievementsManager::clearAchievement(const String &id) {
	if (!isReady()) {
		return false;
	}

	_iniFile->removeKey(id, "achievements");
	_iniFile->saveToSaveFile(_iniFileName);
	return true;
}


bool AchievementsManager::setStatFloatEx(const String &id, float value, const String &section) const {
	if (!isReady()) {
		return false;
	}

	String tmp = Common::String::format("%8.8f", value);
	_iniFile->setKey(id, section, tmp);
	_iniFile->saveToSaveFile(_iniFileName);
	return 0;
}


float AchievementsManager::getStatFloatEx(const String &id, const String &section) const {
	if (!isReady()) {
		return 0.0;
	}

	String tmp;
	_iniFile->getKey(id, section, tmp);
	return (float)atof(tmp.c_str());
}


bool AchievementsManager::setStatFloat(const String &id, float value) {
	return setStatFloatEx(id, value, "statistics");
}


float AchievementsManager::getStatFloat(const String &id) const {
	return getStatFloatEx(id, "statistics");
}


bool AchievementsManager::updateAverageRateStatFloat(const String &id, float count, float times) {
	if (!isReady()) {
		return false;
	}

	float old_count = getStatFloatEx(id + "_count", "rates");
	float old_times = getStatFloatEx(id + "_times", "rates");

	setStatFloatEx(id + "_count", old_count + count, "rates");
	setStatFloatEx(id + "_times", old_times + times, "rates");

	return 0;
}


float AchievementsManager::getAverageRateStatFloat(const String &id) const {
	if (!isReady()) {
		return 0.0;
	}

	float count = getStatFloatEx(id + "_count", "rates");
	float times = getStatFloatEx(id + "_times", "rates");

	return (times != 0) ? (count / times) : 0.0;
}


bool AchievementsManager::setStatInt(String const &id, int value) {
	if (!isReady()) {
		return false;
	}

	String tmp = Common::String::format("%d", value);
	_iniFile->setKey(id, "statistics", tmp);
	_iniFile->saveToSaveFile(_iniFileName);
	return 0;
}


int AchievementsManager::getStatInt(String const &id) const {
	if (!isReady()) {
		return 0;
	}

	String tmp;
	_iniFile->getKey(id, "statistics", tmp);
	return (int)atol(tmp.c_str());
}


const String AchievementsManager::getStatRaw(String const &id) const {
	if (!isReady()) {
		return "";
	}

	String tmp;
	_iniFile->getKey(id, "statistics", tmp);
	return tmp;
}


bool AchievementsManager::setSpecialString(String const &id, String const &value) {
	if (!isReady()) {
		return false;
	}

	_iniFile->setKey(id, "special", value);
	_iniFile->saveToSaveFile(_iniFileName);
	return 0;
}


bool AchievementsManager::resetAllAchievements() {
	if (!isReady()) {
		return false;
	}

	_iniFile->removeSection("achievements");
	_iniFile->saveToSaveFile(_iniFileName);
	return 0;
}


bool AchievementsManager::resetAllStats() {
	if (!isReady()) {
		return false;
	}

	_iniFile->removeSection("statistics");
	_iniFile->removeSection("rates");
	_iniFile->saveToSaveFile(_iniFileName);
	return 0;
}


uint16 AchievementsManager::getAchievementCount() const {
	if (!isReady()) {
		return 0;
	}

	const String &lang = getCurrentLang();
	if (!_achievements.contains(lang)) {
		return 0;
	}

	return _achievements[lang].size();
}


const AchievementDescription *AchievementsManager::getAchievementDescription(uint16 index) const {
	if (!isReady()) {
		return nullptr;
	}

	const String &lang = getCurrentLang();
	if (!_achievements.contains(lang)) {
		return nullptr;
	}

	if (index >= _achievements[lang].size()) {
		return nullptr;
	}

	return &(_achievements[lang][index]);
}


uint16 AchievementsManager::getStatCount() const {
	if (!isReady()) {
		return 0;
	}

	return _stats.size();
}


const StatDescription *AchievementsManager::getStatDescription(uint16 index) const {
	if (!isReady()) {
		return nullptr;
	}

	if (index >= _stats.size()) {
		return nullptr;
	}

	return &(_stats[index]);
}

bool AchievementsManager::setCEAchievement(const String &game, const String &variant, const String &achievement) {
	return CEAchievements().unlock(*this, game, variant, achievement);
}

bool AchievementsManager::noteCEGameEvent(const String &game, const String &variant, const String &event) {
	return CEAchievements().noteGameEvent(*this, game, variant, event);
}

bool AchievementsManager::isCEChallengeModeEnabled() const {
	return CEAchievements().isChallengeModeEnabled();
}

void AchievementsManager::setCEChallengeModeEnabled(bool enabled) {
	CEAchievements().setChallengeModeEnabled(enabled);
}

bool AchievementsManager::areCECheatsEnabled(const String &domain) const {
	return CEAchievements().areCheatsEnabled(domain);
}

void AchievementsManager::setCECheatsEnabled(bool enabled, const String &domain) {
	CEAchievements().setCheatsEnabled(enabled, domain);
}

bool AchievementsManager::shouldApplyCECheat(const String &key, const String &domain) const {
	return CEAchievements().shouldApplyCheat(key, domain);
}

bool AchievementsManager::isCERepeatPopupsEnabled() const {
	return CEAchievements().isRepeatPopupsEnabled();
}

void AchievementsManager::setCERepeatPopupsEnabled(bool enabled) {
	CEAchievements().setRepeatPopupsEnabled(enabled);
}

bool AchievementsManager::areCEPopupsEnabled() const {
	return CEAchievements().arePopupsEnabled();
}

void AchievementsManager::setCEPopupsEnabled(bool enabled) {
	CEAchievements().setPopupsEnabled(enabled);
}

CEAchievementPopupLocation AchievementsManager::getCEPopupLocation() const {
	return CEAchievements().getPopupLocation();
}

void AchievementsManager::setCEPopupLocation(CEAchievementPopupLocation location) {
	CEAchievements().setPopupLocation(location);
}

bool AchievementsManager::isCEPopupSoundEnabled() const {
	return CEAchievements().isPopupSoundEnabled();
}

void AchievementsManager::setCEPopupSoundEnabled(bool enabled) {
	CEAchievements().setPopupSoundEnabled(enabled);
}

bool AchievementsManager::isCEDisqualifyingAssistanceActive(const String &domain) const {
	return CEAchievements().isDisqualifyingAssistanceActive(domain);
}

void AchievementsManager::disableCEDisqualifyingAssistance() {
	CEAchievements().disableDisqualifyingAssistance();
}

void AchievementsManager::noteCEDisqualifyingAssistance(const String &flag) {
	CEAchievements().noteDisqualifyingAssistance(flag);
}

Array<String> AchievementsManager::getCEAssistanceFlags(const String &domain) const {
	return CEAchievements().getAssistanceFlags(domain);
}

String AchievementsManager::getCELinkToken() const {
	return CEAchievements().getLinkToken();
}

void AchievementsManager::setCELinkToken(const String &token) {
	CEAchievements().setLinkToken(token);
}

uint32 AchievementsManager::getCEPendingEventCount() const {
	return CEAchievements().getPendingEventCount();
}

String AchievementsManager::getCELastSyncStatus() const {
	return CEAchievements().getLastSyncStatus();
}

void AchievementsManager::retryCEQueuedEvents() {
	CEAchievements().retryQueuedEvents();
}

void AchievementsManager::testCELinkToken() {
	CEAchievements().testLinkToken();
}

bool AchievementsManager::isCESyncBusy() const {
	return CEAchievements().isSyncBusy();
}

String AchievementsManager::getCEAchievementId(const String &game, uint index) const {
	return CEAchievements().getAchievementId(game, index);
}

const char *const *AchievementsManager::getCEOperationStealthAchievementKeys() const {
	return CEAchievements().getOperationStealthAchievementKeys();
}

} // End of namespace Common
