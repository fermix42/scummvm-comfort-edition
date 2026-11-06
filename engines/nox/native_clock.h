/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_CLOCK_H
#define NOX_NATIVE_CLOCK_H

#include "common/scummsys.h"

namespace Nox {
namespace Native {
struct TurnClock {
	byte phase, hour, minute, lightTicks, lightActive, hourlyCounter;
	TurnClock() : phase(0), hour(0), minute(0), lightTicks(0), lightActive(0), hourlyCounter(0) {}
};
struct ClockAdvance {
	TurnClock after;
	bool minuteAdvanced, hourAdvanced;
	ClockAdvance() : minuteAdvanced(false), hourAdvanced(false) {}
};

inline bool advanceTurnClock(const TurnClock &before, ClockAdvance &output) {
	if (before.phase > 1 || (before.hour & 15) > 9 || before.hour >= 0x24 ||
		(before.minute & 15) > 9 || before.minute >= 0x60) return false;
	ClockAdvance next; next.after = before;
	if (++next.after.phase == 2) {
		next.after.phase = 0; next.minuteAdvanced = true;
		if (!next.after.lightTicks) next.after.lightActive = 0;
		else {
			--next.after.lightTicks;
			if (next.after.lightTicks & 128) next.after.lightActive = 0;
		}
		uint minute = (before.minute >> 4) * 10 + (before.minute & 15) + 1;
		if (minute == 60) {
			minute = 0; next.hourAdvanced = true;
			if (next.after.hourlyCounter) --next.after.hourlyCounter;
			const uint hour = ((before.hour >> 4) * 10 + (before.hour & 15) + 1) % 24;
			next.after.hour = byte((hour / 10) * 16 + hour % 10);
		}
		next.after.minute = byte((minute / 10) * 16 + minute % 10);
	}
	output = next; return true;
}
} // namespace Native
} // namespace Nox
#endif
