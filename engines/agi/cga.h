/* ScummVM - Graphic Adventure Engine
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef AGI_CGA_H
#define AGI_CGA_H

#include "common/scummsys.h"

namespace Agi {
namespace CGA {

// Original AGI v2/v3 CGA_GRAF.OVL / AGIDATA.OVL tables, verified in
// KQ1, KQ2, SQ1, SQ2 and LSL1. Each nibble holds the left pixel in bits
// 3..2 and the right pixel in bits 1..0. PCXC returns odd in AL, even in AH.
static const byte rgbPicture[16][2] = {
	{0x0, 0x0}, {0x0, 0x0}, {0x4, 0x5}, {0x1, 0x0},
	{0xA, 0xA}, {0x3, 0xC}, {0xB, 0xE}, {0x3, 0x4},
	{0x4, 0x3}, {0xD, 0x0}, {0x1, 0x4}, {0x5, 0x5},
	{0xE, 0xE}, {0x2, 0x8}, {0xD, 0x7}, {0xF, 0xF}
};

// PCXO uses a separate, row-independent view table. Nibble 4 is reserved
// for transparency and is absent from all opaque entries (including black).
static const byte rgbView[16] = {
	0x0, 0x0, 0xC, 0x1, 0xA, 0x2, 0x9, 0xD,
	0x0, 0x3, 0x5, 0x7, 0xE, 0xE, 0xF, 0xF
};

static const byte rgbPalette[12] = {
	0x00, 0x00, 0xAA, // blue background (BIOS AH=0Bh, BH=0, BL=1)
	0x00, 0xAA, 0x00, // palette 0, low intensity
	0xAA, 0x00, 0x00,
	0xAA, 0x55, 0x00
};

// GfxMgr's packed screen stores the left pixel in the low two bits.
inline byte pack(byte original) {
	return ((original & 3) << 2) | (original >> 2);
}

inline byte pictureColor(byte color, int y) {
	return pack(rgbPicture[color & 15][(y & 1) ? 0 : 1]);
}

inline byte viewColor(byte color) {
	return pack(rgbView[color & 15]);
}

inline byte transparentColor() {
	return pack(4);
}

} // namespace CGA
} // namespace Agi

#endif
