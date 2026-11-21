// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GUI_CE_SESSION_DATE_H
#define GUI_CE_SESSION_DATE_H

#include "common/str.h"
#include "common/system.h"
#include "common/translation.h"

namespace GUI {
inline Common::String formatSessionCreationDate(const TimeDate &local) {
	static const char *const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	if (local.tm_mon < 0 || local.tm_mon > 11)
		return _("Creation date unavailable").encode();
	return Common::String::format(_("Created %s %d, %d - %d:%02d %s").encode().c_str(),
		_(months[local.tm_mon]).encode().c_str(), local.tm_mday, local.tm_year + 1900,
		local.tm_hour % 12 == 0 ? 12 : local.tm_hour % 12, local.tm_min,
		local.tm_hour < 12 ? "AM" : "PM");
}
}
#endif
