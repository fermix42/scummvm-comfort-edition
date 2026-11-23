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
#include "common/file.h"
#include "common/endian.h"
#include "common/crc.h"
#include "common/md5.h"

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
bool ResourceManager::init(Common::String *failure) {
	for (int i = 0; i < kNumBins; i++) {
		Common::String name = Common::String::format("%d.BIN", i);
		Common::PEResources *pe = new Common::PEResources();
		if (!pe->loadFromEXE(Common::Path(name))) {
			warning("Gooka: cannot open resource container %s", name.c_str());
			if (failure) *failure = Common::String::format("Cannot open Gooka resource container %s", name.c_str());
			delete pe;
			return false;
		}
		_bins.push_back(pe);
	}
	if (Common::parseLanguage(ConfMan.get("language")) == Common::EN_ANY)
		return loadEnglish(failure);
	return true;
}

static bool invalidEnglish(Common::String *failure, const char *reason) {
	if (failure)
		*failure = Common::String::format("Cannot start Gooka in English: %s. Install a valid, compatible gooka.dat in the game folder or ScummVM extra-data search path, or select Czech in Game Options.", reason);
	return false;
}

// Same ordered formatting tokens checked by the localization generator/tests.
static Common::Array<Common::String> textControls(const Common::String &text) {
	Common::Array<Common::String> tokens;
	for (uint i = 0; i < text.size(); ++i) {
		const byte c = byte(text[i]);
		if (c && c < 32) {
			tokens.push_back(Common::String(text.c_str() + i, 1));
		} else if (c == '@') {
			uint end = i + 1;
			while (end < text.size() && text[end] >= '0' && text[end] <= '9') ++end;
			if (end > i + 1) { tokens.push_back(Common::String(text.c_str() + i, end - i)); i = end - 1; }
		} else if (c == '%') {
			uint end = i + 1;
			while (end < text.size() && strchr("-+0123456789.", text[end])) ++end;
			if (end < text.size() && ((text[end] >= 'a' && text[end] <= 'z') || (text[end] >= 'A' && text[end] <= 'Z'))) {
				tokens.push_back(Common::String(text.c_str() + i, end + 1 - i)); i = end;
			}
		}
	}
	return tokens;
}

bool ResourceManager::loadEnglish(Common::String *failure) {
	Common::File file;
	if (!file.open("gooka.dat")) return invalidEnglish(failure, "gooka.dat is missing");
	const uint32 limit = 2 * 1024 * 1024;
	if (file.size() < 24 || file.size() > limit + 24) return invalidEnglish(failure, "gooka.dat has an invalid size");
	Common::Array<byte> bytes;
	bytes.resize(uint(file.size()));
	if (file.read(bytes.data(), bytes.size()) != bytes.size() || file.err()) return invalidEnglish(failure, "gooka.dat could not be read completely");
	const byte *data = bytes.data();
	if (memcmp(data, "GOOKADAT", 8) || READ_BE_UINT16(data + 8) != 1 || READ_BE_UINT16(data + 10) != 1250)
		return invalidEnglish(failure, "gooka.dat has an unsupported format, version or encoding");
	const uint32 count = READ_BE_UINT32(data + 12), size = READ_BE_UINT32(data + 16);
	Common::CRC32 crc;
	if (!count || count > 4096 || size != bytes.size() - 24 || count > size / 22 || crc.crcFast(data + 24, size) != READ_BE_UINT32(data + 20))
		return invalidEnglish(failure, "gooka.dat is corrupt or truncated");
	Common::Array<byte> expected;
	expected.resize(65536);
	uint expectedCount = 0;
	for (uint n = 0; n < _bins.size(); ++n) {
		const Common::Array<Common::WinResourceID> ids = _bins[n]->getIDList(Common::WinResourceID(kResText));
		for (uint i = 0; i < ids.size(); ++i) {
			uint32 id = ids[i].getID();
			if (!id || id > 65535) return invalidEnglish(failure, "the game has unsupported text resource IDs");
			if (!expected[id]) { expected[id] = 1; ++expectedCount; }
		}
	}
	if (count != expectedCount) return invalidEnglish(failure, "gooka.dat does not contain every text resource for this game edition");
	Common::Array<EnglishText> translated;
	uint32 cursor = 24;
	uint16 previous = 0;
	for (uint n = 0; n < count; ++n) {
		if (bytes.size() - cursor < 22) return invalidEnglish(failure, "gooka.dat has a truncated text entry");
		EnglishText entry;
		entry.id = READ_BE_UINT16(data + cursor);
		uint32 length = READ_BE_UINT32(data + cursor + 2);
		const byte *fingerprint = data + cursor + 6;
		cursor += 22;
		if (entry.id <= previous || !expected[entry.id] || length > 65535 || length > bytes.size() - cursor)
			return invalidEnglish(failure, "gooka.dat has missing, duplicate or invalid text entries");
		previous = entry.id;
		for (uint i = 0; i < length; ++i) {
			byte c = data[cursor + i];
			if (!c || c == 0x81 || c == 0x83 || c == 0x88 || c == 0x90 || c == 0x98)
				return invalidEnglish(failure, "gooka.dat contains invalid CP1250 text");
		}
		entry.text = Common::String(reinterpret_cast<const char *>(data + cursor), length);
		cursor += length;
		Common::ScopedPtr<Common::SeekableReadStream> original(get(kResText, entry.id));
		byte digest[16];
		if (!original || original->size() > 131072 || !Common::computeStreamMD5(*original, digest) || memcmp(fingerprint, digest, 16))
			return invalidEnglish(failure, "gooka.dat does not match the original game text resources");
		original->seek(0);
		Common::String source = original->readString();
		if ((!source.empty() && entry.text.empty()) || textControls(source) != textControls(entry.text))
			return invalidEnglish(failure, "gooka.dat changes required text formatting or picture commands");
		translated.push_back(entry);
	}
	if (cursor != bytes.size()) return invalidEnglish(failure, "gooka.dat contains trailing or unowned data");
	_englishText = translated;
	_englishEnabled = true;
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
	if (_englishEnabled) {
		uint first = 0, end = _englishText.size();
		while (first < end) {
			const uint middle = first + (end - first) / 2;
			if (_englishText[middle].id < id) first = middle + 1;
			else end = middle;
		}
		if (first < _englishText.size() && _englishText[first].id == id)
			return _englishText[first].text;
		// Absent original resources have always returned empty. An existing ID
		// cannot be missing from the fully validated English table.
		if (has(kResText, id)) error("Gooka: gooka.dat has no English text for resource %u", id);
		return Common::String();
	}
	Common::ScopedPtr<Common::SeekableReadStream> original(get(kResText, id));
	return original ? original->readString() : Common::String();
}

} // End of namespace Gooka
