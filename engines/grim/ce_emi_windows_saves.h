/* SPDX-License-Identifier: GPL-3.0-or-later */

// CE-owned save banks for MI4 Windows. The native scripts still see 160 slots.
#ifndef GRIM_CE_EMI_WINDOWS_SAVES_H
#define GRIM_CE_EMI_WINDOWS_SAVES_H

#include "common/algorithm.h"
#include "common/md5.h"
#include "common/memstream.h"
#include "common/savefile.h"
#include "common/system.h"
#include "common/util.h"
#include "engines/ce_achievements.h"

namespace Grim {
namespace CEWindowsSaves {

enum { kSlotCount = 160 };

inline Common::String nameKey(const Common::String &value) {
	Common::MemoryReadStream stream(reinterpret_cast<const byte *>(value.c_str()), value.size());
	return Common::computeStreamMD5AsString(stream);
}

inline Common::String prefix(const Common::String &target) {
	return "efmi-win-ce-" + nameKey(target) + "-";
}

inline Common::String basename(Common::String filename) {
	filename.replace('\\', '/');
	filename = Common::lastPathComponent(filename, '/');
	filename.toLowercase();
	return filename;
}

inline Common::String nativeFile(int slot) {
	return Common::String::format("efmi%03d.gsv", slot + 1);
}

inline int playerSlot(const Common::String &filename) {
	const Common::String name = basename(filename);
	int digits = -1;
	if (name.size() == 11 && name.hasPrefix("efmi") && name.hasSuffix(".gsv")) {
		digits = 4;
	} else if (name.hasPrefix("efmi-win-ce-") && name.hasSuffix(".gsv") &&
	           name.substr(name.size() - 12, 5) == "-slot") {
		digits = name.size() - 7;
	}
	if (digits < 0)
		return -1;
	int slot = 0;
	for (int i = 0; i < 3; ++i) {
		const char digit = name[digits + i];
		if (digit < '0' || digit > '9')
			return -1;
		slot = slot * 10 + digit - '0';
	}
	--slot;
	return slot >= 0 && slot < kSlotCount ? slot : -1;
}

inline Common::String sessionFile(const Common::String &target, const Common::String &session, int slot, int copy = 0) {
	const Common::String sessionKey = session.empty() ? "untracked" : nameKey(session);
	const Common::String copySuffix = copy ? Common::String::format("-copy%d", copy) : Common::String();
	return prefix(target) + sessionKey + copySuffix + Common::String::format("-slot%03d.gsv", slot + 1);
}

struct Entry {
	Common::String filename;
	Common::String session;
	int slot;
};

struct EntryComparator {
	bool operator()(const Entry &left, const Entry &right) const {
		if (left.session != right.session)
			return left.session < right.session;
		if (left.slot != right.slot)
			return left.slot < right.slot;
		return left.filename < right.filename;
	}
};

inline Common::StringArray files(const Common::String &target) {
	Common::SaveFileManager *manager = g_system->getSavefileManager();
	Common::StringArray names = manager->listSavefiles("efmi###.gsv");
	names.push_back(manager->listSavefiles(prefix(target) + "*-slot###.gsv"));
	Common::Array<Entry> entries;
	for (const auto &name : names) {
		Entry entry;
		entry.filename = name;
		entry.session = Common::CEAchievements().getSavePlaythroughForFile(name);
		entry.slot = playerSlot(name);
		// Preserve older saves outside the native menu's 160-slot range too.
		if (entry.slot < 0)
			entry.slot = atoi(name.c_str() + 4);
		entries.push_back(entry);
	}
	Common::sort(entries.begin(), entries.end(), EntryComparator());
	names.clear();
	for (const auto &entry : entries)
		names.push_back(entry.filename);
	return names;
}

inline Common::String playerFile(const Common::String &target, const Common::String &session, int slot) {
	Common::SaveFileManager *manager = g_system->getSavefileManager();
	const Common::String preferred = sessionFile(target, session, slot);
	if (manager->exists(preferred) && Common::CEAchievements().getSavePlaythroughForFile(preferred) == session)
		return preferred;
	for (const auto &name : files(target)) {
		if (playerSlot(name) == slot && Common::CEAchievements().getSavePlaythroughForFile(name) == session)
			return name;
	}
	// Never overwrite a file whose sidecar does not establish ownership.
	Common::String available = preferred;
	for (int copy = 1; manager->exists(available); ++copy)
		available = sessionFile(target, session, slot, copy);
	return available;
}

inline Common::String resolveNative(const Common::String &target, const Common::String &filename) {
	const Common::String name = basename(filename);
	const int slot = playerSlot(name);
	if (slot >= 0 && name == nativeFile(slot))
		return playerFile(target, Common::CEAchievements().getCurrentPlaythroughForTarget(target), slot);
	return filename;
}

inline Common::StringArray nativeFiles(const Common::String &target) {
	const Common::String session = Common::CEAchievements().getCurrentPlaythroughForTarget(target);
	Common::StringArray names;
	bool visible[kSlotCount] = {};
	for (const auto &name : files(target)) {
		const int slot = playerSlot(name);
		if (slot >= 0 && Common::CEAchievements().getSavePlaythroughForFile(name) == session)
			visible[slot] = true;
	}
	for (int slot = 0; slot < kSlotCount; ++slot) {
		if (visible[slot])
			names.push_back(nativeFile(slot));
	}
	return names;
}

} // namespace CEWindowsSaves
} // namespace Grim
#endif
