/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_ENHANCEMENT_FIXTURE_H
#define NOX_ENHANCEMENT_FIXTURE_H
#include "nox/enhancements.h"
#include "common/memstream.h"
namespace Nox {
// Explicitly synthetic owner values; never claim these as earned conversations
// or explored campaign locations. The actual game's RAM/disk is not altered.
inline bool seedEnhancementFixture(Enhancements &owner, bool sessionA) {
	if (!owner.characters().empty() || !owner.cells().empty() || owner.sequence()) return false;
	const char packet[] = "$!CAVE:$!RIVER:$!SECRETS:";
	const Common::String suffix = sessionA ? "A" : "B";
	return owner.beginConversation("NA/DIALOG.FIX" + suffix, sessionA ? 7 : 8, sessionA ? 2 : 3, reinterpret_cast<const byte *>(packet), sizeof(packet) - 1) &&
	       owner.nameCharacter("Fixture " + suffix) &&
	       owner.recordReply(true, "", "", sessionA ? "Cave ahead." : "River ahead.") &&
	       owner.recordReply(false, sessionA ? "cave" : "river", sessionA ? "CAVE" : "RIVER", "Resolved fixture reply.") &&
	       owner.discover("NA/DATA.MAP.FIX" + suffix, 5, sessionA ? 1 : 2, sessionA ? 24 : 10, sessionA ? 38 : 11, true, false, "Observed fixture " + suffix) &&
	       owner.discover("NA/DATA.MAP.FIX" + suffix, 5, sessionA ? 1 : 2, sessionA ? 25 : 11, sessionA ? 38 : 11, false, true);
}
inline bool matchesEnhancementFixture(const Enhancements &owner, bool sessionA) {
	Enhancements expected;
	Common::MemoryWriteStreamDynamic actualBytes(DisposeAfterUse::YES), expectedBytes(DisposeAfterUse::YES);
	return seedEnhancementFixture(expected, sessionA) && owner.write(actualBytes) && expected.write(expectedBytes) &&
	       actualBytes.size() == expectedBytes.size() && !memcmp(actualBytes.getData(), expectedBytes.getData(), actualBytes.size());
}
} // namespace Nox
#endif
