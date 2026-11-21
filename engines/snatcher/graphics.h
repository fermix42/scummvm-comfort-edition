/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_GRAPHICS_H
#define SNATCHER_GRAPHICS_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Snatcher {

// Independently specified resource codec. The caller removes the BE16 length
// header and supplies that exact length. This is data decoding, not VDP/CPU code.
// On failure output is empty, consumed is zero and error explains the rejection.
bool decodeGraphics(const uint8_t *src, size_t size, size_t expectedSize,
                    size_t maxOutput, std::vector<uint8_t> &out,
                    size_t &consumed, std::string &error, bool prefixOnly = false);

// Native resource geometry; raw words stay uninterpreted until tile/palette
// associations are independently established.
struct FramePiece {
    int x, y;
    unsigned width, height;
    uint16_t sizeWord, attributeWord;
};

// Offsets are relative to the complete DATA resource. Only normal sequence
// record indices (0..127) are accepted; no animation control code is executed.
bool readFrameGeometry(const uint8_t *data, size_t size, size_t tableOffset,
                       unsigned frameIndex, size_t maxPieces,
                       std::vector<FramePiece> &pieces, std::string &error);

} // namespace Snatcher
#endif
