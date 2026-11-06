/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_DIALOGUE_VALIDATION_H
#define NOX_NATIVE_DIALOGUE_VALIDATION_H

#include "nox/native_dialogue.h"

namespace Nox {
inline bool validateNativeDialogue(HdvArchive &archive, Common::String &report, Common::String &error) {
	uint packets = 0;
	auto digest = [](const Common::Array<byte> &data) -> Common::String {
		Common::MemoryReadStream input(data.data(), data.size()); return Common::computeStreamMD5AsString(input);
	};
	for (HdvArchive::Entries::const_iterator it = archive.entries().begin(); it != archive.entries().end(); ++it) {
		if (!it->_key.hasPrefix("NA/DATA.TLK.")) continue;
		Common::Array<byte> data;
		if (!archive.read(it->_key, data)) { error = archive.error(); return false; }
		uint offset = 0; bool seen[256] = {};
		for (uint record = 1; record <= 32; ++record) {
			if (offset >= data.size() || data[offset] == 252 || data.size() - offset < 3) break;
			const byte actor = data[offset]; const uint size = READ_LE_UINT16(data.data() + offset + 1);
			// Original lookup stops at an invalid header. Retain the prior packets;
			// do not invent actor-aligned records from the known malformed tails.
			if (!size || size > data.size() - offset - 3) break;
			if (!seen[actor] && !(actor % 8)) {
				Native::DialoguePacket packet;
				if (!Native::selectDialoguePacket(data, actor, packet, error) || packet.record != record || packet.offset != offset + 3) return false;
				report += Common::String::format("native-dialogue\t%s\t%u\t%u\t%u\t%u\t%s\t%s\n", it->_key.c_str(), actor, record, packet.offset, packet.text.size(), digest(packet.encoded).c_str(), digest(packet.text).c_str());
				++packets;
			}
			seen[actor] = true; offset += 3 + size;
		}
	}
	if (packets != 439) { error = Common::String::format("Native dialogue corpus count mismatch: %u", packets); return false; }
	report += Common::String::format("native-dialogue-summary\tpackets=%u\towner=resource-codec-only\n", packets);
	return true;
}
} // namespace Nox
#endif
