/* ScummVM - Graphic Adventure Engine
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SCI_ENGINE_CE_SQ5_BATTLE_H
#define SCI_ENGINE_CE_SQ5_BATTLE_H

#include "common/scummsys.h"

namespace Sci {

enum CESQ5BattleHook {
	kCESQ5BattleNone,
	kCESQ5BattleReadProbes,
	kCESQ5BattleConsumeProbe,
	kCESQ5BattleScan,
	kCESQ5BattleFinishProbe
};

// DOS 1.03/1.04 (also used by Trash Talkin' Edition). Check the original
// launchProbe bytecode before interpreting its locals. Unknown scripts fail
// closed. No resource bytes are patched, so changing cheat settings or loading
// a save never leaves an enabled cheat baked into the script.
inline CESQ5BattleHook ceSQ5BattleHook(const byte *code, uint32 size, uint32 pc, uint32 localsCount) {
	if (localsCount != 385 || pc + 1 >= size || size < 0x2574)
		return kCESQ5BattleNone;

	CESQ5BattleHook hook = kCESQ5BattleNone;
	if ((code[pc] == 0x83 || code[pc] == 0x8b) && code[pc + 1] == 29)
		hook = kCESQ5BattleReadProbes;
	else if (pc == 0x230a)
		hook = kCESQ5BattleConsumeProbe;
	else if (pc == 0x2522)
		hook = kCESQ5BattleScan;
	else if (pc == 0x2559)
		hook = kCESQ5BattleFinishProbe;
	if (hook == kCESQ5BattleNone)
		return hook;

	static const byte start[] = {0x3f, 0x01, 0x87, 0x01, 0x65, 0x14, 0x36, 0x3c,
		0x35, 0x00, 0x1a, 0x30, 0xfc, 0x00, 0x8b, 0x20, 0x35, 0x00, 0x1a,
		0x30, 0x0c, 0x00, 0xe3, 0x1d};
	static const byte scan[] = {0x36, 0x40, 0xf7, 0xe5, 0x08, 0x7a, 0x8b, 0x05};
	static const byte finish[] = {0x8b, 0x20, 0x35, 0x00, 0x1a, 0x30, 0x0c, 0x00,
		0x38, 0x91, 0x00, 0x76, 0x72, 0x5c, 0x13, 0x4a, 0x04, 0x32, 0x05, 0x00,
		0x76, 0x40, 0xd4, 0xe0, 0x00};
	if (memcmp(code + 0x22f4, start, sizeof(start)) ||
		memcmp(code + 0x2521, scan, sizeof(scan)) ||
		memcmp(code + 0x2554, finish, sizeof(finish)))
		return kCESQ5BattleNone;
	return hook;
}

// Word exposes getOffset/setOffset, as SCI's reg_t does. Keeping these
// operations independent of the VM allows tests to exercise packed board
// updates and turn decisions without launching or altering the user's game.
template<class Word>
void ceApplySQ5BattleHook(CESQ5BattleHook hook, Word *locals, Word &acc,
		bool infiniteProbes, bool freeProbes, bool wholeBoard) {
	switch (hook) {
	case kCESQ5BattleReadProbes:
		// Restore availability even when loading a save with no probes left.
		if (infiniteProbes && locals[29].getOffset() == 0)
			locals[29].setOffset(1);
		break;
	case kCESQ5BattleConsumeProbe:
		// Refund immediately before the original decrement/display update.
		if (infiniteProbes && locals[32].getOffset() == 0 && locals[29].getOffset() < 65535)
			locals[29].setOffset(locals[29].getOffset() + 1);
		break;
	case kCESQ5BattleScan:
		if (wholeBoard && locals[32].getOffset() == 0) {
			// Two cells per word. Low three bits identify the ship; bit 4
			// reveals it. Preserve damage/shot flags and leave empty cells
			// and the player's own 150 board words untouched.
			for (uint i = 235; i < 385; ++i) {
				uint16 cells = locals[i].getOffset();
				if (cells & 0x0700)
					cells |= 0x1000;
				if (cells & 0x0007)
					cells |= 0x0010;
				locals[i].setOffset(cells);
			}
		}
		break;
	case kCESQ5BattleFinishProbe:
		// Take launchProbe's existing handsOn branch instead of cueing the
		// turn transition. The original cleanup, UI and disposal still run.
		if (freeProbes && locals[32].getOffset() == 0)
			acc.setOffset(0);
		break;
	default:
		break;
	}
}

} // namespace Sci

#endif
