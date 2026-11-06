/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_MAPS_VALIDATION_H
#define NOX_NATIVE_MAPS_VALIDATION_H

#include "nox/native_maps.h"
#include "nox/resources.h"

namespace Nox {
inline Common::String nativeMapDigest(const Common::Array<byte> &bytes) {
	Common::MemoryReadStream stream(bytes.data(), bytes.size());
	return Common::computeStreamMD5AsString(stream);
}

// Runs before Runtime construction. Full decoded terrain/state hashes are
// compared by the local independent reader; no proprietary bytes are exported.
inline bool validateNativeMaps(HdvArchive &archive, Common::String &report,
		Common::String &error) {
	const char *families[] = { "SURF", "UM", "RUIN", "TOWN", "TOWN2", "CSTL", "KEEP" };
	uint maps = 0, sectors = 0;
	for (uint family = 0; family < ARRAYSIZE(families); ++family) {
		const Common::String name = Common::String("NA/DATA.MAP.") + families[family];
		Common::Array<byte> data;
		if (!archive.read(name, data)) { error = archive.error(); return false; }
		Native::MapResource resource;
		if (!resource.open(data, family == 0, family == 1)) { error = resource.error(); return false; }
		for (uint index = 0; index < resource.maps().size(); ++index) {
			const Native::MapDefinition &map = resource.maps()[index];
			const uint width = map.compressed ? 256 : 48;
			const uint count = map.compressed ? 256 : 9;
			Common::Array<byte> terrain; terrain.resize(width * width);
			for (uint sector = 0; sector < count; ++sector) {
				Common::Array<byte> cells;
				if (!resource.sector(map.id, sector, cells) || cells.size() != 256) {
					error = Common::String::format("Native map sector decode failed: %s/%u/%u", name.c_str(), map.id, sector); return false;
				}
				const uint columns = width / 16;
				for (uint y = 0; y < 16; ++y)
					for (uint x = 0; x < 16; ++x)
						terrain[((sector / columns) * 16 + y) * width + (sector % columns) * 16 + x] = cells[y * 16 + x];
				++sectors;
			}
			Common::Array<byte> tables;
			if (!resource.stateTables(map.id, tables) || tables.size() != 1792) {
				error = "Native map state tables missing"; return false;
			}
			for (uint y = 0; y < width; y += 15)
				for (uint x = 0; x < width; x += 15) {
					byte value = 0;
					if (!resource.tile(map.id, byte(x), byte(y), value) || value != terrain[y * width + x]) {
						error = "Native coordinate lookup disagrees with decoded sectors"; return false;
					}
				}
			report += Common::String::format("native-map\t%s\t%u\t%u\t%s\t%s\t%s\n", name.c_str(), map.id, map.compressed ? 1 : 0,
				nativeMapDigest(terrain).c_str(), nativeMapDigest(tables).c_str(), nativeMapDigest(map.metadata).c_str());
			++maps;
		}
	}
	if (maps != 76) { error = "Native map catalog does not cover all76 authored definitions"; return false; }
	report += Common::String::format("native-map-summary\tmaps=%u\tsectors=%u\towner=resource-and-format-only\n", maps, sectors);
	return true;
}
} // namespace Nox
#endif
