/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "graphics.h"

namespace Snatcher {

namespace {
unsigned beWord(const uint8_t *p) {
    return (static_cast<unsigned>(p[0]) << 8) | p[1];
}
int signedWord(const uint8_t *p) {
    const unsigned value = beWord(p);
    return value < 32768 ? static_cast<int>(value) : static_cast<int>(value) - 65536;
}
}

bool readFrameGeometry(const uint8_t *data, size_t size, size_t tableOffset,
                       unsigned frameIndex, size_t maxPieces,
                       std::vector<FramePiece> &pieces, std::string &error) {
    pieces.clear();
    error.clear();
    if (!data || frameIndex >= 128 || tableOffset > size ||
        size - tableOffset < 2 || frameIndex > (size - tableOffset - 2) / 2) {
        error = "Frame table outside resource";
        return false;
    }
    const int relative = signedWord(data + tableOffset + frameIndex * 2);
    if ((relative < 0 && static_cast<size_t>(-relative) > tableOffset) ||
        (relative >= 0 && static_cast<size_t>(relative) > size - tableOffset)) {
        error = "Frame header outside resource";
        return false;
    }
    const size_t offset = relative < 0 ? tableOffset - static_cast<size_t>(-relative) :
                                       tableOffset + static_cast<size_t>(relative);
    if (size - offset < 2) {
        error = "Truncated frame header";
        return false;
    }
    const size_t count = beWord(data + offset) + 1;
    if (count > maxPieces || count > (size - offset - 2) / 8) {
        error = "Frame pieces exceed bounds";
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        const uint8_t *p = data + offset + 2 + i * 8;
        FramePiece piece;
        piece.y = signedWord(p);
        piece.sizeWord = static_cast<uint16_t>(beWord(p + 2));
        piece.width = ((piece.sizeWord >> 7) & 24) + 8;
        piece.height = ((piece.sizeWord >> 5) & 24) + 8;
        piece.attributeWord = static_cast<uint16_t>(beWord(p + 4));
        piece.x = signedWord(p + 6);
        pieces.push_back(piece);
    }
    return true;
}

bool decodeGraphics(const uint8_t *src, size_t size, size_t expectedSize,
                    size_t maxOutput, std::vector<uint8_t> &out,
                    size_t &consumed, std::string &error, bool prefixOnly) {
    out.clear();
    consumed = 0;
    error.clear();
    if (!src || !size) {
        error = "Missing graphics stream";
        return false;
    }
    if (expectedSize > maxOutput) {
        error = "Graphics length exceeds output bound";
        return false;
    }
    std::vector<uint8_t> decoded;
    size_t pos = 0;
    while (pos < size) {
        const unsigned flags = src[pos++];
        for (unsigned bit = 0; bit < 8; ++bit) {
            if (pos == size) {
                error = "Truncated graphics token";
                return false;
            }
            const unsigned command = src[pos++];
            if (!(flags & (1U << bit))) {
                if (decoded.size() == expectedSize) {
                    error = "Graphics literal exceeds declared length";
                    return false;
                }
                decoded.push_back(static_cast<uint8_t>(command));
                if(prefixOnly&&decoded.size()==expectedSize){out.swap(decoded);consumed=pos;return true;}
                continue;
            }
            if (command == 0x1f) {
                if (decoded.size() != expectedSize) {
                    error = "Graphics decoded length mismatch";
                    return false;
                }
                out.swap(decoded);
                consumed = pos;
                return true;
            }
            size_t count, distance;
            if (command >= 0xc0) {
                count = command - 0xb8;
                if (count > size - pos) {
                    error = "Truncated graphics literal run";
                    return false;
                }
                if(prefixOnly&&count>expectedSize-decoded.size())count=expectedSize-decoded.size();
                if (count > expectedSize - decoded.size()) {
                    error = "Graphics literal run exceeds declared length";
                    return false;
                }
                decoded.insert(decoded.end(), src + pos, src + pos + count);
                pos += count;
                if(prefixOnly&&decoded.size()==expectedSize){out.swap(decoded);consumed=pos;return true;}
                continue;
            }
            if (command < 0x80) {
                if (pos == size) {
                    error = "Truncated graphics long reference";
                    return false;
                }
                distance = ((command << 3) & 0xff00) | src[pos++];
                count = (command & 31) + 3;
            } else {
                distance = command & 15;
                count = (command >> 4) - 6;
            }
            if (!distance || distance > decoded.size()) {
                error = "Graphics reference precedes decoded history";
                return false;
            }
            if(prefixOnly&&count>expectedSize-decoded.size())count=expectedSize-decoded.size();
            if (count > expectedSize - decoded.size()) {
                error = "Graphics reference exceeds declared length";
                return false;
            }
            // Read growing history, rather than copying a fixed source range:
            // distance smaller than count intentionally repeats new bytes.
            for (size_t i = 0; i < count; ++i)
                decoded.push_back(decoded[decoded.size() - distance]);
            if(prefixOnly&&decoded.size()==expectedSize){out.swap(decoded);consumed=pos;return true;}
        }
    }
    error = "Graphics stream has no end marker";
    return false;
}

} // namespace Snatcher
