/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_RECORDS_H
#define NOX_RECORDS_H
#include "common/scummsys.h"
#include "common/endian.h"
#include "common/str.h"
#include "common/util.h"

namespace Nox {
// Lossless native records adapted from conversion/native/{party,equipment}.
// The original standalone library remains intact as a comparison oracle.
struct StatProgress {
	byte value, practice, threshold;
	bool advance() {
		practice = byte(practice + 1);
		if (practice < threshold)
			return false;
		practice = 0;
		if (value != 255)
			++value;
		return true;
	}
};
struct CharacterRecord {
	byte bytes[128] = {};
	uint16 health() const { return READ_LE_UINT16(bytes + 2); }
	void setHealth(uint16 hp) { WRITE_LE_UINT16(bytes + 2, hp); }
	void damage(uint16 amount) {
		if (health() <= amount) { setHealth(0); bytes[0] = 255; }
		else setHealth(health() - amount);
	}
};
struct EquipmentRecord {
	byte bytes[16] = {};
	bool ready(byte kind, byte id, byte flags) {
		uint slot = flags & 31;
		if (slot < 1 || slot > 7)
			return false;
		if (slot == 1) {
			if (flags & 128) { bytes[0] = kind; bytes[1] = id; bytes[2] = 3; return true; }
			if (bytes[0] == 3 || (bytes[0] == 0 && bytes[1] == 0)) slot = 0;
			else if (bytes[2] == 3 || (bytes[2] == 0 && bytes[3] == 0)) slot = 1;
			else return false;
		}
		bytes[slot * 2] = kind;
		bytes[slot * 2 + 1] = id;
		return true;
	}
};
struct ItemDefinition {
	byte bytes[32] = {};
	byte weight() const { return bytes[2]; }
	Common::String name() const {
		Common::String out;
		for (uint i = 13; i < 32 && bytes[i]; ++i)
			out += char(bytes[i] & 127);
		return out;
	}
};
inline byte equipmentWeight(byte current, byte added) { return MIN(uint(current) + added, 255U); }
inline byte weaponSkillBonus(byte kind, byte first, byte second, byte cap) {
	byte half = (kind >= 1 ? second : first) >> 1;
	return cap < half ? cap : MAX<byte>(half, 1);
}
} // namespace Nox
#endif
