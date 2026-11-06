// SPDX-License-Identifier: GPL-3.0-or-later
#include <cxxtest/TestSuite.h>
#include "common/config-manager.h"
#include "common/formats/json.h"
#include "common/fs.h"
#include "common/savefile.h"
#include "common/system.h"
#include "engines/achievements.h"
#include "engines/ce_achievements.h"
#include "engines/grim/ce_ps2_saves.h"
#include "engines/grim/ce_emi_windows_saves.h"
#include "../system/null_osystem.h"

class CEAcceptanceTestSuite : public CxxTest::TestSuite {
	Common::String _directory;
	Common::String read(const char *name) {
		Common::InSaveFile *in = g_system->getSavefileManager()->openForLoading(name);
		if (!in) return Common::String();
		Common::String value = in->readString(0, in->size());
		delete in;
		return value;
	}
	void write(const char *name, const Common::String &value) {
		Common::OutSaveFile *out = g_system->getSavefileManager()->openForSaving(name, false);
		TS_ASSERT(out);
		if (!out) return;
		out->write(value.c_str(), value.size()); out->finalize();
		TS_ASSERT(!out->err()); delete out;
	}
	uint achievements() {
		Common::JSONValue *root = Common::JSON::parse(read("ce-achievements-queue.json"));
		if (!root || !root->isArray()) { delete root; return 0; }
		uint count = 0;
		for (const auto *row : root->asArray())
			if (row->asObject().getVal("request_type")->asString() == "achievement") ++count;
		delete root; return count;
	}
public:
	void setUp() {
		Common::install_null_g_system_with_saves();
		Common::FSNode parent("test/ce-acceptance"); parent.createDirectory();
		uint run = 0;
		do { _directory = Common::String::format("test/ce-acceptance/run-%u", run++); }
		while (Common::FSNode(Common::Path(_directory)).exists());
		TS_ASSERT(Common::FSNode(Common::Path(_directory)).createDirectory());
		Common::WriteStream *cfg = Common::FSNode(Common::Path(_directory + "/isolated.ini")).createWriteStream();
		TS_ASSERT(cfg);
		if (cfg) { const char initial[] = "[scummvm]\n"; cfg->write(initial, sizeof(initial) - 1); cfg->finalize(); delete cfg; }
		ConfMan.loadConfigFile(Common::Path(_directory + "/isolated.ini"), Common::Path());
		ConfMan.setPath("savepath", Common::Path(_directory), Common::ConfigManager::kApplicationDomain);
		ConfMan.addGameDomain("ce-acceptance"); ConfMan.setActiveDomain("ce-acceptance");
		ConfMan.set("ce_link_token", "", Common::ConfigManager::kApplicationDomain);
		ConfMan.set("ce_achievements_api_base_url", "http://127.0.0.1:1/api/v1", Common::ConfigManager::kApplicationDomain);
		ConfMan.setBool("ce_achievements_challenge_mode", true, Common::ConfigManager::kApplicationDomain);
		ConfMan.setBool("ce_achievements_popups_enabled", false, Common::ConfigManager::kApplicationDomain);
		ConfMan.flushToDisk();
		Common::CEAchievements().resetPopupState();
	}
	void tearDown() { Common::uninstall_null_g_system(); }

	void testOfflineUnlockOwnershipAndDuplicateSuppression() {
		Common::CEAchievementService service;
		Common::AchievementsManager manager;
		TS_ASSERT(service.startPlaythroughForTarget("ce-acceptance", "operation-stealth", "dos-vga", "A"));
		Common::String owner = service.getCurrentPlaythroughForTarget("ce-acceptance");
		TS_ASSERT(service.noteGameEvent(manager, "operation-stealth", "dos-vga", "customs_passport_accepted"));
		Common::String first = read("ce-achievements-queue.json");
		TS_ASSERT_EQUALS(achievements(), 1U);
		TS_ASSERT(first.contains(owner));
		TS_ASSERT(first.contains("passport_to_trouble"));
		TS_ASSERT(service.noteGameEvent(manager, "operation-stealth", "dos-vga", "customs_passport_accepted"));
		TS_ASSERT_EQUALS(achievements(), 1U);
		TS_ASSERT_EQUALS(read("ce-achievements-queue.json"), first);
		service.unlock(manager, "operation-stealth", "amiga", "passport_to_trouble");
		TS_ASSERT_EQUALS(achievements(), 1U);
		service.retryQueuedEvents();
		TS_ASSERT(!service.isSyncBusy());
		TS_ASSERT_EQUALS(read("ce-achievements-queue.json"), first);
		TS_ASSERT(first.contains("\"attempt_count\":0"));
	}

	void testRepeatUnlockInIndependentSessionAndTerminalStates() {
		Common::CEAchievementService service; Common::AchievementsManager manager;
		TS_ASSERT(service.startPlaythroughForTarget("ce-acceptance", "operation-stealth", "dos-vga", "A"));
		Common::String a = service.getCurrentPlaythroughForTarget("ce-acceptance");
		service.unlock(manager, "operation-stealth", "dos-vga", "passport_to_trouble");
		TS_ASSERT(service.endActivePlaythrough("completed"));
		service.unlock(manager, "operation-stealth", "dos-vga", "shave_and_a_briefing");
		TS_ASSERT_EQUALS(achievements(), 1U);
		// Human session-creation actions span timer ticks; IDs include getMillis.
		g_system->delayMillis(20);
		TS_ASSERT(service.startPlaythroughForTarget("ce-acceptance", "operation-stealth", "dos-vga", "B"));
		Common::String b = service.getCurrentPlaythroughForTarget("ce-acceptance");
		TS_ASSERT_DIFFERS(a, b);
		service.unlock(manager, "operation-stealth", "dos-vga", "passport_to_trouble");
		TS_ASSERT_EQUALS(achievements(), 2U);
		TS_ASSERT(service.endActivePlaythrough("abandoned"));
		Common::String queue = read("ce-achievements-queue.json");
		TS_ASSERT(queue.contains(a)); TS_ASSERT(queue.contains(b));
		TS_ASSERT(queue.contains("completed")); TS_ASSERT(queue.contains("abandoned"));
	}

	void testProgressSidecarAndServiceRecreation() {
		Common::CEAchievementService service;
		service.setPopupProgress("case_file_curious", 5);
		write("ce-progress.sav", "immutable test save payload");
		service.savePopupStateForSave("ce-progress.sav");
		Common::String original = read("ce-progress.sav");
		Common::CEAchievementService restarted;
		restarted.loadPopupStateForSave("ce-progress.sav");
		TS_ASSERT_EQUALS(restarted.getPopupProgress("case_file_curious"), 5U);
		restarted.resetPopupState();
		TS_ASSERT_EQUALS(restarted.getPopupProgress("case_file_curious"), 0U);
		restarted.loadPopupStateForSave("ce-progress.sav");
		TS_ASSERT_EQUALS(restarted.getPopupProgress("case_file_curious"), 5U);
		TS_ASSERT_EQUALS(read("ce-progress.sav"), original);
	}

	void testCheatGateAndPermanentSessionAssistance() {
		Common::CEAchievementService service; Common::AchievementsManager manager;
		TS_ASSERT(service.startPlaythroughForTarget("ce-acceptance", "operation-stealth", "dos-vga", "Challenge"));
		ConfMan.setBool("ce_test_cheat", true, "ce-acceptance");
		TS_ASSERT(!service.shouldApplyCheat("ce_test_cheat"));
		service.setCheatsEnabled(true);
		TS_ASSERT(!service.isChallengeModeEnabled());
		TS_ASSERT(service.shouldApplyCheat("ce_test_cheat"));
		service.noteDisqualifyingAssistance("ce_test_cheat");
		service.setChallengeModeEnabled(true);
		TS_ASSERT(!service.shouldApplyCheat("ce_test_cheat"));
		TS_ASSERT(!service.getAssistanceFlags().empty());
		service.unlock(manager, "operation-stealth", "dos-vga", "passport_to_trouble");
		TS_ASSERT(read("ce-achievements-queue.json").contains("\"assisted\":true"));
	}

	void testMI4WindowsSlotBoundariesAndSessionNamespaces() {
		for (int i = 0; i < 160; ++i) {
			TS_ASSERT_EQUALS(Grim::CEWindowsSaves::playerSlot(Grim::CEWindowsSaves::nativeFile(i)), i);
			TS_ASSERT_EQUALS(Grim::CEWindowsSaves::playerSlot(Grim::CEWindowsSaves::sessionFile("target", "A", i)), i);
		}
		TS_ASSERT_EQUALS(Grim::CEWindowsSaves::playerSlot("efmi000.gsv"), -1);
		TS_ASSERT_EQUALS(Grim::CEWindowsSaves::playerSlot("efmi161.gsv"), -1);
		TS_ASSERT_DIFFERS(Grim::CEWindowsSaves::sessionFile("target", "A", 0), Grim::CEWindowsSaves::sessionFile("target", "B", 0));
		TS_ASSERT_DIFFERS(Grim::CEWindowsSaves::sessionFile("target", "A", 0), Grim::CEWindowsSaves::sessionFile("target", "A", 0, 1));
	}

	void testMI4PS2SlotBoundariesAndSessionNamespaces() {
		for (int i = 0; i < 4; ++i) {
			TS_ASSERT_EQUALS(Grim::CEPS2Saves::playerSlot(Common::String::format("efmi%03d.ps2", i)), i);
			TS_ASSERT_EQUALS(Grim::CEPS2Saves::playerSlot(Grim::CEPS2Saves::sessionFile("target", "A", i)), i);
		}
		TS_ASSERT_EQUALS(Grim::CEPS2Saves::playerSlot("efmi004.ps2"), -1);
		TS_ASSERT_DIFFERS(Grim::CEPS2Saves::sessionFile("target", "A", 0), Grim::CEPS2Saves::sessionFile("target", "B", 0));
		TS_ASSERT_DIFFERS(Grim::CEPS2Saves::sessionFile("target", "A", 0), Grim::CEPS2Saves::sessionFile("other", "A", 0));
	}
};

// The normal suite runs these in order; the acceptance driver also selects each
// method in a separate fresh process to verify the actual disk restart boundary.
class CEColdProcessTestSuite : public CxxTest::TestSuite {
	Common::String readFile(const Common::Path &path) {
		Common::SeekableReadStream *in = Common::FSNode(path).createReadStream();
		if (!in) return Common::String();
		Common::String value = in->readString(0, in->size()); delete in; return value;
	}
	void writeFile(const Common::Path &path, const Common::String &value) {
		Common::WriteStream *out = Common::FSNode(path).createWriteStream();
		TS_ASSERT(out); if (!out) return;
		out->write(value.c_str(), value.size()); out->finalize(); delete out;
	}
public:
	void setUp() { Common::install_null_g_system_with_saves(); }
	void tearDown() { Common::uninstall_null_g_system(); }
	void testWriteOfflineAchievementFixture() {
		Common::FSNode("test/ce-cold").createDirectory();
		uint run = 0; Common::String directory;
		do { directory = Common::String::format("test/ce-cold/run-%u", run++); }
		while (Common::FSNode(Common::Path(directory)).exists());
		TS_ASSERT(Common::FSNode(Common::Path(directory)).createDirectory());
		Common::Path config(directory + "/isolated.ini");
		writeFile(config, "[scummvm]\n");
		ConfMan.loadConfigFile(config, Common::Path());
		ConfMan.setPath("savepath", Common::Path(directory), Common::ConfigManager::kApplicationDomain);
		ConfMan.addGameDomain("ce-cold"); ConfMan.setActiveDomain("ce-cold");
		ConfMan.setBool("ce_achievements_challenge_mode", true, Common::ConfigManager::kApplicationDomain);
		ConfMan.setBool("ce_achievements_popups_enabled", false, Common::ConfigManager::kApplicationDomain);
		Common::CEAchievementService service; Common::AchievementsManager manager;
		TS_ASSERT(service.startPlaythroughForTarget("ce-cold", "operation-stealth", "dos-vga", "Offline restart"));
		TS_ASSERT(service.noteGameEvent(manager, "operation-stealth", "dos-vga", "customs_passport_accepted"));
		service.setPopupProgress("case_file_curious", 7);
		service.savePopupStateForSave("cold.sav");
		ConfMan.flushToDisk();
		TS_ASSERT_EQUALS(service.getPendingEventCount(), 2U);
		writeFile(Common::Path("test/ce-cold/current-config.txt"), directory + "/isolated.ini");
	}
	void testReadOfflineAchievementFixture() {
		const Common::String path = readFile(Common::Path("test/ce-cold/current-config.txt"));
		TS_ASSERT(!path.empty()); if (path.empty()) return;
		TS_ASSERT(ConfMan.loadConfigFile(Common::Path(path), Common::Path()));
		ConfMan.setActiveDomain("ce-cold");
		Common::CEAchievementService service; Common::AchievementsManager manager;
		TS_ASSERT(service.getLinkToken().empty());
		TS_ASSERT_EQUALS(service.getPendingEventCount(), 2U);
		service.loadPopupStateForSave("cold.sav");
		TS_ASSERT_EQUALS(service.getPopupProgress("case_file_curious"), 7U);
		const Common::String owner = service.getCurrentPlaythroughForTarget("ce-cold");
		TS_ASSERT(!owner.empty());
		TS_ASSERT_EQUALS(service.listPlaythroughsForTarget("ce-cold").size(), 1U);
		TS_ASSERT(service.noteGameEvent(manager, "operation-stealth", "dos-vga", "customs_passport_accepted"));
		service.retryQueuedEvents();
		TS_ASSERT(!service.isSyncBusy());
		TS_ASSERT_EQUALS(service.getPendingEventCount(), 2U);
	}
};
