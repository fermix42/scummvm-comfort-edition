/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_GATES_VALIDATION_H
#define NOX_NATIVE_GATES_VALIDATION_H

#include "nox/native_gates.h"
#include "nox/resources.h"

namespace Nox {
// Resource/semantic unit checks only. This does not claim that the ordinary CE
// command loop has stopped using its temporary execution bridge.
inline bool validateNativeGates(HdvArchive &archive, Common::String &report,
		Common::String &error) {
	uint maps = 0, keyed = 0, compounds = 0, plates = 0, portals = 0, cases = 0;
	const char *names[] = { "SURF", "UM", "RUIN", "TOWN", "TOWN2", "CSTL", "KEEP" };
	for (uint family = 0; family < ARRAYSIZE(names); ++family) {
		Common::Array<byte> data;
		if (!archive.read(Common::String("NA/DATA.MAP.") + names[family], data)) {
			error = archive.error(); return false;
		}
		uint offset = family ? 1 : 2;
		while (offset < data.size() && data[offset] != 255) {
			const uint size = data[offset] ? data[offset] : 256;
			if (size < 23 || size > data.size() - offset) {
				error = "Native gate validation: invalid map header"; return false;
			}
			const byte *header = data.data() + offset;
			const uint tables = uint(header[7]) | uint(header[8]) << 8 | uint(header[9]) << 16;
			if (tables > data.size() || data.size() - tables < 1792) {
				error = "Native gate validation: truncated map tables"; return false;
			}
			++maps;
			for (uint slot = 0; slot <= 252; slot += 4) {
				const byte *object = data.data() + tables + 1024 + slot;
				const uint length = 256 - slot;
				if (!object[2] || object[2] == 255) continue;
				const byte kind = object[3] & 0xbf;
				byte flags[256] = {};
				Native::QuestFields fields(flags, sizeof(flags));
				if (kind == 18 && length >= 6) {
					if (Native::keyDoor(object, length, fields) != Native::kGateLocked) {
						error = "Native key gate admitted absent authorization"; return false;
					}
					flags[object[4]] = object[5];
					if (Native::keyDoor(object, length, fields) != Native::kGateAllowed) {
						error = "Native key gate refused original authorization"; return false;
					}
					++keyed; cases += 2;
				} else if (kind == 19 && length >= 8) {
					const uint first = object[5], count = object[7];
					if (first >= 8 || !count || count > 8 - first) {
						error = "Native compound gate has unsupported authored bit range"; return false;
					}
					for (uint combination = 0; combination < (1U << count); ++combination) {
						flags[object[4]] = byte(combination << first);
						const bool allowed = combination == (1U << count) - 1;
						if ((Native::compoundDoor(object, length, fields) == Native::kGateAllowed) != allowed) {
							error = "Native compound gate changed a prerequisite combination"; return false;
						}
						++cases;
					}
					++compounds;
				} else if (kind == 54 && length >= 8) {
					flags[object[4]] = byte(~object[7]); byte after = 0;
					if (Native::pressurePlate(object, length, fields, after) != Native::kGateAllowed || after != 255) {
						error = "Native plate lost unrelated quest bits"; return false;
					}
					flags[object[4]] = after;
					if (Native::pressurePlate(object, length, fields, after) != Native::kGateAllowed || after != 255) {
						error = "Native plate repeat was not idempotent"; return false;
					}
					++plates; cases += 2;
				} else if (kind == 56 && length >= 12 && object[5]) {
					Native::PortalDestination dest = { 11, 22, 33, 44 };
					if (Native::portal(object, length, fields, true, dest) != Native::kGateLocked || dest.map != 22) {
						error = "Native portal absent-authorization refusal changed destination"; return false;
					}
					flags[object[9]] = object[11];
					if (Native::portal(object, length, fields, false, dest) != Native::kGateNeedsFootTravel || dest.map != 22 ||
						Native::portal(object, length, fields, true, dest) != Native::kGateAllowed ||
						dest.mapType != object[4] || dest.map != object[5] || dest.x != object[7] || dest.y != object[8]) {
						error = "Native portal authorization/foot-travel/destination mismatch"; return false;
					}
					++portals; cases += 3;
				}
			}
			offset += size;
		}
	}
	Native::ControlPair before(50, 49, 116), after(7, 8, 9);
	if (Native::toggleControl(before, after) != Native::kGateAllowed || after.wallState != 254 || after.controlState != 48 || after.controlPicture != 115 ||
		Native::toggleControl(after, after) != Native::kGateAllowed || after.wallState != before.wallState || after.controlState != before.controlState || after.controlPicture != before.controlPicture) {
		error = "Native paired control did not reverse exactly"; return false;
	}
	if (maps != 76 || keyed != 38 || compounds != 5 || plates != 14) {
		error = Common::String::format("Native gate inventory mismatch: maps=%u keyed=%u compound=%u plates=%u", maps, keyed, compounds, plates); return false;
	}
	report += Common::String::format("native-gates\tmaps=%u\tkeyed=%u\tcompound=%u\tplates=%u\tportals=%u\tcases=%u\townership=semantic-unit-only\n", maps, keyed, compounds, plates, portals, cases + 2);
	return true;
}
} // namespace Nox
#endif
