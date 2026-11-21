/* Independently reconstructed Snatcher data formats. */
#ifndef SNATCHER_PCM_H
#define SNATCHER_PCM_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Snatcher {

struct PcmCatalogEntry {
	uint16_t id;
	uint8_t mode, priority = 0, level, pan;
	uint16_t startSector, sectorCount;
	size_t offset, length;
};

struct PcmSample {
	size_t headerOffset, payloadOffset, sampleCount, encodedLength, markerLength;
	uint16_t nominalRate;
	uint32_t loopField;
};

// Deliberately no guessed playback defaults. A stored nominal header value is
// not necessarily the effective source rate used by original playback.
struct PcmPlaybackParameters {
	bool rateKnown, polarityKnown, gainKnown, loopKnown;
	uint32_t sourceRate;
	PcmPlaybackParameters() : rateKnown(false), polarityKnown(false),
		gainKnown(false), loopKnown(false), sourceRate(0) {}
};

bool parsePcmCatalog(const uint8_t *index, size_t size, size_t bankSize,
	std::vector<PcmCatalogEntry> &entries, std::string &error);
const PcmCatalogEntry *findPcmEntry(const std::vector<PcmCatalogEntry> &entries, uint16_t id);
bool parsePcmSamples(const uint8_t *data, size_t size, bool bankPadding,
	std::vector<PcmSample> &samples, size_t &tailOffset, std::string &error);
bool parsePcmStream(const uint8_t *data, size_t size, PcmSample &sample,
	size_t &paddingBytes, std::string &error);

// Explicit research/sample-domain conversion. No sample clock, gain, loop or
// output device is selected here. FF is a boundary marker and is rejected.
bool decodePcmCandidate(const uint8_t *data, size_t size,
	std::vector<int16_t> &samples, std::string &error);

} // namespace Snatcher
#endif
