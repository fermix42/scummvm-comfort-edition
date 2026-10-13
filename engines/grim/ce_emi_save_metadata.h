/* SPDX-License-Identifier: GPL-3.0-or-later */

// CE-owned gallery reader for existing EMI saves. No save format changes.
#ifndef GRIM_CE_EMI_SAVE_METADATA_H
#define GRIM_CE_EMI_SAVE_METADATA_H

#include "common/ptr.h"
#include "common/savefile.h"
#include "common/system.h"
#include "engines/grim/savegame.h"
#include "engines/savestate.h"
#include "graphics/surface.h"

namespace Grim {
namespace CESaveMetadata {

inline SaveStateDescriptor saveMetadata(const MetaEngine *metaEngine, const Common::String &filename, int index, uint32 descriptionTag) {
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
		if (tag == descriptionTag && size >= 4) {
			const int32 length = file->readSint32LE();
			if (length > 0 && length <= 256 && (uint32)length <= size - 4) {
				char description[256];
				if (file->read(description, length) == (uint32)length) {
					description[length - 1] = '\0';
					desc.setDescription(description);
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

} // namespace CESaveMetadata
} // namespace Grim
#endif
