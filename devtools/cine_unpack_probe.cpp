#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static uint16_t be16(const uint8_t *p) {
	return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t be32(const uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

class CineUnpacker {
public:
	bool unpack(const uint8_t *src, unsigned int srcLen, uint8_t *dst, unsigned int dstLen);
private:
	uint32_t readSource();
	unsigned int rcr(bool inputCarry);
	unsigned int nextBit();
	unsigned int getBits(unsigned int numBits);
	void unpackRawBytes(unsigned int numBytes);
	void copyRelocatedBytes(unsigned int offset, unsigned int numBytes);

	uint32_t _crc = 0;
	uint32_t _chunk32b = 0;
	uint8_t *_dst = nullptr;
	const uint8_t *_src = nullptr;
	bool _error = false;
	const uint8_t *_srcBegin = nullptr;
	const uint8_t *_srcEnd = nullptr;
	uint8_t *_dstBegin = nullptr;
	uint8_t *_dstEnd = nullptr;
};

uint32_t CineUnpacker::readSource() {
	if (_src < _srcBegin || _src + 4 > _srcEnd) {
		_error = true;
		return 0;
	}
	uint32_t value = be32(_src);
	_src -= 4;
	return value;
}

unsigned int CineUnpacker::rcr(bool inputCarry) {
	unsigned int outputCarry = (_chunk32b & 1);
	_chunk32b >>= 1;
	if (inputCarry)
		_chunk32b |= 0x80000000;
	return outputCarry;
}

unsigned int CineUnpacker::nextBit() {
	unsigned int carry = rcr(false);
	if (_chunk32b == 0) {
		_chunk32b = readSource();
		_crc ^= _chunk32b;
		carry = rcr(true);
	}
	return carry;
}

unsigned int CineUnpacker::getBits(unsigned int numBits) {
	unsigned int c = 0;
	while (numBits--) {
		c <<= 1;
		c |= nextBit();
	}
	return c;
}

void CineUnpacker::unpackRawBytes(unsigned int numBytes) {
	if (_dst >= _dstEnd || _dst - numBytes + 1 < _dstBegin) {
		_error = true;
		return;
	}
	while (numBytes--) {
		*_dst = (uint8_t)getBits(8);
		--_dst;
	}
}

void CineUnpacker::copyRelocatedBytes(unsigned int offset, unsigned int numBytes) {
	if (_dst + offset >= _dstEnd || _dst - numBytes + 1 < _dstBegin) {
		_error = true;
		return;
	}
	while (numBytes--) {
		*_dst = *(_dst + offset);
		--_dst;
	}
}

bool CineUnpacker::unpack(const uint8_t *src, unsigned int srcLen, uint8_t *dst, unsigned int dstLen) {
	_error = false;
	_srcBegin = src;
	_srcEnd = src + srcLen;
	_dstBegin = dst;
	_dstEnd = dst + dstLen;

	_src = _srcBegin + srcLen - 4;
	uint32_t unpackedLength = readSource();
	_dst = _dstBegin + unpackedLength - 1;
	_crc = readSource();
	_chunk32b = readSource();
	_crc ^= _chunk32b;

	while (_dst >= _dstBegin && !_error) {
		if (!nextBit()) {
			if (!nextBit()) {
				unsigned int numBytes = getBits(3) + 1;
				unpackRawBytes(numBytes);
			} else {
				unsigned int numBytes = 2;
				unsigned int offset = getBits(8);
				copyRelocatedBytes(offset, numBytes);
			}
		} else {
			unsigned int c = getBits(2);
			if (c == 3) {
				unsigned int numBytes = getBits(8) + 9;
				unpackRawBytes(numBytes);
			} else if (c < 2) {
				unsigned int numBytes = c + 3;
				unsigned int offset = getBits(c + 9);
				copyRelocatedBytes(offset, numBytes);
			} else {
				unsigned int numBytes = getBits(8) + 1;
				unsigned int offset = getBits(12);
				copyRelocatedBytes(offset, numBytes);
			}
		}
	}
	printf("status error=%d crc=%08x first=%02x%02x%02x%02x\n", _error ? 1 : 0, _crc, dst[0], dst[1], dst[2], dst[3]);
	return !_error && (_crc == 0);
}

int main(int argc, char **argv) {
	if (argc != 3 && argc != 4)
		return 2;
	FILE *f = fopen(argv[1], "rb");
	if (!f)
		return 3;
	fseek(f, 0, SEEK_END);
	long fileSize = ftell(f);
	fseek(f, 0, SEEK_SET);
	std::vector<uint8_t> file(fileSize);
	fread(file.data(), 1, file.size(), f);
	fclose(f);

	uint16_t count = be16(file.data());
	uint16_t entrySize = be16(file.data() + 2);
	for (uint16_t i = 0; i < count; ++i) {
		const uint8_t *entry = file.data() + 4 + i * entrySize;
		char name[15] = {};
		memcpy(name, entry, 14);
		if (!strcmp(name, argv[2])) {
			uint32_t offset = be32(entry + 14);
			uint32_t packedSize = be32(entry + 18);
			uint32_t unpackedSize = be32(entry + 22);
			std::vector<uint8_t> out(unpackedSize);
			CineUnpacker unpacker;
			bool ok = unpacker.unpack(file.data() + offset, packedSize, out.data(), unpackedSize);
			printf("ok=%d count=%u entrySize=%u packed=%u unpacked=%u scriptCount=%u\n", ok ? 1 : 0, count, entrySize, packedSize, unpackedSize, be16(out.data()));
			if (ok && argc == 4) {
				FILE *outFile = fopen(argv[3], "wb");
				if (!outFile)
					return 5;
				fwrite(out.data(), 1, out.size(), outFile);
				fclose(outFile);
			}
			return ok ? 0 : 1;
		}
	}
	return 4;
}
