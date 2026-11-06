/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_RESOURCES_H
#define NOX_RESOURCES_H

#include "common/array.h"
#include "common/endian.h"
#include "common/hash-str.h"
#include "common/hashmap.h"
#include "common/md5.h"
#include "common/memstream.h"
#include "common/stream.h"
#include "common/str.h"
#include "common/util.h"

namespace Nox {

// Adapted from conversion/native/hdv_archive.cpp. Immutable input, explicit
// errors and Common containers allow the same format rules in CE and detection.
// Inline implementation is shared by detection and engine plugins without a
// dependency from the detector onto the runtime plugin.
class HdvArchive {
public:
	struct Entry {
		byte storage, type;
		uint16 key, auxiliary;
		uint32 size;
	};
	typedef Common::HashMap<Common::String, Entry> Entries;

	bool open(Common::SeekableReadStream &stream) {
		clear();
		const int64 size = stream.size();
		if (size < 1536 || size % 512 || size / 512 > 65535)
			return fail("Not a supported raw ProDOS HDV image");
		_image.resize((uint32)size);
		if (!stream.seek(0) || stream.read(_image.data(), _image.size()) != _image.size())
			return fail("Unable to read HDV image");
		_blocks = _image.size() / 512;
		const byte *header = _image.data() + 1028;
		if ((header[0] >> 4) != 15 || !(header[0] & 15) || READ_LE_UINT16(header + 37) != _blocks)
			return fail("Invalid ProDOS volume header or size");
		if (!directory(2, "", 0)) {
			_entries.clear();
			return false;
		}
		return true;
	}

	const Common::String &error() const { return _error; }
	const Entries &entries() const { return _entries; }
	const Common::Array<byte> &image() const { return _image; }
	const Entry *entry(Common::String name) const {
		name.toUppercase();
		Entries::const_iterator it = _entries.find(name);
		return it == _entries.end() ? nullptr : &it->_value;
	}

	bool read(const Common::String &name, Common::Array<byte> &output) {
		const Entry *e = entry(name);
		if (!e)
			return fail("Missing HDV resource: " + name);
		return readRange(name, 0, e->size, output);
	}

	// Allocation-capacity reads preserve the original parser's explicit padding
	// API. Ordinary reads enforce logical EOF. Failure never modifies output.
	bool readRange(const Common::String &name, uint32 offset, uint32 count,
	               Common::Array<byte> &output, bool allocated = false) {
		const Entry *e = entry(name);
		if (!e || e->storage < 1 || e->storage > 3)
			return fail("Missing file resource: " + name);
		const uint32 capacity = e->storage == 1 ? 512 : e->storage == 2 ? 131072 : 33554432;
		const uint32 limit = allocated ? capacity : e->size;
		if (e->size > capacity || offset > limit || count > limit - offset)
			return fail("Resource range exceeds extent");
		Common::Array<byte> result;
		result.resize(count);
		if (count)
			memset(result.data(), 0, count);
		for (uint32 done = 0; done < count;) {
			const uint32 logical = offset + done, index = logical / 512;
			uint16 id = e->key;
			if (e->storage == 2) {
				if (!pointer(e->key, index, id))
					return false;
			} else if (e->storage == 3) {
				uint16 branch;
				if (!pointer(e->key, index / 256, branch) || !pointer(branch, index % 256, id))
					return false;
			}
			const uint32 amount = MIN(count - done, 512 - logical % 512);
			if (id) {
				const byte *source = block(id);
				if (!source)
					return false;
				memcpy(result.data() + done, source + logical % 512, amount);
			}
			done += amount;
		}
		output = result;
		return true;
	}

	// Compatibility identity is qualified by the same three immutable resource
	// signatures required by detection, never by mutable original save sectors.
	static Common::String baseV119Identity() {
		return "v119:2954f5fbb2576a3d5b4da631f3ae9841:a6ff35713915439a4b8b646d39e8a469:b4349c4c1a78554015d4abb4c9360c60";
	}
	bool isBaseV119() {
		static const struct { const char *name; uint32 size; const char *md5; } signatures[] = {
			{"NA/LOADER.P", 10514, "2954f5fbb2576a3d5b4da631f3ae9841"},
			{"NA/GAME", 13824, "a6ff35713915439a4b8b646d39e8a469"},
			{"NA/BS.ROUTINES.BK2", 12286, "b4349c4c1a78554015d4abb4c9360c60"}
		};
		for (uint i = 0; i < ARRAYSIZE(signatures); ++i) {
			Common::Array<byte> data;
			if (!read(signatures[i].name, data) || data.size() != signatures[i].size)
				return fail("Unsupported Nox runtime layout (base v119 required)");
			Common::MemoryReadStream stream(data.data(), data.size());
			if (Common::computeStreamMD5AsString(stream) != signatures[i].md5)
				return fail("Unsupported Nox runtime signature (base v119 required)");
		}
		return true;
	}

private:
	Common::Array<byte> _image;
	Common::Array<uint16> _directories;
	Entries _entries;
	uint32 _blocks = 0;
	Common::String _error;
	void clear() { _image.clear(); _directories.clear(); _entries.clear(); _blocks = 0; _error.clear(); }
	bool fail(const Common::String &message) { _error = message; return false; }
	const byte *block(uint16 id) {
		if (id >= _blocks) {
			fail("HDV block pointer outside volume");
			return nullptr;
		}
		return _image.data() + uint32(id) * 512;
	}
	bool pointer(uint16 id, uint32 index, uint16 &value) {
		if (index >= 256)
			return fail("Invalid ProDOS index offset");
		value = 0;
		if (!id)
			return true;
		const byte *b = block(id);
		if (!b)
			return false;
		value = b[index] | (uint16(b[index + 256]) << 8);
		return true;
	}
	bool directory(uint16 key, const Common::String &path, uint depth) {
		if (!key || depth > 32)
			return fail("Invalid or excessively nested HDV directory");
		for (uint i = 0; i < _directories.size(); ++i)
			if (_directories[i] == key)
				return fail("Cyclic HDV directory");
		_directories.push_back(key);
		const byte *first = block(key);
		if (!first)
			return false;
		const byte *header = first + 4;
		if ((header[0] >> 4) != (path.empty() ? 15 : 14) || header[31] != 39 || header[32] != 13)
			return fail("Unsupported ProDOS directory layout");
		const uint expected = READ_LE_UINT16(header + 33);
		uint count = 0;
		uint16 previous = 0;
		Common::Array<byte> seen;
		seen.resize(_blocks);
		memset(seen.data(), 0, seen.size());
		for (uint16 id = key; id;) {
			const byte *b = block(id);
			if (!b)
				return false;
			if (seen[id] || READ_LE_UINT16(b) != previous)
				return fail("Cyclic or broken HDV directory chain");
			seen[id] = 1;
			for (uint slot = id == key ? 1 : 0; slot < 13; ++slot) {
				const byte *e = b + 4 + slot * 39;
				const byte kind = e[0] >> 4, length = e[0] & 15;
				if (!kind)
					continue;
				if (!length)
					return fail("Empty HDV resource name");
				Common::String name((const char *)e + 1, length);
				for (uint i = 0; i < name.size(); ++i)
					if (!((name[i] >= 'A' && name[i] <= 'Z') || (name[i] >= '0' && name[i] <= '9') || name[i] == '.'))
						return fail("Invalid ProDOS resource name");
				const Common::String full = path.empty() ? name : path + "/" + name;
				if (_entries.contains(full))
					return fail("Duplicate HDV resource path");
				Entry info = {kind, e[16], READ_LE_UINT16(e + 17), READ_LE_UINT16(e + 31), uint32(e[21]) | uint32(e[22]) << 8 | uint32(e[23]) << 16};
				_entries[full] = info;
				if (kind == 13) {
					if (!directory(info.key, full, depth + 1))
						return false;
				} else if (kind > 3) {
					return fail("Unsupported ProDOS storage type");
				}
				++count;
			}
			previous = id;
			id = READ_LE_UINT16(b + 2);
		}
		return count == expected || fail("HDV directory entry count mismatch");
	}
};
} // namespace Nox
#endif
