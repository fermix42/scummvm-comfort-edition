/* Independently reconstructed Snatcher catalog/sample parsing. */
#include "pcm.h"

namespace Snatcher {
namespace {
uint16_t be16(const uint8_t *p) { return uint16_t((uint16_t(p[0]) << 8) | p[1]); }
uint32_t be32(const uint8_t *p) {
	return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
bool all(const uint8_t *p, size_t size, uint8_t value) {
	for (size_t i = 0; i < size; ++i)
		if (p[i] != value) return false;
	return true;
}
bool fail(std::string &error, const char *message) { error = message; return false; }
}

bool parsePcmCatalog(const uint8_t *index, size_t size, size_t bankSize,
	std::vector<PcmCatalogEntry> &entries, std::string &error) {
	entries.clear(); error.clear();
	if (!index || size < 0x4010 || !bankSize || bankSize % 2048)
		return fail(error, "Invalid or truncated PCM catalog/bank extent");
	const size_t eventBytes = size - 0x4000;
	if (eventBytes % 16 || eventBytes / 16 > 255 || index[0x4000] != eventBytes / 16)
		return fail(error, "PCM effect directory extent/count mismatch");
	std::vector<PcmCatalogEntry> parsed;
	size_t cursor = 8;
	uint32_t nextSector = 0;
	while (cursor + 8 <= 0x4000) {
		const uint8_t *p = index + cursor;
		if (all(p, 8, 0)) break;
		PcmCatalogEntry entry;
		entry.id = uint16_t(cursor / 8);
		entry.priority = p[0]; entry.mode = p[1]; entry.level = p[2]; entry.pan = p[3];
		entry.startSector = be16(p + 4); entry.sectorCount = be16(p + 6);
		if ((entry.mode != 1 && entry.mode != 2 && entry.mode != 0x82) || !entry.sectorCount)
			return fail(error, "Unsupported PCM catalog mode or empty extent");
		if (entry.mode == 1 && entry.sectorCount >= 0x35)
			return fail(error, "Resident PCM bank exceeds established original limit");
		const uint32_t endSector = uint32_t(entry.startSector) + entry.sectorCount;
		if (entry.startSector != nextSector || uint64_t(endSector) * 2048 > bankSize)
			return fail(error, "PCM catalog extent is noncontiguous or outside bank");
		entry.offset = size_t(entry.startSector) * 2048;
		entry.length = size_t(entry.sectorCount) * 2048;
		parsed.push_back(entry);
		nextSector = endSector;
		cursor += 8;
	}
	if (parsed.empty() || uint64_t(nextSector) * 2048 != bankSize)
		return fail(error, "PCM catalog does not cover source bank");
	if (!all(index + cursor, 0x4000 - cursor, 0))
		return fail(error, "Unclassified nonzero PCM catalog tail");
	entries.swap(parsed);
	return true;
}

const PcmCatalogEntry *findPcmEntry(const std::vector<PcmCatalogEntry> &entries, uint16_t id) {
	if (!id || size_t(id) > entries.size()) return 0;
	const PcmCatalogEntry &entry = entries[id - 1];
	return entry.id == id ? &entry : 0;
}

bool parsePcmSamples(const uint8_t *data, size_t size, bool bankPadding,
	std::vector<PcmSample> &samples, size_t &tailOffset, std::string &error) {
	samples.clear(); tailOffset = 0; error.clear();
	if (!data || !size) return fail(error, "Missing PCM sample bytes");
	std::vector<PcmSample> parsed;
	size_t cursor = 0;
	while (size - cursor >= 16) {
		const uint8_t *header = data + cursor;
		const uint32_t count = be32(header), loop = be32(header + 4);
		const uint16_t nominal = be16(header + 8);
		if (loop != 0xffffffffU || nominal < 1000 || count < 4) {
			if (bankPadding && !parsed.empty() && size - cursor < 2048) break;
			return fail(error, "Unsupported PCM sample header");
		}
		if (size_t(count) > size - cursor - 16)
			return fail(error, "PCM sample payload is truncated");
		const size_t payload = cursor + 16;
		size_t endOfSamples = 0;
		while (endOfSamples < count && data[payload + endOfSamples] != 0xff) ++endOfSamples;
		const size_t markers = size_t(count) - endOfSamples;
		if ((markers != 4 && markers != 5) || !all(data + payload + endOfSamples, markers, 0xff))
			return fail(error, "PCM sample marker is invalid or internal");
		PcmSample sample;
		sample.headerOffset = cursor; sample.payloadOffset = payload;
		sample.sampleCount = endOfSamples; sample.encodedLength = count; sample.markerLength = markers;
		sample.nominalRate = nominal; sample.loopField = loop;
		parsed.push_back(sample);
		cursor = payload + count;
	}
	if (parsed.empty()) return fail(error, "No supported PCM sample blocks");
	if (bankPadding) {
		const size_t tail = size - cursor;
		if (tail < 4 || tail >= 2048 || !all(data + cursor, 4, 0xff) || !all(data + cursor + 4, tail - 4, 0x80))
			return fail(error, "Resident PCM bank footer/padding is invalid");
	} else if (cursor != size) {
		return fail(error, "Unclassified bytes after PCM sample blocks");
	}
	tailOffset = cursor;
	samples.swap(parsed);
	return true;
}

bool parsePcmStream(const uint8_t *data, size_t size, PcmSample &sample,
	size_t &paddingBytes, std::string &error) {
	sample = PcmSample(); paddingBytes = 0; error.clear();
	if (!data || !size) return fail(error, "Missing PCM stream extent");
	size_t end = 0;
	while (end < size && data[end] != 0xff) ++end;
	if (end == size) return fail(error, "PCM stream has no terminal marker");
	size_t cursor = end;
	while (cursor < size && data[cursor] == 0xff) ++cursor;
	const size_t markerLength = cursor - end;
	if ((markerLength != 4 && markerLength != 5) || size - cursor > 2048 || !all(data + cursor, size - cursor, 0x80))
		return fail(error, "Unsupported PCM stream marker/allocation padding");
	sample.payloadOffset = 0; sample.sampleCount = end; sample.encodedLength = cursor;
	sample.markerLength = markerLength;
	paddingBytes = size - cursor;
	return true;
}

bool decodePcmCandidate(const uint8_t *data, size_t size,
	std::vector<int16_t> &samples, std::string &error) {
	samples.clear(); error.clear();
	if (!data && size) return fail(error, "Missing PCM sample data");
	std::vector<int16_t> decoded;
	decoded.reserve(size);
	for (size_t i = 0; i < size; ++i) {
		const uint8_t code = data[i];
		if (code == 0xff) return fail(error, "PCM end marker supplied as sample");
		const int magnitude = int(code & 0x7f) * 256;
		decoded.push_back(int16_t((code & 0x80) ? magnitude : -magnitude));
	}
	samples.swap(decoded);
	return true;
}

} // namespace Snatcher
