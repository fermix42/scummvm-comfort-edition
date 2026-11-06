/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_ENHANCEMENT_VALIDATION_H
#define NOX_ENHANCEMENT_VALIDATION_H
#include "nox/enhancements.h"
#include "common/memstream.h"
#include "common/endian.h"

namespace Nox {
// Controlled owner fixtures, not earned conversation or campaign evidence.
static bool enhancementBytes(const Enhancements &state, Common::Array<byte> &bytes) {
	Common::MemoryWriteStreamDynamic out(DisposeAfterUse::YES);
	if (!state.write(out)) return false;
	bytes.resize(out.size()); memcpy(bytes.data(), out.getData(), out.size()); return true;
}
static bool enhancementEqual(const Enhancements &state, const Common::Array<byte> &expected) {
	Common::Array<byte> actual; return enhancementBytes(state, actual) && actual == expected;
}
static bool validateEnhancements(Common::String &error, uint &cases) {
	auto check = [&](bool condition, const char *message) {
		if (!condition) error = message; else ++cases;
		return condition;
	};
	Enhancements state;
	const char packet[] = "$!HELLO:$!CAVE:$!SECRETS:$!DFL:$!%HIDDEN:";
	if (!check(state.beginConversation("NA/DIALOG.TEST", 7, 2, reinterpret_cast<const byte *>(packet), sizeof(packet) - 1) && state.characters().size() == 1 && state.characters()[0].topics.empty(), "Resource read disclosed private dialogue topics")) return false;
	if (!check(state.nameCharacter("Fixture NPC") && state.recordReply(true, "", "", "Hello. Seek the cave.") && state.characters()[0].topics.size() == 2 && !state.characters()[0].topics[0].used, "Spoken discovery differs from existing journal semantics")) return false;
	if (!check(state.recordReply(false, "cave", "CAVE", "A resolved reply.") && state.characters()[0].replies.back().recognized && state.characters()[0].topics[1].used, "Resolved keyword was not recorded as used")) return false;
	if (!check(state.recordReply(false, "guessed", "DFL", "Unknown.") && state.characters()[0].topics.size() == 2 && state.characters()[0].replies.back().keyword.empty(), "Unseen private keyword leaked from a default response")) return false;
	if (!check(state.recordReply(false, "hello", "DFL", "Unknown.") && state.characters()[0].topics[0].used && !state.characters()[0].replies.back().recognized && state.characters()[0].replies.back().keyword == "HELL", "Previously discovered default-response keyword lost used state")) return false;
	if (!check(state.beginConversation("NA/DIALOG.TEST", 8, 3, reinterpret_cast<const byte *>(packet), sizeof(packet) - 1) && state.recordReply(true, "", "", "Second character.") && state.characters().size() == 2 && state.characters()[1].topics.empty(), "Separate character records merged journal discoveries")) return false;
	if (!check(state.discover("NA/DATA.MAP.TEST", 5, 1, 24, 38, true, false, "Observed fixture") && state.discover("NA/DATA.MAP.TEST", 5, 1, 25, 38, false, true) && state.cells().size() == 2 && !state.cells()[1].observed && state.cells()[1].revealed, "Reveal was incorrectly promoted to observed exploration")) return false;
	if (!check(state.discover("NA/DATA.MAP.TEST", 5, 1, 25, 38, true, false) && state.cells().size() == 2 && state.cells()[1].observed && state.cells()[1].revealed, "Observed/reveal merge lost distinct provenance")) return false;
	if (!check(state.discover("NA/DATA.MAP.OTHER", 5, 1, 24, 38, true, false) && state.cells().size() == 3, "Resource-qualified discovery identities collided")) return false;
	Common::Array<byte> encoded;
	if (!check(enhancementBytes(state, encoded) && READ_BE_UINT32(encoded.data()) == MKTAG('N','X','E','H') && READ_LE_UINT32(encoded.data() + 4) == 1 && READ_LE_UINT32(encoded.data() + 8) == encoded.size() - 12, "Enhancement chunk envelope invalid")) return false;
	bool privateLabelSaved = false;
	for (uint i = 0; i + 7 <= encoded.size(); ++i) if (!memcmp(encoded.data() + i, "SECRETS", 7)) privateLabelSaved = true;
	if (!check(!privateLabelSaved, "Private authored keyword cache was serialized")) return false;
	Enhancements loaded; Common::MemoryReadStream input(encoded.data(), encoded.size());
	if (!check(loaded.read(input) && input.pos() == input.size() && enhancementEqual(loaded, encoded), "All enhancement owners did not round-trip exactly")) return false;
	// A loaded journal rebuilds matching data only when its original packet is
	// encountered again. Durable discovered/used/history state must survive.
	if (!check(loaded.beginConversation("NA/DIALOG.TEST", 7, 2, reinterpret_cast<const byte *>(packet), sizeof(packet) - 1) && loaded.recordReply(false, "cave", "CAVE", "After reload.") && loaded.sequence() == state.sequence() + 1 && loaded.characters()[0].replies.size() == 5, "Derived matching cache failed to rebuild after load")) return false;
	if (!check(loaded.discover("NA/DATA.MAP.TEST", 5, 1, 24, 38, false, true) && loaded.cells().size() == 3 && loaded.cells()[0].observed && loaded.cells()[0].revealed, "Derived cell index failed to rebuild after load")) return false;
	// Every possible truncation must reject without modifying the previous owner.
	for (uint size = 0; size < encoded.size(); ++size) {
		Common::MemoryReadStream shortInput(encoded.data(), size);
		if (state.read(shortInput) || !enhancementEqual(state, encoded)) { error = "Truncated enhancement load mutated live owner"; return false; }
	}
	++cases;
	for (uint fault = 0; fault < 4; ++fault) {
		Common::Array<byte> bad = encoded;
		if (fault == 0) bad[0] ^= 1;
		if (fault == 1) WRITE_LE_UINT32(bad.data() + 4, 2);
		if (fault == 2) WRITE_LE_UINT32(bad.data() + 8, Enhancements::kMaxBytes + 1);
		if (fault == 3) WRITE_LE_UINT32(bad.data() + 20, 1025);
		Common::MemoryReadStream badInput(bad.data(), bad.size());
		if (!check(!state.read(badInput) && enhancementEqual(state, encoded), "Invalid enhancement envelope/count accepted or mutated owner")) return false;
	}
	Enhancements cellOnly;
	cellOnly.discover("NA/DATA.MAP.TEST", 5, 1, 24, 38, true, false);
	Common::Array<byte> cellBytes; enhancementBytes(cellOnly, cellBytes);
	const uint flagOffset = 28 + 4 + Common::String("NA/DATA.MAP.TEST").size() + 4;
	Common::Array<byte> badCell = cellBytes; badCell[flagOffset] = 2;
	Common::MemoryReadStream badFlag(badCell.data(), badCell.size());
	if (!check(!state.read(badFlag) && enhancementEqual(state, encoded), "Noncanonical enhancement boolean accepted")) return false;
	const uint originalSize = cellBytes.size(); cellBytes.resize(originalSize + originalSize - 28);
	memcpy(cellBytes.data() + originalSize, cellBytes.data() + 28, originalSize - 28);
	WRITE_LE_UINT32(cellBytes.data() + 24, 2); WRITE_LE_UINT32(cellBytes.data() + 8, cellBytes.size() - 12);
	Common::MemoryReadStream duplicateCell(cellBytes.data(), cellBytes.size());
	if (!check(!state.read(duplicateCell) && enhancementEqual(state, encoded), "Duplicate discovery identity accepted")) return false;
	if (!check(!state.beginConversation("../OTHER", 1, 1, reinterpret_cast<const byte *>(packet), sizeof(packet) - 1) && !state.discover("NA/DATA.MAP.TEST", 5, 1, 1, 1, false, false) && !state.recordReply(false, Common::String(65, 'X'), "", "Invalid") && enhancementEqual(state, encoded), "Invalid enhancement mutations changed prior state")) return false;
	Enhancements pristine; Common::Array<byte> empty; enhancementBytes(pristine, empty);
	loaded.reset();
	if (!check(enhancementEqual(loaded, empty) && enhancementEqual(state, encoded), "Session reset/cross-owner isolation failed")) return false;
	return true;
}
} // namespace Nox
#endif
