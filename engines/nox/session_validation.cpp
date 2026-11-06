/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nox/nox.h"
#include "engines/ce_achievements.h"
#include "common/config-manager.h"
#include "common/savefile.h"

namespace Nox {
bool NoxEngine::validateSessions(Common::String &error) {
	Common::CEAchievementService &service = Common::CEAchievements();
	// This driver is only for isolated offline harness configurations. It must
	// never submit synthetic game identities or test sessions to a website.
	if (!service.getLinkToken().empty()) { error = "Session validation requires no CE Link token"; return false; }
	const Common::String target = ConfMan.getActiveDomainName();
	if (!service.hasPlaythroughSupportForTarget(target, "nox-archaist", "apple2-v119") ||
	    service.hasPlaythroughSupportForTarget(target, "nox-archaist", "apple2-v137")) {
		error = "Registered base Nox session capability mismatch"; return false;
	}
	const Common::String game = "ce-offline-session-fixture", variant = "no-awards";
	if (service.hasAchievementSupport(game, variant)) { error = "Session fixture unexpectedly has awards"; return false; }
	Common::Array<Common::CEPlaythroughRecord> records = service.listPlaythroughsForTarget(target);
	const bool cold = !records.empty();
	if (!cold) {
		ConfMan.set("ce_achievements_game", game, target);
		ConfMan.set("ce_achievements_variant", variant, target);
		ConfMan.setBool("ce_playthrough_tracking", false, target);
		if (service.hasPlaythroughSupportForTarget(target, game, variant)) { error = "Session opt-in bypassed"; return false; }
		ConfMan.setBool("ce_playthrough_tracking", true, target);
		if (!service.hasPlaythroughSupportForTarget(target, game, variant) ||
		    service.hasPlaythroughSupportForTarget(target, game, "wrong-variant") ||
		    service.hasPlaythroughSupportForTarget(target, "wrong-game", variant)) { error = "Session capability identity validation failed"; return false; }
		ConfMan.removeKey("ce_achievements_variant", target);
		if (service.hasPlaythroughSupportForTarget(target, game, variant)) { error = "Missing variant accepted"; return false; }
		ConfMan.set("ce_achievements_variant", variant, target);
		if (!service.hasPlaythroughSupportForTarget(target, "operation-stealth", "dos-vga")) { error = "Existing catalog support regressed"; return false; }
		for (uint i = 0; i < 2; ++i) {
			if (!service.startPlaythroughForTarget(target, game, variant, i ? "Offline fixture B" : "Offline fixture A")) { error = "Could not start CE session"; return false; }
			const Common::String file = i ? "nox-session-fixture-b.dat" : "nox-session-fixture-a.dat";
			Common::OutSaveFile *out = getSaveFileManager()->openForSaving(file, false);
			if (!out) { error = "Cannot create session metadata fixture"; return false; }
			out->writeUint32BE(MKTAG('T','E','S','T')); out->finalize();
			const bool failed = out->err(); delete out;
			if (failed) { error = "Cannot persist session metadata fixture"; return false; }
			service.savePopupStateForSave(file);
			service.recordSaveCreated(file, 1, "Offline metadata fixture");
			if (service.getSavePlaythroughForFile(file) != service.getCurrentPlaythroughForTarget(target)) { error = "CE save/session association failed"; return false; }
		}
		records = service.listPlaythroughsForTarget(target);
	}
	if (records.size() != 2 || records[0].clientPlaythroughId == records[1].clientPlaythroughId || !records[0].syncEligible || !records[1].syncEligible) { error = "CE sessions are not distinct offline records"; return false; }
	const Common::String a = service.getSavePlaythroughForFile("nox-session-fixture-a.dat");
	const Common::String b = service.getSavePlaythroughForFile("nox-session-fixture-b.dat");
	if (a.empty() || b.empty() || a == b) { error = "CE metadata fixtures share session identity"; return false; }
	service.loadPopupStateForSave("nox-session-fixture-a.dat");
	service.recordSaveLoaded("nox-session-fixture-a.dat", 1);
	if (service.getCurrentPlaythroughForTarget(target) != a) { error = "CE load did not resume session A"; return false; }
	service.loadPopupStateForSave("nox-session-fixture-b.dat");
	service.recordSaveLoaded("nox-session-fixture-b.dat", 1);
	if (service.getCurrentPlaythroughForTarget(target) != b || service.listPlaythroughsForTarget(target).size() != 2 || service.getPendingEventCount() != (cold ? 8u : 6u)) { error = "CE session switch changed owner count or lost durable offline events"; return false; }
	const Common::String report = Common::String::format("PASS\tphase=%s\tsessions=2\tawards=0\tqueued_events=%u\tnetwork_requests=0\n", cold ? "cold" : "create", service.getPendingEventCount());
	Common::OutSaveFile *out = getSaveFileManager()->openForSaving(cold ? "nox-session-cold.tsv" : "nox-session-create.tsv", false);
	if (!out) { error = "Cannot open session validation report"; return false; }
	out->write(report.c_str(), report.size()); out->finalize();
	const bool failed = out->err(); delete out;
	if (failed) { error = "Cannot persist session validation report"; return false; }
	return true;
}
} // namespace Nox
