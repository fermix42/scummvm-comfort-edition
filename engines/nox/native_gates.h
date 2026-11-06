/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_GATES_H
#define NOX_NATIVE_GATES_H

#include "common/scummsys.h"

namespace Nox {
namespace Native {

// Original data layouts are inputs, not executable programs. These operations
// have no registers, program counter, emulated stack, bank mapping or I/O calls.
enum GateResult {
	kGateAllowed,
	kGateLocked,
	kGateNeedsFootTravel,
	kGateUnsupported,
	kGateInvalid
};

struct QuestFields {
	const byte *data;
	uint size;
	QuestFields(const byte *bytes, uint length) : data(bytes), size(length) {}
	bool contains(uint field) const { return data && field < size; }
};

struct PortalDestination {
	byte mapType, map, x, y;
};

inline GateResult keyDoor(const byte *object, uint length, QuestFields fields) {
	if (!object || length < 6)
		return kGateInvalid;
	if ((object[3] & 0xbf) != 18)
		return kGateUnsupported;
	if (!fields.contains(object[4]))
		return kGateInvalid;
	// A keyed door tests any bit in its authored mask, not equality or all bits.
	return fields.data[object[4]] & object[5] ? kGateAllowed : kGateLocked;
}

inline GateResult compoundDoor(const byte *object, uint length, QuestFields fields) {
	if (!object || length < 8)
		return kGateInvalid;
	if ((object[3] & 0xbf) != 19)
		return kGateUnsupported;
	const uint first = object[5], count = object[7];
	if (!fields.contains(object[4]) || first >= 8 || !count || count > 8 - first)
		return kGateInvalid;
	const byte required = byte(((1U << count) - 1) << first);
	return (fields.data[object[4]] & required) == required ? kGateAllowed : kGateLocked;
}

inline GateResult pressurePlate(const byte *object, uint length, QuestFields fields,
		byte &after) {
	if (!object || length < 8)
		return kGateInvalid;
	if ((object[3] & 0xbf) != 54)
		return kGateUnsupported;
	if (!fields.contains(object[4]))
		return kGateInvalid;
	after = fields.data[object[4]] | object[7];
	return kGateAllowed;
}

struct ControlPair {
	byte wallState, controlState, controlPicture;
	ControlPair(byte wall, byte state, byte picture)
		: wallState(wall), controlState(state), controlPicture(picture) {}
};

inline GateResult toggleControl(const ControlPair &before, ControlPair &after) {
	// Validate the complete pair before committing anything. A refused operation
	// leaves its output untouched, including when before and after alias.
	ControlPair next = before;
	if (before.controlState == 49 && before.wallState == 50) {
		next.controlState = 48;
		next.controlPicture = byte(before.controlPicture - 1);
		next.wallState = 254;
	} else if (before.controlState == 48 && before.wallState == 254) {
		next.controlState = 49;
		next.controlPicture = byte(before.controlPicture + 1);
		next.wallState = 50;
	} else {
		return kGateUnsupported;
	}
	after = next;
	return kGateAllowed;
}

inline GateResult portal(const byte *object, uint length, QuestFields fields,
		bool onFoot, PortalDestination &after) {
	if (!object || length < 12)
		return kGateInvalid;
	if ((object[3] & 0xbf) != 56)
		return kGateUnsupported;
	if (!fields.contains(object[9]))
		return kGateInvalid;
	// Match source precedence: absent authorization refuses before vehicle rules.
	if (!(fields.data[object[9]] & object[11]))
		return kGateLocked;
	// Map-zero portals need a surface-return owner; do not guess its coordinates.
	if (!object[5])
		return kGateUnsupported;
	if (!onFoot)
		return kGateNeedsFootTravel;
	PortalDestination next = { object[4], object[5], object[7], object[8] };
	after = next;
	return kGateAllowed;
}

} // namespace Native
} // namespace Nox
#endif
