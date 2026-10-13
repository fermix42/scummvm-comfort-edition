/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// CE-owned MI4 PS2 save naming. Game scripts still use efmi000..efmi003.
#ifndef GRIM_CE_PS2_SAVES_H
#define GRIM_CE_PS2_SAVES_H

#include "common/algorithm.h"
#include "common/md5.h"
#include "common/memstream.h"
#include "common/savefile.h"
#include "common/system.h"
#include "common/util.h"
#include "common/ptr.h"
#include "common/translation.h"
#include "engines/ce_achievements.h"
#include "engines/grim/ce_emi_save_metadata.h"
#include "engines/savestate.h"
#include "graphics/surface.h"

namespace Grim {
namespace CEPS2Saves {

inline Common::String nameKey(const Common::String &value) {
	// A stable, portable filename token only. Sidecars retain the full IDs.
	Common::MemoryReadStream stream(reinterpret_cast<const byte *>(value.c_str()), value.size());
	return Common::computeStreamMD5AsString(stream);
}

inline Common::String prefix(const Common::String &target) {
	return "efmi-ce-" + nameKey(target) + "-";
}

inline Common::String sessionFile(const Common::String &target, const Common::String &session, int slot, int copy = 0) {
	const Common::String sessionKey = session.empty() ? "untracked" : nameKey(session);
	const Common::String copySuffix = copy ? Common::String::format("-copy%d", copy) : Common::String();
	return prefix(target) + sessionKey + copySuffix + Common::String::format("-slot%d.ps2", slot + 1);
}

inline int playerSlot(const Common::String &filename) {
	for (int slot = 0; slot < 4; ++slot) {
		if (filename == Common::String::format("efmi%03d.ps2", slot) ||
		    (filename.hasPrefix("efmi-ce-") && filename.hasSuffix(Common::String::format("-slot%d.ps2", slot + 1))))
			return slot;
	}
	return -1;
}

struct ChooserEntry {
	Common::String filename;
	Common::String session;
	int slot;
};

struct ChooserEntryComparator {
	bool operator()(const ChooserEntry &left, const ChooserEntry &right) const {
		if (left.session != right.session)
			return left.session < right.session;
		if (left.slot != right.slot)
			return left.slot < right.slot;
		return left.filename < right.filename;
	}
};

inline Common::StringArray files(const Common::String &target) {
	Common::SaveFileManager *manager = g_system->getSavefileManager();
	Common::StringArray result = manager->listSavefiles("efmi###.ps2");
	result.push_back(manager->listSavefiles(prefix(target) + "*-slot?.ps2"));
	// Sort by sidecar ownership, then the native slot. Legacy and session
	// filenames can belong to the same bank. Read each sidecar only once.
	Common::Array<ChooserEntry> entries;
	for (const auto &name : result) {
		ChooserEntry entry;
		entry.filename = name;
		entry.session = Common::CEAchievements().getSavePlaythroughForFile(name);
		entry.slot = playerSlot(name);
		entries.push_back(entry);
	}
	Common::sort(entries.begin(), entries.end(), ChooserEntryComparator());
	result.clear();
	for (const auto &entry : entries)
		result.push_back(entry.filename);
	return result;
}

// Chooser IDs are indices in the current file list, not the game's four slot
// numbers. This lets the upstream chooser list slot 1 from multiple sessions.
// The chooser is refreshed after changes; deletion already runs backwards.
inline Common::String chooserFile(const Common::String &target, int index) {
	const Common::StringArray names = files(target);
	return index >= 0 && (uint)index < names.size() ? names[index] : Common::String();
}

inline SaveStateDescriptor saveMetadata(const MetaEngine *metaEngine, const Common::String &filename, int index) {
	SaveStateDescriptor desc = CESaveMetadata::saveMetadata(metaEngine, filename, index, MKTAG('P', 'S', '2', 'S'));
	if (!desc.getDescription().empty())
		desc.setDescription(Common::String::format(_("Slot %d - %s").encode().c_str(), playerSlot(filename) + 1, desc.getDescription().c_str()));
	return desc;
}

inline Common::String playerFile(const Common::String &target, const Common::String &session, int slot) {
	Common::SaveFileManager *manager = g_system->getSavefileManager();
	const Common::String preferred = sessionFile(target, session, slot);
	if (manager->exists(preferred) && Common::CEAchievements().getSavePlaythroughForFile(preferred) == session)
		return preferred;

	// Continue using an existing owned file, including legacy efmi###.ps2.
	// New sessions cannot claim or overwrite another session's file.
	const Common::StringArray names = files(target);
	for (const auto &name : names) {
		if (playerSlot(name) == slot && Common::CEAchievements().getSavePlaythroughForFile(name) == session)
			return name;
	}
	// A missing or mismatched sidecar does not give us ownership of an
	// existing file, even if its name looks right. Preserve it and allocate
	// an unused name; subsequent saves find this session's owned copy above.
	Common::String available = preferred;
	for (int copy = 1; manager->exists(available); ++copy)
		available = sessionFile(target, session, slot, copy);
	return available;
}

inline Common::String resolveNative(const Common::String &target, const Common::String &filename) {
	// Only rewrite script-facing legacy names. Already resolved chooser files
	// and internal renderer checkpoints must retain their exact filenames.
	Common::String scriptName(filename);
	scriptName.replace('\\', '/');
	scriptName = Common::lastPathComponent(scriptName, '/');
	for (int slot = 0; slot < 4; ++slot) {
		if (scriptName == Common::String::format("efmi%03d.ps2", slot))
			return playerFile(target, Common::CEAchievements().getCurrentPlaythroughForTarget(target), slot);
	}
	return filename;
}

} // namespace CEPS2Saves
} // namespace Grim
#endif
