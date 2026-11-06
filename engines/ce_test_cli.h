/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef ENGINES_CE_TEST_CLI_H
#define ENGINES_CE_TEST_CLI_H

#include "common/formats/json.h"
#include "common/str-array.h"

namespace CETestCLI {
// Transport is installed only by an explicitly activated platform entry point.
// read returns bytes, 0 for no data, -1 for EOF. All engine work stays on its thread.
typedef int (*Read)(char *, unsigned);
typedef void (*Write)(const char *, unsigned);
void activate(Read read, Write write, const Common::String &root, bool offscreen);
bool active();
bool offscreen();
void setVideoDriver(const char *driver);
// Before scummvm_main: validates isolation, serves idle commands and waits for launch.
bool prepare(Common::StringArray &arguments);
void pump();
void frame();
int finish(int result);
void fatal(const char *message);
Common::JSONValue *unsupported();
Common::JSONValue *error(const char *code);
Common::JSONValue *ok();
Common::String stringField(Common::JSONValue &value, const char *key);
int intField(Common::JSONValue &value, const char *key, int fallback = -1);
}
#endif
