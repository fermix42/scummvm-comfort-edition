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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 * Access to the game data. Every N.BIN file (0..11) is a Win32 PE DLL
 * whose .rsrc section holds all resources as custom numeric types.
 */

#ifndef GOOKA_RESOURCE_H
#define GOOKA_RESOURCE_H

#include "common/array.h"
#include "common/str.h"
#include "common/stream.h"

namespace Common {
class PEResources;
}

namespace Gooka {

/** Resource types stored in the .BIN containers (numeric Win32 resource types). */
enum ResourceType {
	kResMusic = 32750, ///< RIFF 'MIDS' MIDI stream
	kResImage = 32751, ///< RLE picture or animation (RGB555 + unused 8-bit copy)
	kResWave  = 32752, ///< RIFF WAVE PCM: speech and sound effects
	kResFont  = 32753, ///< 256 glyphs of 8x16 pixels
	kResZone  = 32754, ///< 2 planes of 160x100 cells: walk map and hotspot map
	kResText  = 32755  ///< zero-terminated CP1250 string
};

/**
 * Opens the twelve .BIN containers and looks resources up by (type, id).
 * IDs are global: like the original, the containers are searched in order.
 */
class ResourceManager {
public:
	ResourceManager();
	~ResourceManager();

	/** Opens 0.BIN .. 11.BIN; returns false if any of them is missing or broken. */
	bool init();

	/** Returns a new stream with the resource data (caller deletes it), nullptr if missing. */
	Common::SeekableReadStream *get(uint16 type, uint16 id) const;

	/** Tells whether a resource exists without reading it. */
	bool has(uint16 type, uint16 id) const;

	/** Returns the raw CP1250 text of a text resource, empty if missing. */
	Common::String getText(uint16 id) const;

private:
	Common::Array<Common::PEResources *> _bins; ///< opened containers, index = BIN number
};

} // End of namespace Gooka

#endif // GOOKA_RESOURCE_H
