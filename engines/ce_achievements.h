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

#ifndef ENGINES_CE_ACHIEVEMENTS_H
#define ENGINES_CE_ACHIEVEMENTS_H

#include "common/array.h"
#include "common/str.h"

namespace Common {

class AchievementsManager;

enum CEAchievementSyncAction {
	kCEAchievementSyncIdle,
	kCEAchievementSyncTestToken,
	kCEAchievementSyncQueue
};

struct CEAchievementEvent {
	String eventId;
	String game;
	String variant;
	String achievement;
	bool assisted;
	String historyStatus;
	Array<String> assistanceFlags;
	String occurredAt;
	uint32 attemptCount;
	String lastAttemptAt;
	String lastError;
	String state;
	String signature;
};

struct CEAchievementDefinition {
	String game;
	String variant;
	String id;
	String title;
	String description;
	bool challengeEligible;
	bool hidden;
};

struct CEAchievementRule {
	String game;
	String variant;
	String event;
	String achievement;
};

class CEAchievementService {
	friend class CEHttpRequest;
public:
	CEAchievementService();

	bool unlock(AchievementsManager &achievements, const String &game, const String &variant, const String &achievement);
	bool noteGameEvent(AchievementsManager &achievements, const String &game, const String &variant, const String &event);

	bool isChallengeModeEnabled() const;
	void setChallengeModeEnabled(bool enabled);
	bool areCheatsEnabled(const String &domain = String()) const;
	void setCheatsEnabled(bool enabled, const String &domain = String());
	bool shouldApplyCheat(const String &key, const String &domain = String()) const;
	bool isRepeatPopupsEnabled() const;
	void setRepeatPopupsEnabled(bool enabled);

	bool isDisqualifyingAssistanceActive(const String &domain = String()) const;
	void disableDisqualifyingAssistance();
	void noteDisqualifyingAssistance(const String &flag);
	Array<String> getAssistanceFlags(const String &domain = String()) const;

	String getLinkToken() const;
	void setLinkToken(const String &token);
	uint32 getPendingEventCount() const;
	String getLastSyncStatus() const;
	void retryQueuedEvents();
	void testLinkToken();
	bool isSyncBusy() const { return _syncAction != kCEAchievementSyncIdle; }

	String getAchievementId(const String &game, uint index) const;
	String getAchievementTitle(const String &game, const String &variant, const String &achievement) const;
	const char *const *getOperationStealthAchievementKeys() const;

private:
	void loadCatalogIfNeeded() const;
	void addBuiltinCatalog() const;
	void loadBundledCatalog() const;
	void loadExternalCatalog() const;
	void addCatalogDefinition(const CEAchievementDefinition &definition) const;
	const CEAchievementDefinition *findDefinition(const String &game, const String &variant, const String &achievement) const;
	void addBuiltinRules() const;
	void addCatalogRule(const CEAchievementRule &rule) const;
	const CEAchievementRule *findRule(const String &game, const String &variant, const String &event) const;
	Array<CEAchievementEvent> loadQueue() const;
	bool saveQueue(const Array<CEAchievementEvent> &events) const;
	bool queueAchievementEvent(const String &game, const String &variant, const String &achievement);
	String canonicalizeEvent(const CEAchievementEvent &event) const;
	String signEvent(const CEAchievementEvent &event) const;
	bool verifyEventSignature(const CEAchievementEvent &event) const;
	String getOrCreateInstallSecret() const;
	String makeEventId(const String &game, const String &variant, const String &achievement, bool assisted) const;
	String currentTimeString() const;
	void startRequest(CEAchievementSyncAction action, const String &url, const String &body = String(), int queueIndex = -1);
	void requestSucceeded(const String &response, long httpCode);
	void requestFailed(const String &response, long httpCode);
	void submitNextQueuedEvent();
	void finishRequest(const String &status);

	CEAchievementSyncAction _syncAction;
	int _syncQueueIndex;
	mutable bool _catalogLoaded;
	mutable Array<CEAchievementDefinition> _catalog;
	mutable Array<CEAchievementRule> _rules;
};

CEAchievementService &CEAchievements();

} // End of namespace Common

#endif
