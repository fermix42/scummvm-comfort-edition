/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_CLOCK_VALIDATION_H
#define NOX_NATIVE_CLOCK_VALIDATION_H

#include "nox/native_clock.h"
#include "common/str.h"

namespace Nox {
inline bool validateNativeClock(Common::String &report, Common::String &error) {
	Native::TurnClock midnight; midnight.phase = 1; midnight.hour = 0x23; midnight.minute = 0x59;
	midnight.lightTicks = 1; midnight.lightActive = 1; midnight.hourlyCounter = 1;
	Native::ClockAdvance step;
	if (!Native::advanceTurnClock(midnight, step) || !step.minuteAdvanced || !step.hourAdvanced ||
		step.after.phase || step.after.hour || step.after.minute || step.after.hourlyCounter || step.after.lightTicks || step.after.lightActive != 1) {
		error = "Native clock changed midnight/counter/light boundary"; return false;
	}
	// Original expiry clears on the following minute, not on decrement1->0.
	step.after.phase = 1;
	if (!Native::advanceTurnClock(step.after, step) || step.after.lightActive || step.after.minute != 1 || step.hourAdvanced) {
		error = "Native clock changed original delayed light expiry"; return false;
	}
	Native::TurnClock day; uint minutes = 0, hours = 0;
	for (uint turn = 0; turn < 2880; ++turn) {
		if (!Native::advanceTurnClock(day, step)) { error = "Native daily clock refused valid time"; return false; }
		minutes += step.minuteAdvanced ? 1 : 0; hours += step.hourAdvanced ? 1 : 0; day = step.after;
	}
	if (day.hour || day.minute || day.phase || minutes != 1440 || hours != 24) {
		error = "Native daily clock BCD carry mismatch"; return false;
	}
	report += "native-clock\thalf_turns=2880\tminutes=1440\thours=24\tmidnight=1\toriginal_light_expiry=1\towner=rule-unit-only\n";
	return true;
}
} // namespace Nox
#endif
