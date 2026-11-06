/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_ENHANCEMENTS_H
#define NOX_ENHANCEMENTS_H
#include "common/array.h"
#include "common/str.h"
#include "common/stream.h"
#include "common/hash-str.h"
#include "common/hashmap.h"

namespace Nox {
struct JournalTopic {
	Common::String key, label;
	bool used = false;
};
struct JournalReply {
	uint64 sequence = 0;
	Common::String input, keyword, response;
	bool opening = false, recognized = false;
};
struct JournalCharacter {
	Common::String resource, name, defaultResponse;
	uint16 record = 0, actor = 0;
	Common::Array<JournalTopic> topics;
	Common::Array<JournalReply> replies;
};
struct DiscoveryCell {
	Common::String resource, annotation;
	byte mapType = 0, mapId = 0, x = 0, y = 0;
	bool observed = false, revealed = false;
};
// Per-world persistent enhancement owner. Adapted from the standalone
// ConversationJournal; private authored matching data is derived, never saved
// or exposed as a discovery. No CE account/session history belongs here.
class Enhancements {
public:
	bool beginConversation(const Common::String &resource, uint16 record, uint16 actor, const byte *packet, uint size);
	bool nameCharacter(const Common::String &name);
	bool recordReply(bool opening, const Common::String &input, const Common::String &selected, const Common::String &response);
	bool discover(const Common::String &resource, byte mapType, byte mapId, byte x, byte y, bool observed, bool revealed, const Common::String &annotation = Common::String());
	const Common::Array<JournalCharacter> &characters() const { return _characters; }
	const Common::Array<DiscoveryCell> &cells() const { return _cells; }
	uint64 sequence() const { return _sequence; }
	void reset();
	bool write(Common::WriteStream &stream) const;
	// Parse into a candidate; failure never changes this owner. One chunk is
	// consumed, allowing the runtime caller to enforce its own exact EOF.
	bool read(Common::SeekableReadStream &stream);
	enum { kMaxBytes = 16 * 1024 * 1024 };
private:
	uint64 _sequence = 0;
	Common::Array<JournalCharacter> _characters;
	Common::Array<DiscoveryCell> _cells;
	Common::HashMap<Common::String, uint> _cellIndex; // Derived lookup, rebuilt on load.
	int _active = -1;
	Common::Array<JournalTopic> _authored;
	bool readBody(Common::SeekableReadStream &stream);
};
} // namespace Nox
#endif
