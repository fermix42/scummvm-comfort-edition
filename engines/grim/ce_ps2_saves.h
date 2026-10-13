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
#include "engines/grim/savegame.h"
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
	if (filename.empty())
		return SaveStateDescriptor();
	Common::ScopedPtr<Common::InSaveFile> file(g_system->getSavefileManager()->openForLoading(filename));
	if (!file || file->readUint32BE() != MKTAG('R', 'S', 'A', 'V'))
		return SaveStateDescriptor();
	const uint32 major = file->readUint32BE();
	const uint32 minor = file->readUint32BE();
	if (major != SaveGame::SAVEGAME_MAJOR_VERSION || minor > SaveGame::SAVEGAME_MINOR_VERSION)
		return SaveStateDescriptor();

	SaveStateDescriptor desc(metaEngine, index);
	// Read the existing sections without the runtime loader's fatal error
	// path: absent or malformed screenshots simply have no gallery image.
	while (!file->err() && !file->eos()) {
		const uint32 tag = file->readUint32BE();
		if (tag == MKTAG('E', 'S', 'A', 'V'))
			break;
		const uint32 size = file->readUint32BE();
		if (file->err() || size > file->size() - file->pos())
			break;
		const int64 end = file->pos() + size;
		if (tag == MKTAG('P', 'S', '2', 'S') && size >= 4) {
			const int32 length = file->readSint32LE();
			if (length > 0 && length <= 256 && (uint32)length <= size - 4) {
				char description[256];
				if (file->read(description, length) == (uint32)length) {
					description[length - 1] = '\0';
					desc.setDescription(Common::String::format(_("Slot %d - %s").encode().c_str(), playerSlot(filename) + 1, description));
				}
			}
		} else if (tag == MKTAG('S', 'I', 'M', 'G') && size == 256 * 128 * 2) {
			Graphics::Surface *image = new Graphics::Surface();
			image->create(256, 128, Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0));
			for (int y = 0; y < image->h; ++y) {
				uint16 *row = static_cast<uint16 *>(image->getBasePtr(0, y));
				for (int x = 0; x < image->w; ++x)
					row[x] = file->readUint16LE();
			}
			// The game stretched its 160x120 screenshot to a 256x128
			// texture. Restore the standard chooser's 4:3 thumbnail size.
			if (!file->err())
				desc.setThumbnail(image->scale(160, 120, true));
			image->free();
			delete image;
		}
		if (!file->seek(end))
			break;
	}
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
