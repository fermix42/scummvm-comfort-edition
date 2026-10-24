// SPDX-License-Identifier: GPL-3.0-or-later
// Standalone native checks; optionally pass a locally extracted script 850.
#define FORBIDDEN_SYMBOL_ALLOW_ALL
#include "sci/engine/ce_sq5_battle.h"

using namespace Sci;

struct Word {
	uint16 value;
	uint16 getOffset() const { return value; }
	void setOffset(uint16 v) { value = v; }
};

int main(int argc, char **argv) {
	for (uint flags = 0; flags < 8; ++flags) {
		const bool infinite = flags & 1, free = flags & 2, full = flags & 4;
		Word locals[385] = {}, acc = {1};
		locals[29].value = 2;
		ceApplySQ5BattleHook(kCESQ5BattleConsumeProbe, locals, acc, infinite, free, full);
		--locals[29].value; // Original VM instruction still executes.
		assert(locals[29].value == (infinite ? 2 : 1));
		for (uint shot = 0; shot < 20; ++shot) {
			if (!infinite && locals[29].value == 0)
				break;
			ceApplySQ5BattleHook(kCESQ5BattleConsumeProbe, locals, acc, infinite, free, full);
			--locals[29].value;
		}
		assert(locals[29].value == (infinite ? 2 : 0));
		ceApplySQ5BattleHook(kCESQ5BattleReadProbes, locals, acc, infinite, free, full);
		assert(locals[29].value == (infinite ? 2 : 0));
		locals[29].value = 0; // Exhausted-probe save restored with cheat on/off.
		ceApplySQ5BattleHook(kCESQ5BattleReadProbes, locals, acc, infinite, free, full);
		assert(locals[29].value == (infinite ? 1 : 0));

		// Both packed bytes, all sectors, empty cells, hit flags, and previously
		// revealed cells. Own board and all other locals must remain unchanged.
		Word before[385];
		for (uint i = 0; i < 385; ++i)
			locals[i].value = uint16((i % 32) | (((i / 3) % 32) << 8));
		locals[32].value = 0;
		memcpy(before, locals, sizeof(locals));
		ceApplySQ5BattleHook(kCESQ5BattleScan, locals, acc, infinite, free, full);
		for (uint i = 0; i < 385; ++i) {
			uint16 expected = before[i].value;
			if (full && i >= 235) {
				if (expected & 7) expected |= 0x10;
				if (expected & 0x700) expected |= 0x1000;
			}
			assert(locals[i].value == expected);
		}

		ceApplySQ5BattleHook(kCESQ5BattleFinishProbe, locals, acc, infinite, free, full);
		assert(acc.value == (free ? 0 : 1)); // Original bnt selects handsOn or turn cue.
		locals[32].value = 150; // Opponent's turn must never be affected.
		memcpy(before, locals, sizeof(locals));
		acc.value = 1;
		ceApplySQ5BattleHook(kCESQ5BattleConsumeProbe, locals, acc, true, true, true);
		ceApplySQ5BattleHook(kCESQ5BattleScan, locals, acc, true, true, true);
		ceApplySQ5BattleHook(kCESQ5BattleFinishProbe, locals, acc, true, true, true);
		assert(memcmp(before, locals, sizeof(locals)) == 0 && acc.value == 1);
	}

	if (argc == 2) {
		byte code[65536];
		FILE *file = fopen(argv[1], "rb");
		assert(file);
		const uint32 size = uint32(fread(code, 1, sizeof(code), file));
		fclose(file);
		assert(ceSQ5BattleHook(code, size, 0x230a, 385) == kCESQ5BattleConsumeProbe);
		assert(ceSQ5BattleHook(code, size, 0x2522, 385) == kCESQ5BattleScan);
		assert(ceSQ5BattleHook(code, size, 0x2559, 385) == kCESQ5BattleFinishProbe);
		uint reads = 0;
		for (uint i = 0; i + 1 < size; ++i)
			if (ceSQ5BattleHook(code, size, i, 385) == kCESQ5BattleReadProbes)
				++reads;
		assert(reads >= 3);
		assert(ceSQ5BattleHook(code, size, size, 385) == kCESQ5BattleNone);
		assert(ceSQ5BattleHook(code, size, 0x230a, 384) == kCESQ5BattleNone);
		assert(ceSQ5BattleHook(code, 10, 0x230a, 385) == kCESQ5BattleNone);
		code[0x2554] ^= 1;
		assert(ceSQ5BattleHook(code, size, 0x230a, 385) == kCESQ5BattleNone);
		printf("Original script 850: all hook signatures verified (%u probe reads).\n", reads);
	}
	puts("All eight cheat combinations, depleted saves, board flags, turn decisions and opponent isolation passed.");
	return 0;
}
