/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_LZ_H
#define NOX_NATIVE_LZ_H

#include "common/array.h"

namespace Nox {
namespace Native {

class ReversePacketReader {
public:
	ReversePacketReader(const byte *data, uint size) : _data(data), _cursor(size), _buffer(0), _ok(data && size) {}
	byte readByte() {
		if (!_ok || !_cursor) { _ok = false; return 0; }
		return _data[--_cursor];
	}
	bool readBit() {
		bool carry = (_buffer & 128) != 0;
		_buffer = byte(_buffer << 1);
		if (!_buffer) {
			const byte value = readByte();
			carry = (value & 128) != 0; _buffer = byte((value << 1) | 1);
		}
		return carry;
	}
	bool ok() const { return _ok; }
	uint remaining() const { return _cursor; }
private:
	const byte *_data;
	uint _cursor;
	byte _buffer;
	bool _ok;
};

// The original backward LZ data format, with explicit bounds and staged output.
// This decodes data tokens; it never decodes/dispatches Apple II instructions.
inline bool decodeReversePacket(const byte *data, uint size, uint outputSize, Common::Array<byte> &output) {
	if (!data || !size || !outputSize || outputSize > 65536) return false;
	Common::Array<byte> decoded; decoded.resize(outputSize);
	ReversePacketReader stream(data, size);
	uint destination = outputSize;
	decoded[--destination] = stream.readByte();
	while (stream.ok()) {
		if (!stream.readBit()) {
			if (!destination) return false;
			decoded[--destination] = stream.readByte(); continue;
		}
		uint width = 1;
		while (!stream.readBit()) if (!stream.ok() || ++width > 17) return false;
		if (!stream.ok()) return false;
		if (width == 17) {
			if (destination || stream.remaining()) return false;
			output = decoded; return true;
		}
		uint length = 1;
		for (uint i = 1; i < width; ++i) length = (length << 1) | uint(stream.readBit());
		++length;
		const byte low = stream.readByte(); uint distance = (low & 127) + 1;
		if (low & 128) {
			uint prefix = 0;
			for (uint i = 0; i < 4; ++i) prefix = (prefix << 1) | uint(stream.readBit());
			distance += (prefix + 1) * 128;
		}
		if (!stream.ok() || length > destination || distance > outputSize - destination) return false;
		for (uint i = 0; i < length; ++i) { --destination; decoded[destination] = decoded[destination + distance]; }
	}
	return false;
}

} // namespace Native
} // namespace Nox
#endif
