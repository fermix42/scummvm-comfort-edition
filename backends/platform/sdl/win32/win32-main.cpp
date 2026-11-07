/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// Disable symbol overrides so that we can use system headers.
#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "common/scummsys.h"

#ifdef WIN32

// Fix for bug #4700 "MSVC compilation broken with r47595":
// We need to keep this on top of the "common/scummsys.h"(base/main.h) include,
// otherwise we will get errors about the windows headers redefining
// "ARRAYSIZE" for example.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>

#include "backends/platform/sdl/win32/win32.h"
#include "backends/platform/sdl/win32/win32_wrapper.h"
#include "backends/plugins/win32/win32-provider.h"
#include "base/main.h"
#include "engines/ce_test_cli.h"

static HANDLE cliInput = INVALID_HANDLE_VALUE, cliOutput = INVALID_HANDLE_VALUE;
static int readCLI(char *buffer, unsigned size) {
	DWORD available = 0, read = 0;
	if (!PeekNamedPipe(cliInput, nullptr, 0, nullptr, &available, nullptr)) return -1;
	if (!available) return 0;
	if (!ReadFile(cliInput, buffer, available < size ? available : size, &read, nullptr)) return -1;
	return (int)read;
}
static void writeCLI(const char *buffer, unsigned size) {
	DWORD written = 0;
	while (size && WriteFile(cliOutput, buffer, size, &written, nullptr) && written) {
		buffer += written; size -= written;
	}
}
static int cliUsageError(const char *code) {
	Common::JSONValue *error = CETestCLI::error(code);
	Common::String line = error->stringify() + "\n"; delete error;
	DWORD written;
	WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), line.c_str(), line.size(), &written, nullptr);
	return 64;
}

int __stdcall WinMain(HINSTANCE /*hInst*/, HINSTANCE /*hPrevInst*/,  LPSTR /*lpCmdLine*/, int /*iShowCmd*/) {
#if !SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_SetModuleHandle(GetModuleHandle(NULL));
#endif
// HACK: __argc, __argv are broken and return zero when using mingwrt 4.0+ on MinGW
// HACK: MinGW-w64 based toolchains neither feature _argc nor _argv. The 32 bit
// incarnation only defines __MINGW32__. This leads to build breakage due to
// missing declarations. Luckily MinGW-w64 based toolchains define
// __MINGW64_VERSION_foo macros inside _mingw.h, which is included from all
// system headers. Thus we abuse that to detect them.
#if defined(__GNUC__) && defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR)
	return main(_argc, _argv);
#else
	return main(__argc, __argv);
#endif
}

// Resolve explicitly supplied CLI filenames before anchoring legacy diagnostics.
// Relative INI storage values remain executable-relative; explicit CLI paths keep
// their established caller-working-directory semantics.
static Common::String absoluteCLIPath(const Common::String &path) {
	if (path.empty()) return path;
	wchar_t *wide = Win32::ansiToUnicode(path.c_str());
	Common::Array<wchar_t> buffer;
	buffer.resize(GetFullPathNameW(wide, 0, nullptr, nullptr));
	Common::String result(path);
	if (buffer.size() && GetFullPathNameW(wide, buffer.size(), buffer.data(), nullptr)) {
		char *utf8 = Win32::unicodeToAnsi(buffer.data()); result = utf8; free(utf8);
	}
	free(wide);
	return result;
}

static void normalizeCLIPaths(Common::StringArray &args) {
	const char *options[] = {"--config", "--savepath", "--logfile", "--screenshotpath", "--path",
	                        "--extrapath", "--themepath", "--iconspath", "--dlcspath", "--soundfont",
	                        "--record-file", "--playback-file", "-c", "-p"};
	for (uint i = 1; i < args.size(); ++i) {
		for (const char *option : options) {
			Common::String name(option);
			if (args[i] == name && i + 1 < args.size()) {
				args[i + 1] = absoluteCLIPath(args[i + 1]); ++i; break;
			}
			if (args[i].hasPrefix(name + "=")) {
				args[i] = name + "=" + absoluteCLIPath(args[i].substr(name.size() + 1)); break;
			}
			if (name.size() == 2 && args[i].hasPrefix(name) && args[i].size() > 2) {
				args[i] = name + absoluteCLIPath(args[i].substr(2)); break;
			}
		}
	}
}

int main(int argc, char *argv[]) {
#ifdef UNICODE
	char **utf8Argv = Win32::getArgvUtf8(&argc);
	argv = utf8Argv;
#endif
	bool testCLI = false, offscreen = false;
	Common::String root;
	for (int i = 1; i < argc; ++i) if (!strcmp(argv[i], "--test-cli")) testCLI = true;
	Common::StringArray cliArgs;
	if (testCLI) {
#if !SDL_VERSION_ATLEAST(2, 0, 0) || SDL_VERSION_ATLEAST(3, 0, 0)
		return cliUsageError("test_cli_requires_sdl2");
#else
		cliArgs.push_back(argv[0]);
		for (int i = 1; i < argc; ++i) {
			Common::String arg(argv[i]);
			if (arg == "--test-cli") continue;
			if (arg.hasPrefix("--test-root=")) root = arg.substr(12);
			else if (arg == "--test-render=offscreen") offscreen = true;
			else if (arg.hasPrefix("--extrapath=")) cliArgs.push_back(arg);
			else return cliUsageError("unknown_test_option");
		}
		if (root.size() < 4 || root[1] != ':' || (root[2] != '\\' && root[2] != '/')) return cliUsageError("absolute_test_root_required");
		cliInput = GetStdHandle(STD_INPUT_HANDLE);
		if (GetFileType(cliInput) != FILE_TYPE_PIPE || GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) != FILE_TYPE_PIPE) return cliUsageError("stdin_stdout_pipes_required");
		if (!DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_OUTPUT_HANDLE), GetCurrentProcess(), &cliOutput, 0, FALSE, DUPLICATE_SAME_ACCESS)) return 64;
		if (_dup2(_fileno(stderr), _fileno(stdout)) != 0) return 64;
		CETestCLI::activate(readCLI, writeCLI, root, offscreen);
		SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
		SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
		if (!offscreen) {
			SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
			SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
		}
#endif
	}

	if (testCLI) normalizeCLIPaths(cliArgs);

	Common::StringArray normalArguments;
	Common::Array<char *> normalPointers;
	if (!testCLI) {
		for (int i = 0; i < argc; ++i) normalArguments.push_back(argv[i]);
		normalizeCLIPaths(normalArguments);
		for (const auto &arg : normalArguments) normalPointers.push_back(const_cast<char *>(arg.c_str()));
		normalPointers.push_back(nullptr);
		argv = normalPointers.data();
		Win32::initializePortableStorage();
	}

	// Create our OSystem instance
	g_system = new OSystem_Win32();
	assert(g_system);

	// Pre initialize the backend
	g_system->init();
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (testCLI) CETestCLI::setVideoDriver(SDL_GetCurrentVideoDriver());
#endif

#ifdef DYNAMIC_MODULES
	PluginManager::instance().addPluginProvider(new Win32PluginProvider());
#endif

	// Invoke the actual ScummVM main entry point:
	int res;
	if (testCLI) {
		if (CETestCLI::prepare(cliArgs)) {
			Win32::initializePortableStorage(root);
			Common::Array<const char *> args;
			for (const auto &arg : cliArgs) args.push_back(arg.c_str());
			res = scummvm_main(args.size(), args.data());
		} else res = 64;
		res = CETestCLI::finish(res);
	} else res = scummvm_main(argc, argv);

	// Free OSystem
	g_system->destroy();

#ifdef UNICODE
	Win32::freeArgvUtf8(argc, utf8Argv);
#endif

	return res;
}

#endif
