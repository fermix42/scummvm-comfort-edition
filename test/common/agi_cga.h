/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <cxxtest/TestSuite.h>
#include "engines/agi/cga.h"

class AgiCGATestSuite : public CxxTest::TestSuite {
public:
	void test_original_PCXC_row_patterns() {
		// Independent fixture: original KQ1 CGA_GRAF PCXC AX returns in RGB
		// mode, also identical in KQ2, SQ1, SQ2 and LSL1's shipped tables.
		const uint16 originalAX[16] = {
			0x0000, 0x0000, 0x0504, 0x0001, 0x0A0A, 0x0C03, 0x0E0B, 0x0403,
			0x0304, 0x000D, 0x0401, 0x0505, 0x0E0E, 0x0802, 0x070D, 0x0F0F
		};
		for (int color = 0; color < 16; ++color) {
			for (int y = 0; y < 168; ++y) {
				const byte original = (y & 1) ? (originalAX[color] & 15) : (originalAX[color] >> 8);
				const byte actual = Agi::CGA::pictureColor(color, y);
				TS_ASSERT_EQUALS(actual & 3, original >> 2);
				TS_ASSERT_EQUALS(actual >> 2, original & 3);
			}
		}
	}

	void test_original_PCXO_opaque_colors_never_equal_transparency() {
		const byte originalView[16] = {0, 0, 12, 1, 10, 2, 9, 13, 0, 3, 5, 7, 14, 14, 15, 15};
		for (int color = 0; color < 16; ++color) {
			const byte actual = Agi::CGA::viewColor(color);
			TS_ASSERT_DIFFERS(actual, Agi::CGA::transparentColor());
			TS_ASSERT_EQUALS(actual & 3, originalView[color] >> 2);
			TS_ASSERT_EQUALS(actual >> 2, originalView[color] & 3);
		}
	}
};
