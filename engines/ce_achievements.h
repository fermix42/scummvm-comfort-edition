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
#include "common/hash-str.h"
#include "common/hashmap.h"
#include "common/str.h"

namespace Common {

class AchievementsManager;

enum CEAchievementSyncAction {
	kCEAchievementSyncIdle,
	kCEAchievementSyncTestToken,
	kCEAchievementSyncQueue
};

struct CEAchievementEvent {
	CEAchievementEvent() : assisted(false), tainted(false), attemptCount(0) {}

	String requestType;
	String method;
	String path;
	String eventId;
	String game;
	String variant;
	String achievement;
	String playthrough;
	String clientPlaythroughId;
	String title;
	String status;
	String clientSaveId;
	String slot;
	String label;
	String saveHash;
	String metadata;
	bool assisted;
	bool tainted;
	String historyStatus;
	Array<String> assistanceFlags;
	String occurredAt;
	String lastLoadedAt;
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
	String iconPath;
	bool challengeEligible;
	bool hidden;
};

enum CEAchievementPopupLocation {
	kCEAchievementPopupUpperLeft,
	kCEAchievementPopupUpperRight,
	kCEAchievementPopupLowerRight,
	kCEAchievementPopupTopCenter,
	kCEAchievementPopupBottomCenter
};

struct CEAchievementRule {
	String game;
	String variant;
	String event;
	String achievement;
};

struct CEPlaythroughRecord {
	CEPlaythroughRecord() : casual(false), archived(false), syncEligible(false) {}

	String target;
	String game;
	String variant;
	String clientPlaythroughId;
	String playthroughId;
	String title;
	String status;
	String archivedFromStatus;
	String startedAt;
	String endedAt;
	bool casual;
	bool archived;
	bool syncEligible;
};

class CEAchievementService {
	friend class CEHttpRequest;
public:
	CEAchievementService();

	bool unlock(AchievementsManager &achievements, const String &game, const String &variant, const String &achievement);
	bool noteGameEvent(AchievementsManager &achievements, const String &game, const String &variant, const String &event);
	bool startPlaythroughForTarget(const String &target, const String &game, const String &variant, const String &title);
	bool endActivePlaythrough(const String &status);
	bool hasActivePlaythroughInProgress() const;
	bool isActivePlaythroughSyncEligible(const String &game = String(), const String &variant = String()) const;
	bool hasAchievementSupport(const String &game, const String &variant) const;
	String getCurrentPlaythroughForTarget(const String &target) const;
	Array<CEPlaythroughRecord> listPlaythroughsForTarget(const String &target) const;
	String getDefaultLoadPlaythroughForTarget(const String &target) const;
	String getSavePlaythroughForTargetSlot(const String &target, int slot) const;
	bool setPlaythroughStatusForTarget(const String &target, const String &clientPlaythroughId, const String &status);
	bool setPlaythroughArchivedForTarget(const String &target, const String &clientPlaythroughId, bool archived);
	bool deletePlaythroughForTarget(const String &target, const String &clientPlaythroughId) const;
	bool clearSavePlaythroughForTargetSlot(const String &target, int slot, const String &clientPlaythroughId) const;
	void recordSaveCreated(const String &saveName, int slot, const String &label);
	void recordSaveLoaded(const String &saveName, int slot);
	void resetPopupState();
	void loadPopupStateForSave(const String &saveName);
	void savePopupStateForSave(const String &saveName);
	uint32 getPopupProgress(const String &key) const;
	void setPopupProgress(const String &key, uint32 value);

	bool isChallengeModeEnabled() const;
	void setChallengeModeEnabled(bool enabled);
	bool areCheatsEnabled(const String &domain = String()) const;
	void setCheatsEnabled(bool enabled, const String &domain = String());
	bool shouldApplyCheat(const String &key, const String &domain = String()) const;
	bool isRepeatPopupsEnabled() const;
	void setRepeatPopupsEnabled(bool enabled);
	bool arePopupsEnabled() const;
	void setPopupsEnabled(bool enabled);
	CEAchievementPopupLocation getPopupLocation() const;
	void setPopupLocation(CEAchievementPopupLocation location);
	bool isPopupSoundEnabled() const;
	void setPopupSoundEnabled(bool enabled);

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
	bool queueEvent(const CEAchievementEvent &event);
	bool queuePlaythroughAuditEvent(const String &type, bool assisted, const String &historyStatus, const Array<String> &flags, const String &metadata);
	String canonicalizeEvent(const CEAchievementEvent &event) const;
	String signEvent(const CEAchievementEvent &event) const;
	bool verifyEventSignature(const CEAchievementEvent &event) const;
	String getOrCreateInstallSecret() const;
	String getApiBaseUrl() const;
	String getActivePlaythroughId() const;
	String getActiveClientPlaythroughId() const;
	String getActivePlaythroughTarget() const;
	String getActivePlaythroughGame() const;
	String getActivePlaythroughVariant() const;
	String getActivePlaythroughStatus() const;
	bool isActivePlaythroughCasual() const;
	void setActivePlaythrough(const String &target, const String &game, const String &variant, const String &clientPlaythroughId, const String &playthroughId, const String &status, bool casual, bool syncEligible) const;
	void clearActivePlaythrough() const;
	void markActivePlaythroughCasual(const Array<String> &flags) const;
	Array<CEPlaythroughRecord> loadPlaythroughRegistry() const;
	bool savePlaythroughRegistry(const Array<CEPlaythroughRecord> &records) const;
	void upsertPlaythroughRecord(const CEPlaythroughRecord &record) const;
	void updatePlaythroughRecord(const String &clientPlaythroughId, const String &playthroughId, const String &status, bool casual, bool syncEligible, const String &endedAt = String()) const;
	bool activatePlaythroughRecordForLoadedSave(const String &clientPlaythroughId, const String &playthroughId) const;
	String makePlaythroughId(const String &target) const;
	String makePlaythroughEventId(const String &type) const;
	String makeEventId(const String &game, const String &variant, const String &achievement, bool assisted, const String &clientPlaythroughId = String()) const;
	String makePopupSeenKey(const String &game, const String &variant, const String &achievement, bool assisted) const;
	String makePopupSidecarName(const String &saveName) const;
	String hashSaveFile(const String &saveName) const;
	bool savePlaythroughSidecar(const String &sidecarName, bool tainted, const String &clientPlaythroughId, const String &playthroughId) const;
	bool loadPlaythroughSidecar(const String &sidecarName, bool &tainted, String &clientPlaythroughId, String &playthroughId) const;
	bool resolveQueuedAchievementPlaythroughs(Array<CEAchievementEvent> &events) const;
	bool hasPopupBeenSeen(const String &key) const;
	void markPopupSeen(const String &key);
	bool loadPopupSidecar(const String &sidecarName);
	bool savePopupSidecar(const String &sidecarName) const;
	String currentTimeString() const;
	void startRequest(CEAchievementSyncAction action, const String &url, const String &body = String(), int queueIndex = -1);
	void requestSucceeded(const String &response, long httpCode);
	void requestFailed(const String &response, long httpCode);
	void submitNextQueuedEvent();
	void finishRequest(const String &status);

	CEAchievementSyncAction _syncAction;
	int _syncQueueIndex;
	String _popupSidecarName;
	Array<String> _popupSeenKeys;
	HashMap<String, uint32> _popupProgress;
	mutable bool _catalogLoaded;
	mutable Array<CEAchievementDefinition> _catalog;
	mutable Array<CEAchievementRule> _rules;
};

CEAchievementService &CEAchievements();

} // End of namespace Common

#endif
