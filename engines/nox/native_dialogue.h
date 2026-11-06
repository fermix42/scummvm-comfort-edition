/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_DIALOGUE_H
#define NOX_NATIVE_DIALOGUE_H

#include "nox/native_lz.h"
#include "nox/resources.h"

namespace Nox {
namespace Native {

struct DialoguePacket {
	byte actor, record;
	uint32 offset;
	Common::Array<byte> encoded, text;
	DialoguePacket() : actor(0), record(0), offset(0) {}
};

inline bool decodeDialoguePacket(const byte *data, uint size, Common::Array<byte> &output) {
	if (!data || size < 3 || size >= 0x1400) return false;
	const uint decodedSize = READ_LE_UINT16(data);
	if (!decodedSize || decodedSize > 0x13fd) return false;
	return decodeReversePacket(data + 2, size - 2, decodedSize, output);
}

inline bool selectDialoguePacket(const Common::Array<byte> &bytes, byte actor,
		DialoguePacket &output, Common::String &error) {
	uint offset = 0;
	for (uint record = 1; record <= 32; ++record) {
		if (offset >= bytes.size() || bytes[offset] == 252) break;
		if (bytes.size() - offset < 3) { error = "Truncated dialogue record header"; return false; }
		const uint size = READ_LE_UINT16(bytes.data() + offset + 1);
		if (!size || size > bytes.size() - offset - 3) { error = "Truncated dialogue record payload"; return false; }
		if (bytes[offset] == actor) {
			DialoguePacket next;
			next.actor = actor; next.record = record; next.offset = offset + 3;
			next.encoded.resize(size); memcpy(next.encoded.data(), bytes.data() + next.offset, size);
			if (!decodeDialoguePacket(next.encoded.data(), next.encoded.size(), next.text)) {
				error = "Invalid native dialogue packet"; return false;
			}
			output = next; error.clear(); return true;
		}
		offset += 3 + size;
	}
	error = "Actor has no dialogue record"; return false;
}

inline bool loadDialoguePacket(HdvArchive &archive, const Common::String &name,
		byte actor, DialoguePacket &output, Common::String &error) {
	if (!name.hasPrefix("NA/DATA.TLK.")) { error = "Not a dialogue resource"; return false; }
	Common::Array<byte> data;
	if (!archive.read(name, data)) { error = archive.error(); return false; }
	return selectDialoguePacket(data, actor, output, error);
}

} // namespace Native
} // namespace Nox
#endif
