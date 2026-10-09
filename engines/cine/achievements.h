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

#ifndef CINE_ACHIEVEMENTS_H
#define CINE_ACHIEVEMENTS_H

#include "cine/cine.h"

#include "common/platform.h"
#include "common/util.h"

#include "engines/achievements.h"

namespace Cine {

void checkOperationStealthRazorRecordingAchievementMessage(int scriptIndex, int scriptLine, byte messageIdx);
void checkOperationStealthPassportAchievementMessage(int scriptIndex, int scriptLine, byte messageIdx);
void checkOperationStealthDirectAchievementMessage(int scriptIndex, int scriptLine, byte messageIdx);
void checkOperationStealthBananaOrderAchievementAfterAwardMessage(int scriptIndex, int scriptLine, byte objIdx, int16 x, int16 y, int16 mask, int16 frame);
void checkOperationStealthBankAmbushAchievement(int scriptIndex, int scriptLine, byte varIdx, int16 newValue);
void checkOperationStealthMineEscapeAchievement(int scriptIndex, int scriptLine, byte varIdx, int16 newValue);
void checkOperationStealthJuliaRescueAchievement(int scriptIndex, int scriptLine, byte varIdx, int16 newValue);
void checkOperationStealthPiranhaCageEscapeAchievement(int scriptIndex, int scriptLine, byte varIdx, int16 oldValue, int16 newValue);
void checkOperationStealthRatMazeAchievement(int scriptIndex, int scriptLine, byte varIdx, int16 oldValue, int16 newValue);
void checkOperationStealthSoldierDisguiseAchievement(int scriptIndex, int scriptLine, byte varIdx, int16 oldValue, int16 newValue);
void checkOperationStealthFingerprintDoorAchievement(int scriptIndex, int scriptLine, byte varIdx, int16 oldValue, int16 newValue);
void checkOperationStealthPalaceOfficeAchievement(int scriptIndex, int scriptLine, byte varIdx, int16 newValue);
void checkOperationStealthPalaceSafeEnvelopeAchievement(int scriptIndex, int scriptLine, byte objIdx, byte paramIdx, int16 oldValue, int16 newValue);
void resetOperationStealthParkContactAchievementLatch(int scriptIndex, byte varIdx, int16 newValue);
void checkOperationStealthParkContactAchievement(int scriptIndex, int scriptLine, byte varIdx, int16 newValue);
void checkOperationStealthPassportAchievement(int scriptIndex, int scriptLine, byte varIdx, int16 oldValue, int16 newValue);

static inline Common::String operationStealthAchievementVariantKey() {
	if (!g_cine || g_cine->getPlatform() == Common::kPlatformDOS)
		return "dos-vga";
	if (g_cine->getPlatform() == Common::kPlatformAmiga)
		return "amiga";
	if (g_cine->getPlatform() == Common::kPlatformAtariST)
		return "atari-st";
	return "dos-vga";
}

static inline void noteOperationStealthAchievementEvent(const Common::String &event) {
	if (!g_cine || g_cine->getGameType() != GType_OS)
		return;

	AchMan.noteCEGameEvent("operation-stealth", operationStealthAchievementVariantKey(), event);
}

} // End of namespace Cine

#endif
