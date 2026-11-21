/* SPDX-License-Identifier: GPL-3.0-or-later */
// Exercise the real GL macros against a mock context, without GL headers/devices.
#include <cstdio>
#include <initializer_list>
#define GRAPHICS_OPENGL_CONTEXT_H
namespace OpenGL {
enum ContextType { kContextNone, kContextGL };
struct MockContext { ContextType type = kContextGL; } context;
unsigned cleared = 0, checked = 0, invoked = 0;
bool failure = false;
char order[64]; unsigned position = 0;
void clearGLError() { ++cleared; order[position++] = 'C'; }
bool checkGLError(const char *, const char *, int) { ++checked; order[position++] = 'E'; return failure; }
int operation() { ++invoked; order[position++] = 'G'; return 17; }
void reset() { cleared = checked = invoked = position = 0; }
}
#define OpenGLContext OpenGL::context
#include "graphics/opengl/debug.h"

int main() {
 using namespace OpenGL;
 unsigned failures = 0;
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #condition); ++failures; } } while (false)
#ifdef OPENGL_DEBUG
 const unsigned polling = 1;
#else
 const unsigned polling = 0;
#endif
 CHECK(polling == EXPECT_ROUTINE_POLLING);
 reset(); GL_CALL(operation());
 CHECK(invoked == 1 && cleared == polling && checked == polling);
 CHECK(order[polling] == 'G');
 reset(); int value = 0; GL_ASSIGN(value, operation());
 CHECK(value == 17 && invoked == 1 && cleared == polling && checked == polling);
 reset(); context.type = kContextNone; GL_CALL_SAFE(operation, ());
 CHECK(invoked == 0 && cleared == 0 && checked == 0);
 reset(); context.type = kContextGL; GL_CALL_SAFE(operation, ());
 CHECK(invoked == 1 && cleared == polling && checked == polling);
 for (bool injected : {false, true}) {
  reset(); failure = injected; bool error = !injected;
  GL_CALL_CHECK(error, operation());
  CHECK(invoked == 1 && cleared == 1 && checked == 1 && error == injected);
  CHECK(position == 3 && order[0] == 'C' && order[1] == 'G' && order[2] == 'E');
 }
 return failures ? 1 : 0;
}
