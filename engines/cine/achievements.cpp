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
 */

#include "cine/achievements.h"
#include "cine/gfx.h"
#include "cine/object.h"
#include "cine/script.h"
#include "cine/various.h"

#include "common/config-manager.h"

namespace Cine {

static bool s_operationStealthParkContactEscapeNoted = false;
static bool s_operationStealthBananaOrderNoted = false;
static const char *const kOperationStealthWellReadTitlesMaskKey = "ce_os_well_read_titles_mask";

static bool matchesAnyScriptLine(int scriptLine, int first, int second) {
	return scriptLine == first || scriptLine == second;
}
void checkOperationStealthRazorRecordingAchievementMessage(int scriptIndex, int scriptLine, byte messageIdx) {
	if (g_cine->getGameType() != Cine::GType_OS ||
			scumm_stricmp(currentPrcName, "AIRPORT.PRC") != 0 ||
			(scumm_stricmp(currentRelName, "AEROPORT.REL") != 0 &&
			 scumm_stricmp(currentRelName, "AIRPORT.REL") != 0) ||
			scumm_stricmp(currentMsgName, "AEROPORT.MSG") != 0 ||
			scriptIndex != 95 || scriptLine != 95 || messageIdx != 143) {
		return;
	}

	noteOperationStealthAchievementEvent("razor_recording_heard");
}

void checkOperationStealthPassportAchievementMessage(int scriptIndex, int scriptLine, byte messageIdx) {
	if (g_cine->getGameType() != Cine::GType_OS ||
			scumm_stricmp(currentPrcName, "AIRPORT.PRC") != 0 ||
			(scumm_stricmp(currentRelName, "AEROPORT.REL") != 0 &&
			 scumm_stricmp(currentRelName, "AIRPORT.REL") != 0) ||
			messageIdx != 37 || !matchesAnyScriptLine(scriptLine, 303, 309)) {
		return;
	}

	int expectedScript = -1;
	switch (g_cine->_globalVars[1]) {
	case 0:
		expectedScript = 55;
		break;
	case 1:
		expectedScript = 56;
		break;
	case 2:
		expectedScript = 57;
		break;
	default:
		return;
	}

	if (scriptIndex == expectedScript)
		noteOperationStealthAchievementEvent("customs_passport_accepted");
}

void checkOperationStealthDirectAchievementMessage(int scriptIndex, int scriptLine, byte messageIdx) {
	if (g_cine->getGameType() != Cine::GType_OS)
		return;

	if (scumm_stricmp(currentRelName, "DOUCHE.REL") == 0 && scumm_stricmp(currentMsgName, "DOUCHE.MSG") == 0) {
		if (scriptIndex == 55 && matchesAnyScriptLine(scriptLine, 66, 67) && messageIdx == 55 && getObjectParam(39, 5) == -99) {
			noteOperationStealthAchievementEvent("authorized_mission_submitted");
			return;
		}

		if (scriptIndex == 51 && scriptLine == 0 && messageIdx == 51) {
			noteOperationStealthAchievementEvent("mission_order_read");
			return;
		}
	}

	if ((scumm_stricmp(currentRelName, "DOUCHE.REL") == 0 || scumm_stricmp(currentRelName, "SALLE59.REL") == 0) &&
			(scumm_stricmp(currentPrcName, "DOUCHE6.PRC") == 0 || scumm_stricmp(currentPrcName, "SALLE59.PRC") == 0) &&
			scriptIndex == 92 && matchesAnyScriptLine(scriptLine, 164, 160) && messageIdx == 167 && g_cine->_globalVars[97] == 1) {
		noteOperationStealthAchievementEvent("stealth_virus_installed");
		return;
	}

	if (scumm_stricmp(currentPrcName, "SOUSMAR2.PRC") == 0 &&
			scumm_stricmp(currentRelName, "SOUSMARI.REL") == 0 &&
			scumm_stricmp(currentMsgName, "SOUSMARI.MSG") == 0 &&
			scriptIndex == 35 && matchesAnyScriptLine(scriptLine, 2251, 2261) && messageIdx == 49 &&
			g_cine->_globalVars[240] == 55) {
#ifdef CINE_TRACE_BUILD
		traceCineRuntime("ceCandidate.under_new_management.accepted",
			"script=%d line=%d msg=%d bg=%s v20=%d v240=%d",
			scriptIndex, scriptLine, messageIdx, renderer ? renderer->getBgName() : "",
			g_cine->_globalVars[20], g_cine->_globalVars[240]);
#endif
		noteOperationStealthAchievementEvent("underwater_base_entered");
		return;
	}

	if (scumm_stricmp(currentPrcName, "PALAIS1.PRC") == 0 &&
			scumm_stricmp(currentRelName, "PALAIS.REL") == 0 &&
			scumm_stricmp(currentMsgName, "PALAIS.MSG") == 0 &&
			scriptIndex == 22 && matchesAnyScriptLine(scriptLine, 393, 451) && messageIdx == 29 &&
			g_cine->_globalVars[240] == 50 && g_cine->_globalVars[243] == -1 &&
			getObjectParam(164, 5) == -3) {
#ifdef CINE_TRACE_BUILD
		traceCineRuntime("ceCandidate.making_waves.accepted",
			"script=%d line=%d msg=%d bg=%s obj110y=%d obj111y=%d obj164=%d v240=%d v243=%d",
			scriptIndex, scriptLine, messageIdx, renderer ? renderer->getBgName() : "",
			getObjectParam(110, 2), getObjectParam(111, 2), getObjectParam(164, 5),
			g_cine->_globalVars[240], g_cine->_globalVars[243]);
#endif
		noteOperationStealthAchievementEvent("jetski_chase_completed");
		return;
	}

	if (scumm_stricmp(currentRelName, "PALAIS.REL") == 0 && scumm_stricmp(currentMsgName, "PALAIS.MSG") == 0 &&
			scriptIndex == 27 && messageIdx >= 41 && messageIdx <= 45) {
		const uint32 titleBit = 1 << (messageIdx - 41);
		uint32 titleMask = ConfMan.hasKey(kOperationStealthWellReadTitlesMaskKey) ?
			ConfMan.getInt(kOperationStealthWellReadTitlesMaskKey) : 0;
		titleMask |= titleBit;
		ConfMan.setInt(kOperationStealthWellReadTitlesMaskKey, titleMask);

#ifdef CINE_TRACE_BUILD
		traceCineRuntime("ceCandidate.well_read_intruder.progress",
			"script=%d line=%d msg=%d mask=%u complete=%d",
			scriptIndex, scriptLine, messageIdx, titleMask, titleMask == 0x1F ? 1 : 0);
#endif
		if (titleMask == 0x1F)
			noteOperationStealthAchievementEvent("humorous_book_read");
		return;
	}

}

void checkOperationStealthBananaOrderAchievementAfterAwardMessage(int scriptIndex, int scriptLine, byte objIdx, int16 x, int16 y, int16 mask, int16 frame) {
	if (g_cine->getGameType() != Cine::GType_OS ||
			scumm_stricmp(currentPrcName, "FIN2.PRC") != 0 ||
			scumm_stricmp(currentMsgName, "FIN.MSG") != 0 ||
			scriptIndex != 15 || scriptLine != 150 ||
			objIdx != 20 || x != 74 || y != 39 || mask != 1 || frame != 104 ||
			s_operationStealthBananaOrderNoted) {
		return;
	}

	s_operationStealthBananaOrderNoted = true;
	noteOperationStealthAchievementEvent("banana_order_awarded");
}

void checkOperationStealthBankAmbushAchievement(int scriptIndex, int scriptLine, byte varIdx, int16 newValue) {
	const bool isAmbushTransition =
		(scriptIndex == 163 && scriptLine == 16) ||
		(scriptIndex == 164 && scriptLine == 27);

	if (g_cine->getGameType() != Cine::GType_OS ||
			scumm_stricmp(currentRelName, "VILLE.REL") != 0 ||
			scumm_stricmp(currentMsgName, "VILLE.MSG") != 0 ||
			!isAmbushTransition ||
			varIdx != 240 || newValue != 26 ||
			g_cine->_globalVars[241] != 101) {
		return;
	}

	if (getObjectParam(79, 5) == -3) {
#ifdef CINE_TRACE_BUILD
		traceCineRuntime("ceCandidate.safe_deposit_unsafe_withdrawal.accepted",
			"script=%d line=%d v241=%d obj78=%d obj79=%d",
			scriptIndex, scriptLine, g_cine->_globalVars[241],
			getObjectParam(78, 5), getObjectParam(79, 5));
#endif
		noteOperationStealthAchievementEvent("bank_ambush_reached");
#ifdef CINE_TRACE_BUILD
	} else {
		traceCineRuntime("ceCandidate.safe_deposit_unsafe_withdrawal.rejected",
			"script=%d line=%d v241=%d obj78=%d obj79=%d",
			scriptIndex, scriptLine, g_cine->_globalVars[241],
			getObjectParam(78, 5), getObjectParam(79, 5));
#endif
	}
}

void checkOperationStealthMineEscapeAchievement(int scriptIndex, int scriptLine, byte varIdx, int16 newValue) {
	if (g_cine->getGameType() != Cine::GType_OS ||
			scumm_stricmp(currentPrcName, "AIRPORT.PRC") != 0 ||
			scriptIndex != 25 || !matchesAnyScriptLine(scriptLine, 319, 335) ||
			varIdx != 240 || newValue != 21 ||
			g_cine->_globalVars[241] != 1) {
		return;
	}

#ifdef CINE_TRACE_BUILD
	traceCineRuntime("ceCandidate.escape_clause.accepted",
		"script=%d line=%d v241=%d obj1x=%d obj1y=%d obj83=%d",
		scriptIndex, scriptLine, g_cine->_globalVars[241],
		getObjectParam(1, 1), getObjectParam(1, 2), getObjectParam(83, 5));
#endif
	noteOperationStealthAchievementEvent("mine_tunnels_escaped");
}

void checkOperationStealthJuliaRescueAchievement(int scriptIndex, int scriptLine, byte varIdx, int16 newValue) {
	if (g_cine->getGameType() != Cine::GType_OS ||
			scumm_stricmp(currentPrcName, "BATEAU7.PRC") != 0 ||
			scriptIndex != 27 || !matchesAnyScriptLine(scriptLine, 623, 652) ||
			varIdx != 240 || newValue != 42 ||
			g_cine->_globalVars[241] != 1) {
		return;
	}

#ifdef CINE_TRACE_BUILD
	traceCineRuntime("ceCandidate.two_for_the_surface.accepted",
		"script=%d line=%d v241=%d obj1frame=%d obj2frame=%d obj201frame=%d obj202frame=%d",
		scriptIndex, scriptLine, g_cine->_globalVars[241],
		getObjectParam(1, 4), getObjectParam(2, 4),
		getObjectParam(201, 4), getObjectParam(202, 4));
#endif
	noteOperationStealthAchievementEvent("julia_rescued_underwater");
}

void checkOperationStealthPiranhaCageEscapeAchievement(int scriptIndex, int scriptLine, byte varIdx, int16 oldValue, int16 newValue) {
	if (g_cine->getGameType() != Cine::GType_OS ||
			scumm_stricmp(currentPrcName, "DOUCHE6.PRC") != 0 ||
			scumm_stricmp(currentRelName, "DOUCHE.REL") != 0 ||
			scumm_stricmp(currentMsgName, "DOUCHE.MSG") != 0 ||
			scriptIndex != 113 || !matchesAnyScriptLine(scriptLine, 188, 191) ||
			varIdx != 20 || oldValue != 1 || newValue != 20) {
		return;
	}

#ifdef CINE_TRACE_BUILD
	traceCineRuntime("ceCandidate.pen_mightier_than_piranhas.accepted",
		"script=%d line=%d bg=%s obj1=%d,%d obj79=%d,%d obj80=%d,%d v110=%d v111=%d v112=%d v20=%d",
		scriptIndex, scriptLine, renderer ? renderer->getBgName() : "",
		getObjectParam(1, 1), getObjectParam(1, 2),
		getObjectParam(79, 1), getObjectParam(79, 2),
		getObjectParam(80, 1), getObjectParam(80, 2),
		g_cine->_globalVars[110], g_cine->_globalVars[111],
		g_cine->_globalVars[112], g_cine->_globalVars[20]);
#endif
	noteOperationStealthAchievementEvent("piranha_cage_escaped");
}

void checkOperationStealthRatMazeAchievement(int scriptIndex, int scriptLine, byte varIdx, int16 oldValue, int16 newValue) {
	if (g_cine->getGameType() != Cine::GType_OS ||
			scumm_stricmp(currentPrcName, "EGOU.PRC") != 0 ||
			scumm_stricmp(currentRelName, "LABY.REL") != 0 ||
			scumm_stricmp(currentMsgName, "LABY.MSG") != 0 ||
			scriptIndex != 9 || !matchesAnyScriptLine(scriptLine, 523, 451) ||
			varIdx != 240 || oldValue != 8 || newValue != 10 ||
			g_cine->_globalVars[229] != 9) {
		return;
	}

#ifdef CINE_TRACE_BUILD
	traceCineRuntime("ceCandidate.rats_all_folks.accepted",
		"script=%d line=%d bg=%s v229=%d v240=%d v251=%d v252=%d",
		scriptIndex, scriptLine, renderer ? renderer->getBgName() : "",
		g_cine->_globalVars[229], g_cine->_globalVars[240],
		g_cine->_globalVars[251], g_cine->_globalVars[252]);
#endif
	noteOperationStealthAchievementEvent("rat_mazes_cleared");
}

void checkOperationStealthSoldierDisguiseAchievement(int scriptIndex, int scriptLine, byte varIdx, int16 oldValue, int16 newValue) {
	if (g_cine->getGameType() != Cine::GType_OS ||
			scumm_stricmp(currentPrcName, "DOUCHE6.PRC") != 0 ||
			scumm_stricmp(currentRelName, "DOUCHE.REL") != 0 ||
			scumm_stricmp(currentMsgName, "DOUCHE.MSG") != 0 ||
			scriptIndex != 42 || scriptLine != 147 ||
			varIdx != 242 || oldValue != 1 || newValue != 0 ||
			getObjectParam(58, 5) != -3 || getObjectParam(55, 5) != -3) {
		return;
	}

#ifdef CINE_TRACE_BUILD
	traceCineRuntime("ceCandidate.dressed_to_infiltrate.accepted",
		"script=%d line=%d bg=%s obj58=%d obj55=%d v242=%d v240=%d",
		scriptIndex, scriptLine, renderer ? renderer->getBgName() : "",
		getObjectParam(58, 5), getObjectParam(55, 5),
		g_cine->_globalVars[242], g_cine->_globalVars[240]);
#endif
	noteOperationStealthAchievementEvent("soldier_disguise_acquired");
}

void checkOperationStealthFingerprintDoorAchievement(int scriptIndex, int scriptLine, byte varIdx, int16 oldValue, int16 newValue) {
	if (g_cine->getGameType() != Cine::GType_OS ||
			scumm_stricmp(currentPrcName, "DOUCHE6.PRC") != 0 ||
			scumm_stricmp(currentRelName, "DOUCHE.REL") != 0 ||
			scumm_stricmp(currentMsgName, "DOUCHE.MSG") != 0 ||
			scriptIndex != 7 || scriptLine != 646 ||
			varIdx != 240 || oldValue != 110 || newValue != 57 ||
			g_cine->_globalVars[241] != 3 ||
			g_cine->_globalVars[242] != 0 ||
			getObjectParam(102, 5) != -3) {
		return;
	}

#ifdef CINE_TRACE_BUILD
	traceCineRuntime("ceCandidate.fingerprint_fiction.accepted",
		"script=%d line=%d bg=%s v240=%d v241=%d v242=%d obj100=%d obj102=%d",
		scriptIndex, scriptLine, renderer ? renderer->getBgName() : "",
		g_cine->_globalVars[240], g_cine->_globalVars[241],
		g_cine->_globalVars[242], getObjectParam(100, 4), getObjectParam(102, 5));
#endif
	noteOperationStealthAchievementEvent("fingerprint_door_passed");
}

void checkOperationStealthPalaceOfficeAchievement(int scriptIndex, int scriptLine, byte varIdx, int16 newValue) {
	if (g_cine->getGameType() != Cine::GType_OS ||
			scumm_stricmp(currentPrcName, "LABY.PRC") != 0 ||
			scumm_stricmp(currentRelName, "LABY.REL") != 0 ||
			scumm_stricmp(currentMsgName, "LABY.MSG") != 0 ||
			scriptIndex != 9 || !matchesAnyScriptLine(scriptLine, 482, 425) ||
			varIdx != 240 || newValue != 46 ||
			g_cine->_globalVars[229] != 9) {
		return;
	}

#ifdef CINE_TRACE_BUILD
	traceCineRuntime("ceCandidate.palace_intrigue.accepted",
		"script=%d line=%d bg=%s v229=%d v240=%d v241=%d v243=%d",
		scriptIndex, scriptLine, renderer ? renderer->getBgName() : "",
		g_cine->_globalVars[229], g_cine->_globalVars[240],
		g_cine->_globalVars[241], g_cine->_globalVars[243]);
#endif
	noteOperationStealthAchievementEvent("palace_office_reached");
}

void checkOperationStealthPalaceSafeEnvelopeAchievement(int scriptIndex, int scriptLine, byte objIdx, byte paramIdx, int16 oldValue, int16 newValue) {
	if (g_cine->getGameType() != Cine::GType_OS ||
			scumm_stricmp(currentPrcName, "PALAIS1.PRC") != 0 ||
			scumm_stricmp(currentRelName, "PALAIS.REL") != 0 ||
			scumm_stricmp(currentMsgName, "PALAIS.MSG") != 0 ||
			scriptIndex != 11 || scriptLine != 2 ||
			objIdx != 164 || paramIdx != 5 ||
			oldValue != -1 || newValue != -3) {
		return;
	}

#ifdef CINE_TRACE_BUILD
	traceCineRuntime("ceCandidate.a_safe_bet.accepted",
		"script=%d line=%d bg=%s obj164=%d v240=%d v241=%d v243=%d",
		scriptIndex, scriptLine, renderer ? renderer->getBgName() : "",
		getObjectParam(164, 5), g_cine->_globalVars[240],
		g_cine->_globalVars[241], g_cine->_globalVars[243]);
#endif
	noteOperationStealthAchievementEvent("palace_safe_envelope_recovered");
}

void resetOperationStealthParkContactAchievementLatch(int scriptIndex, byte varIdx, int16 newValue) {
	if (g_cine->getGameType() == Cine::GType_OS &&
			scumm_stricmp(currentPrcName, "AIRPORT.PRC") == 0 &&
			(scriptIndex == 0 || scriptIndex == 1) &&
			varIdx == 10 && newValue == 0) {
		s_operationStealthParkContactEscapeNoted = false;
	}
}

void checkOperationStealthParkContactAchievement(int scriptIndex, int scriptLine, byte varIdx, int16 newValue) {
	if (s_operationStealthParkContactEscapeNoted)
		return;

	if (g_cine->getGameType() != Cine::GType_OS ||
			scumm_stricmp(currentPrcName, "AIRPORT.PRC") != 0 ||
			scriptIndex != 13 || !matchesAnyScriptLine(scriptLine, 271, 279) ||
			varIdx != 240 || newValue != 43 ||
			g_cine->_globalVars[11] != 1 ||
			getObjectParam(88, 5) != 3) {
#ifdef CINE_TRACE_BUILD
		if (g_cine->getGameType() == Cine::GType_OS &&
				scumm_stricmp(currentPrcName, "AIRPORT.PRC") == 0 &&
				scriptIndex == 13 && varIdx == 240 && newValue == 43) {
			traceCineRuntime("ceCandidate.say_it_with_flowers.rejected",
				"script=%d line=%d v11=%d v241=%d obj88=%d obj65=%d obj66=%d obj67=%d",
				scriptIndex, scriptLine, g_cine->_globalVars[11],
				g_cine->_globalVars[241], getObjectParam(88, 5), getObjectParam(65, 5),
				getObjectParam(66, 5), getObjectParam(67, 5));
		}
#endif
		return;
	}

	if (getObjectParam(65, 5) == -3) {
#ifdef CINE_TRACE_BUILD
		traceCineRuntime("ceCandidate.say_it_with_flowers.accepted",
			"script=%d line=%d v11=%d v241=%d obj88=%d obj65=%d obj66=%d obj67=%d",
			scriptIndex, scriptLine, g_cine->_globalVars[11],
			g_cine->_globalVars[241], getObjectParam(88, 5), getObjectParam(65, 5),
			getObjectParam(66, 5), getObjectParam(67, 5));
#endif
		s_operationStealthParkContactEscapeNoted = true;
		noteOperationStealthAchievementEvent("park_contact_info_obtained");
#ifdef CINE_TRACE_BUILD
	} else if (g_cine->getGameType() == Cine::GType_OS &&
			scumm_stricmp(currentPrcName, "AIRPORT.PRC") == 0 &&
			scriptIndex == 13 && varIdx == 240 && newValue == 43) {
		traceCineRuntime("ceCandidate.say_it_with_flowers.rejected",
			"script=%d line=%d v11=%d v241=%d obj88=%d obj65=%d obj66=%d obj67=%d",
			scriptIndex, scriptLine, g_cine->_globalVars[11],
			g_cine->_globalVars[241], getObjectParam(88, 5), getObjectParam(65, 5),
			getObjectParam(66, 5), getObjectParam(67, 5));
#endif
	}
}

void checkOperationStealthPassportAchievement(int scriptIndex, int scriptLine, byte varIdx, int16 oldValue, int16 newValue) {
	if (g_cine->getGameType() != Cine::GType_OS ||
			scumm_stricmp(currentPrcName, "AIRPORT.PRC") != 0 ||
			varIdx != 241 || oldValue != 2 || newValue != 3) {
		return;
	}

	// The correct customs passport is selected at airport setup:
	// 0=French, 1=English, 2=German.
	int passportObject = 0;
	switch (g_cine->_globalVars[1]) {
	case 0:
		passportObject = 10;
		break;
	case 1:
		passportObject = 41;
		break;
	case 2:
		passportObject = 42;
		break;
	default:
		return;
	}

	if (scriptIndex == 6 && matchesAnyScriptLine(scriptLine, 364, 355) && getObjectParam(passportObject, 5) == -3)
		noteOperationStealthAchievementEvent("customs_passport_accepted");
}


} // End of namespace Cine