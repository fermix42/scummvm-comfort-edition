#include <cxxtest/TestSuite.h>
#include "backends/platform/sdl/window-layout.h"

class WindowLayoutTestSuite : public CxxTest::TestSuite {
public:
	void testUltrawideWithBottomTaskbar() {
		WindowLayout::Rect work = {0, 0, 5120, 1392};
		WindowLayout::Rect r = WindowLayout::outerRect(work, WindowLayout::kLeftTwoThirds, 0, 0, 0, 0);
		TS_ASSERT_EQUALS(r.x, 0);
		TS_ASSERT_EQUALS(r.y, 0);
		TS_ASSERT_EQUALS(r.w, 3413);
		TS_ASSERT_EQUALS(r.h, 1392);
		TS_ASSERT_EQUALS(work.w - r.w, 1707);
	}

	void testTaskbarOnTopOrLeftAndNegativeMonitorOrigin() {
		WindowLayout::Rect work = {-5072, -1400, 5072, 1400};
		WindowLayout::Rect r = WindowLayout::outerRect(work, WindowLayout::kLeftTwoThirds, 0, 0, 0, 0);
		TS_ASSERT_EQUALS(r.x, -5072);
		TS_ASSERT_EQUALS(r.y, -1400);
		TS_ASSERT_EQUALS(r.w, 3381);
		TS_ASSERT_EQUALS(r.h, 1400);
	}

	void testDecorationsFitInsideRequestedOuterWindow() {
		WindowLayout::Rect outer = {0, 48, 3413, 1392};
		WindowLayout::Rect r = WindowLayout::clientRect(outer, 31, 8, 8, 8);
		TS_ASSERT_EQUALS(r.x, 8);
		TS_ASSERT_EQUALS(r.y, 79);
		TS_ASSERT_EQUALS(r.w, 3397);
		TS_ASSERT_EQUALS(r.h, 1353);
		TS_ASSERT_EQUALS(r.y + r.h + 8, 1440);
	}

	void testCustomPositionAndSize() {
		WindowLayout::Rect work = {-1920, 24, 1920, 1056};
		WindowLayout::Rect r = WindowLayout::outerRect(work, WindowLayout::kCustom, 1280, 720, 200, 100);
		TS_ASSERT_EQUALS(r.x, -1720);
		TS_ASSERT_EQUALS(r.y, 124);
		TS_ASSERT_EQUALS(r.w, 1280);
		TS_ASSERT_EQUALS(r.h, 720);
	}

	void testOversizeAndOffscreenSettingsAreClamped() {
		WindowLayout::Rect work = {0, 40, 1920, 1040};
		WindowLayout::Rect r = WindowLayout::outerRect(work, WindowLayout::kCustom, 99999, 99999, 99999, -500);
		TS_ASSERT_EQUALS(r.x, 0);
		TS_ASSERT_EQUALS(r.y, 40);
		TS_ASSERT_EQUALS(r.w, 1920);
		TS_ASSERT_EQUALS(r.h, 1040);
		r = WindowLayout::outerRect(work, WindowLayout::kCustom, 1000, 700, 99999, 99999);
		TS_ASSERT_EQUALS(r.x, 920);
		TS_ASSERT_EQUALS(r.y, 380);
	}

	void testZeroHeightFillsUsableAreaAndSmallSizesStayUsable() {
		WindowLayout::Rect work = {0, 0, 5120, 1392};
		WindowLayout::Rect r = WindowLayout::outerRect(work, WindowLayout::kCustom, 3000, 0, 0, 0);
		TS_ASSERT_EQUALS(r.w, 3000);
		TS_ASSERT_EQUALS(r.h, 1392);
		r = WindowLayout::outerRect(work, WindowLayout::kCustom, 1, 1, 0, 0);
		TS_ASSERT_EQUALS(r.w, 320);
		TS_ASSERT_EQUALS(r.h, 200);
	}

	void testLogicalDesktopCoordinatesAreNotScaledTwice() {
		// The same monitor at a scale where SDL exposes a 2560-wide desktop.
		WindowLayout::Rect work = {0, 0, 2560, 696};
		WindowLayout::Rect r = WindowLayout::outerRect(work, WindowLayout::kLeftTwoThirds, 0, 0, 0, 0);
		TS_ASSERT_EQUALS(r.w, 1706);
		TS_ASSERT_EQUALS(r.h, 696);
	}
};
