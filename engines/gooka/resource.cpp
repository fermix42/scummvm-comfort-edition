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
 * Resource lookup over the .BIN containers (see resource.h).
 */

#include "common/debug.h"
#include "common/config-manager.h"
#include "common/language.h"
#include "common/formats/winexe_pe.h"
#include "common/path.h"
#include "common/ptr.h"
#include "common/textconsole.h"

#include "gooka/detection.h"
#include "gooka/resource.h"
#include "gooka/localization/english.h"

namespace Gooka {

/** Number of resource containers shipped with the game (0.BIN .. 11.BIN). */
static const int kNumBins = 12;

ResourceManager::ResourceManager() {
}

// Closes all opened containers.
ResourceManager::~ResourceManager() {
	for (uint i = 0; i < _bins.size(); i++)
		delete _bins[i];
}

// Opens every container as a PE image; the resource directory is the index.
bool ResourceManager::init() {
	for (int i = 0; i < kNumBins; i++) {
		Common::String name = Common::String::format("%d.BIN", i);
		Common::PEResources *pe = new Common::PEResources();
		if (!pe->loadFromEXE(Common::Path(name))) {
			warning("Gooka: cannot open resource container %s", name.c_str());
			delete pe;
			return false;
		}
		_bins.push_back(pe);
	}
	return true;
}

// Searches the containers in order, as the original FindResource loop does.
Common::SeekableReadStream *ResourceManager::get(uint16 type, uint16 id) const {
	for (uint i = 0; i < _bins.size(); i++) {
		Common::SeekableReadStream *s = _bins[i]->getResource(Common::WinResourceID(type), Common::WinResourceID(id));
		if (s) {
			debugC(3, kDebugResource, "Resource %d:%d found in %d.BIN (%d bytes)", type, id, i, (int)s->size());
			return s;
		}
	}
	debugC(1, kDebugResource, "Resource %d:%d missing", type, id);
	return nullptr;
}

// Checks the resource directories only, no data is read.
bool ResourceManager::has(uint16 type, uint16 id) const {
	for (uint i = 0; i < _bins.size(); i++) {
		if (!_bins[i]->getLangList(Common::WinResourceID(type), Common::WinResourceID(id)).empty())
			return true;
	}
	return false;
}

// Reads a zero-terminated CP1250 string resource.
Common::String ResourceManager::getText(uint16 id) const {
	Common::ScopedPtr<Common::SeekableReadStream> s(get(kResText, id));
	if (!s)
		return Common::String();
	const Common::String original = s->readString();
	if (Common::parseLanguage(ConfMan.get("language")) != Common::EN_ANY)
		return original;
	uint first = 0;
	uint end = ARRAYSIZE(kEnglishText);
	while (first < end) {
		const uint middle = first + (end - first) / 2;
		if (kEnglishText[middle].id < id)
			first = middle + 1;
		else
			end = middle;
	}
	if (first < ARRAYSIZE(kEnglishText) && kEnglishText[first].id == id) {
		debugC(2, kDebugResource, "Gooka: English text %u", id);
		return kEnglishText[first].text;
	}
	if (!original.empty())
		warning("Gooka: missing English text %u; using original Czech", id);
	return original;
}

} // End of namespace Gooka
