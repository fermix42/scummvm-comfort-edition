#include "../pcm.h"
#include <cstdio>

namespace {
void put16(std::vector<uint8_t> &b, size_t p, uint16_t n) { b[p] = uint8_t(n >> 8); b[p+1] = uint8_t(n); }
void put32(std::vector<uint8_t> &b, size_t p, uint32_t n) {
	b[p] = uint8_t(n >> 24); b[p+1] = uint8_t(n >> 16); b[p+2] = uint8_t(n >> 8); b[p+3] = uint8_t(n);
}
std::vector<uint8_t> catalog(uint8_t mode = 2, uint16_t sectors = 1) {
	std::vector<uint8_t> data(0x4010, 0);
	data[9] = mode; put16(data, 14, sectors); data[0x4000] = 1;
	return data;
}
std::vector<uint8_t> headerSample(bool bank = false, size_t markerCount = 4) {
	std::vector<uint8_t> data(16 + 3 + markerCount, 0);
	put32(data, 0, uint32_t(3 + markerCount)); put32(data, 4, 0xffffffffU); put16(data, 8, 16000);
	data[16] = 0x80; data[17] = 0x81; data[18] = 1;
	for (size_t i = 19; i < data.size(); ++i) data[i] = 0xff;
	if (bank) { data.insert(data.end(), 4, 0xff); data.insert(data.end(), 12, 0x80); }
	return data;
}
}

int runPcmTests() {
	using namespace Snatcher;
	int failures = 0, checks = 0;
	#define PCM_CHECK(test) do { ++checks; if (!(test)) { ++failures; std::printf("PCM FAIL line %d: %s\n", __LINE__, #test); } } while (0)
	std::string error;
	std::vector<PcmCatalogEntry> entries;
	std::vector<uint8_t> data = catalog();
	PCM_CHECK(parsePcmCatalog(&data[0], data.size(), 2048, entries, error));
	PCM_CHECK(entries.size() == 1 && entries[0].id == 1 && entries[0].offset == 0 && entries[0].length == 2048);
	PCM_CHECK(findPcmEntry(entries, 1) != 0 && !findPcmEntry(entries, 0) && !findPcmEntry(entries, 2));
	PCM_CHECK(!parsePcmCatalog(&data[0], 8, 2048, entries, error) && entries.empty());
	data = catalog(3);
	PCM_CHECK(!parsePcmCatalog(&data[0], data.size(), 2048, entries, error));
	data = catalog(1, 0x35);
	PCM_CHECK(!parsePcmCatalog(&data[0], data.size(), 0x35 * 2048, entries, error));
	data = catalog(); put16(data, 12, 1);
	PCM_CHECK(!parsePcmCatalog(&data[0], data.size(), 2048, entries, error));
	data = catalog(); data[32] = 1;
	PCM_CHECK(!parsePcmCatalog(&data[0], data.size(), 2048, entries, error));
	data = catalog(); data[0x4000] = 2;
	PCM_CHECK(!parsePcmCatalog(&data[0], data.size(), 2048, entries, error));
	data = catalog();
	PCM_CHECK(!parsePcmCatalog(&data[0], data.size(), 2049, entries, error));
	std::vector<PcmSample> blocks;
	size_t tail = 99;
	data = headerSample();
	PCM_CHECK(parsePcmSamples(&data[0], data.size(), false, blocks, tail, error));
	PCM_CHECK(blocks.size() == 1 && blocks[0].sampleCount == 3 && blocks[0].nominalRate == 16000 && tail == data.size());
	data = headerSample(false, 5);
	PCM_CHECK(parsePcmSamples(&data[0], data.size(), false, blocks, tail, error) && blocks[0].markerLength == 5);
	data = headerSample(true);
	PCM_CHECK(parsePcmSamples(&data[0], data.size(), true, blocks, tail, error) && data.size() - tail == 16);
	data.back() = 0;
	PCM_CHECK(!parsePcmSamples(&data[0], data.size(), true, blocks, tail, error) && blocks.empty() && tail == 0);
	data = headerSample();
	PCM_CHECK(!parsePcmSamples(&data[0], data.size(), true, blocks, tail, error));
	data = headerSample(); data[16] = 0xff;
	PCM_CHECK(!parsePcmSamples(&data[0], data.size(), false, blocks, tail, error));
	data = headerSample(); put32(data, 0, 0xffffffffU);
	PCM_CHECK(!parsePcmSamples(&data[0], data.size(), false, blocks, tail, error));
	data = headerSample(); put32(data, 4, 0);
	PCM_CHECK(!parsePcmSamples(&data[0], data.size(), false, blocks, tail, error));
	data = headerSample(); data.pop_back();
	PCM_CHECK(!parsePcmSamples(&data[0], data.size(), false, blocks, tail, error));
	data = headerSample(); data.push_back(0);
	PCM_CHECK(!parsePcmSamples(&data[0], data.size(), false, blocks, tail, error));
	PcmSample stream;
	size_t padding = 0;
	data.assign(3, 0x80); data.insert(data.end(), 4, 0xff); data.insert(data.end(), 2048, 0x80);
	PCM_CHECK(parsePcmStream(&data[0], data.size(), stream, padding, error) && stream.sampleCount == 3 && padding == 2048 && stream.nominalRate == 0);
	data.push_back(0x80);
	PCM_CHECK(!parsePcmStream(&data[0], data.size(), stream, padding, error) && stream.sampleCount == 0 && padding == 0);
	data.assign(3, 0x80); data.insert(data.end(), 5, 0xff);
	PCM_CHECK(parsePcmStream(&data[0], data.size(), stream, padding, error) && stream.markerLength == 5);
	data.assign(3, 0x80);
	PCM_CHECK(!parsePcmStream(&data[0], data.size(), stream, padding, error));
	data.assign(3, 0x80); data.insert(data.end(), 4, 0xff); data.push_back(0);
	PCM_CHECK(!parsePcmStream(&data[0], data.size(), stream, padding, error));
	std::vector<int16_t> decoded;
	const uint8_t encoded[] = {0, 0x80, 1, 0x81, 0x7f, 0xfe};
	PCM_CHECK(decodePcmCandidate(encoded, sizeof(encoded), decoded, error));
	PCM_CHECK(decoded.size() == 6 && decoded[0] == 0 && decoded[1] == 0 && decoded[2] == -256 && decoded[3] == 256 && decoded[4] == -32512 && decoded[5] == 32256);
	const uint8_t marker = 255;
	PCM_CHECK(!decodePcmCandidate(&marker, 1, decoded, error) && decoded.empty());
	PCM_CHECK(decodePcmCandidate(0, 0, decoded, error) && decoded.empty());
	PCM_CHECK(!decodePcmCandidate(0, 1, decoded, error));
	PcmPlaybackParameters parameters;
	PCM_CHECK(!parameters.rateKnown && !parameters.polarityKnown && !parameters.gainKnown && !parameters.loopKnown && parameters.sourceRate == 0);
	std::printf("PCM checks: %d PASS, %d FAIL\n", checks - failures, failures);
	#undef PCM_CHECK
	return failures;
}

#ifdef SNATCHER_PCM_TEST_MAIN
#include <fstream>
#include <limits>

namespace {
bool readFile(const char *path, std::vector<uint8_t> &bytes) {
	std::ifstream in(path, std::ios::binary | std::ios::ate);
	if (!in) return false;
	const std::streamoff size = in.tellg();
	if (size < 0 || uint64_t(size) > 256U * 1024U * 1024U) return false;
	bytes.resize(size_t(size)); in.seekg(0);
	return bytes.empty() || bool(in.read(reinterpret_cast<char *>(&bytes[0]), std::streamsize(bytes.size())));
}
bool privateOutput(std::string path) {
	for (size_t i = 0; i < path.size(); ++i) {
		if (path[i] == '\\') path[i] = '/';
		if (path[i] >= 'A' && path[i] <= 'Z') path[i] += 'a' - 'A';
	}
	path = '/' + path + '/';
	return path.find("/private/") != std::string::npos && path.find("/../") == std::string::npos;
}
bool emitSample(const uint8_t *bytes, const Snatcher::PcmSample &sample, const char *kind,
	uint16_t entry, size_t number, std::ofstream &pcm, std::ofstream &manifest, size_t &pcmOffset, std::string &error) {
	std::vector<int16_t> decoded;
	if (!Snatcher::decodePcmCandidate(bytes + sample.payloadOffset, sample.sampleCount, decoded, error)) return false;
	manifest << kind << '\t' << entry << '\t' << number << '\t' << sample.headerOffset << '\t' << sample.payloadOffset << '\t'
		<< sample.sampleCount << '\t' << sample.encodedLength << '\t' << sample.markerLength << '\t' << sample.nominalRate << '\t'
		<< sample.loopField << '\t' << pcmOffset << '\t' << decoded.size() * 2 << '\n';
	for (size_t i = 0; i < decoded.size(); ++i) {
		const uint16_t value = uint16_t(decoded[i]);
		pcm.put(char(value & 255)); pcm.put(char(value >> 8));
	}
	pcmOffset += decoded.size() * 2;
	return bool(pcm) && bool(manifest);
}
int original(const char *indexPath, const char *bankPath, const char *drumPath, const char *output) {
	using namespace Snatcher;
	if (!privateOutput(output)) { std::printf("Output must be beneath private/ without parent traversal\n"); return 2; }
	std::vector<uint8_t> index, bank, drums;
	if (!readFile(indexPath, index) || !readFile(bankPath, bank) || !readFile(drumPath, drums)) return 2;
	std::vector<PcmCatalogEntry> entries; std::string error;
	if (!parsePcmCatalog(index.data(), index.size(), bank.size(), entries, error)) { std::printf("Catalog error: %s\n", error.c_str()); return 3; }
	const std::string base(output);
	std::ofstream pcm((base + "/pcm-native.s16le").c_str(), std::ios::binary);
	std::ofstream manifest((base + "/pcm-native.tsv").c_str());
	std::ofstream catalogFile((base + "/pcm-catalog.tsv").c_str());
	if (!pcm || !manifest || !catalogFile) return 2;
	manifest << "kind\tentry\tsample\theader_offset\tpayload_offset\tsample_count\tencoded_length\tmarker_length\tnominal_rate\tloop_field\tpcm_offset\tpcm_bytes\n";
	catalogFile << "id\tmode\tpriority\tlevel\tpan\tstart_sector\tsectors\toffset\tlength\n";
	size_t pcmOffset = 0, assets = 0;
	for (size_t i = 0; i < entries.size(); ++i) {
		const PcmCatalogEntry &e = entries[i];
		catalogFile << e.id << '\t' << unsigned(e.mode) << '\t' << unsigned(e.priority) << '\t' << unsigned(e.level) << '\t' << unsigned(e.pan)
			<< '\t' << e.startSector << '\t' << e.sectorCount << '\t' << e.offset << '\t' << e.length << '\n';
		const uint8_t *data = bank.data() + e.offset;
		if (e.mode == 1) {
			std::vector<PcmSample> samples; size_t tail;
			if (!parsePcmSamples(data, e.length, true, samples, tail, error)) { std::printf("Bank %u: %s\n", e.id, error.c_str()); return 3; }
			for (size_t j = 0; j < samples.size(); ++j) {
				if (!emitSample(data, samples[j], "resident", e.id, j, pcm, manifest, pcmOffset, error)) return 3;
				++assets;
			}
		} else {
			PcmSample sample; size_t padding;
			if (!parsePcmStream(data, e.length, sample, padding, error)) { std::printf("Stream %u: %s\n", e.id, error.c_str()); return 3; }
			if (!emitSample(data, sample, "stream", e.id, 0, pcm, manifest, pcmOffset, error)) return 3;
			++assets;
		}
	}
	std::vector<PcmSample> samples; size_t tail;
	if (!parsePcmSamples(drums.data(), drums.size(), false, samples, tail, error)) { std::printf("Drums: %s\n", error.c_str()); return 3; }
	for (size_t j = 0; j < samples.size(); ++j) {
		if (!emitSample(drums.data(), samples[j], "drum", 0, j, pcm, manifest, pcmOffset, error)) return 3;
		++assets;
	}
	pcm.close(); manifest.close(); catalogFile.close();
	if (!pcm || !manifest || !catalogFile) return 2;
	std::printf("PCM original: %u catalog entries, %u assets, %u decoded bytes; playback parameters remain unknown\n",
		unsigned(entries.size()), unsigned(assets), unsigned(pcmOffset));
	return 0;
}
}
int main(int argc, char **argv) {
	if (argc == 1) return runPcmTests() ? 1 : 0;
	if (argc == 6 && std::string(argv[1]) == "--original") return original(argv[2], argv[3], argv[4], argv[5]);
	std::printf("Usage: pcm-tests [--original INDEX BANK DRUM PRIVATE_OUT]\n"); return 2;
}
#endif
