/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <cxxtest/TestSuite.h>
#include "backends/graphics/frame-diagnostics.h"
#include "test/system/null_osystem.h"

#if NULL_OSYSTEM_IS_AVAILABLE
class GraphicsDiagnosticsTestSuite : public CxxTest::TestSuite {
public:
 void setUp() { Common::install_null_g_system(); GraphicsDiagnostics::state() = GraphicsDiagnostics::Frame(); }
 void tearDown() { GraphicsDiagnostics::state().active = false; Common::uninstall_null_g_system(); }
 void test_upload_and_error_check_stalls_keep_their_texture_and_order() {
  using namespace GraphicsDiagnostics;
  begin(false);
  unsigned order = 0;
  {
   TextureScope game(kGame);
   textureGLCall(kTexSubImage,
    [&]() { TS_ASSERT_EQUALS(order++, 1U); g_system->delayMillis(40); },
    [&]() { TS_ASSERT_EQUALS(order++, 0U); },
    [&]() { TS_ASSERT_EQUALS(order++, 2U); });
   {
    TextureScope cursor(kCursor);
    textureGLCall(kBindTexture,
     [&]() { TS_ASSERT_EQUALS(order++, 4U); },
     [&]() { TS_ASSERT_EQUALS(order++, 3U); },
     [&]() { TS_ASSERT_EQUALS(order++, 5U); g_system->delayMillis(40); });
   }
   TS_ASSERT_EQUALS(state().textureRole, kGame);
  }
  end();
  TS_ASSERT_EQUALS(order, 6U);
  const Frame &frame = state();
  TS_ASSERT(frame.textures[kGame].operations[kTexSubImage].maximum >= 25);
  TS_ASSERT(frame.textures[kCursor].operations[kErrorCheck].maximum >= 25);
  TS_ASSERT_EQUALS(frame.textures[kGame].operations[kErrorCheck].calls, 1U);
  TS_ASSERT_EQUALS(frame.textures[kCursor].operations[kTexSubImage].calls, 0U);
  TS_ASSERT_EQUALS(frame.textures[kGame].operations[kBindTexture].calls, 0U);
  TS_ASSERT_EQUALS(frame.textures[kOverlay].operations[kTextureUpdate].calls, 0U);
  TS_ASSERT_EQUALS(frame.textureRole, kUnattributed);
 }
 void test_interval_keeps_worst_upload_metadata_and_resets_each_frame() {
  using namespace GraphicsDiagnostics;
  begin(false);
  {
   TextureScope game(kGame);
   UploadInfo info; info.texture = 17; info.sourceWidth = 256; info.bottom = 16;
   info.bytes = 8192;
   noteUpload(info, g_system->getMillis(true) - 70);
  }
  end();
  TextureTiming interval; interval.merge(state().textures[kGame]);
  begin(false);
  TS_ASSERT_EQUALS(state().textures[kGame].uploads, 0U);
  {
   TextureScope game(kGame);
   UploadInfo info; info.texture = 23; info.bytes = 32;
   noteUpload(info, g_system->getMillis(true));
  }
  end();
  interval.merge(state().textures[kGame]);
  TS_ASSERT_EQUALS(interval.uploads, 2U);
  TS_ASSERT_EQUALS(interval.bytes, 8224U);
  TS_ASSERT_EQUALS(interval.last.texture, 23U);
  TS_ASSERT_EQUALS(interval.slowest.texture, 17U);
  TS_ASSERT_EQUALS(interval.slowest.sourceWidth, 256);
  unsigned calls = 0;
  textureGLCall(kTexSubImage, [&]() { ++calls; }, [&]() { ++calls; }, [&]() { ++calls; });
  TS_ASSERT_EQUALS(calls, 3U); // Disabled diagnostics still execute the normal calls.
  TS_ASSERT_EQUALS(state().textures[kUnattributed].operations[kTexSubImage].calls, 0U);
 }
};
#endif
