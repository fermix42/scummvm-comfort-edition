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

#ifdef WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#if defined(__GNUC__) && defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR)
// required for SHGFP_TYPE_CURRENT in shlobj.h
#define _WIN32_IE 0x500
#endif
#include <shlobj.h>
#include <tchar.h>

#include "common/scummsys.h"
#include "common/config-manager.h"
#include "engines/ce_test_cli.h"
#include "common/error.h"
#include "common/textconsole.h"

#include "backends/audiocd/win32/win32-audiocd.h"
#include "backends/platform/sdl/win32/win32.h"
#include "backends/platform/sdl/win32/win32-window.h"
#include "backends/platform/sdl/win32/win32_wrapper.h"
#include "backends/saves/windows/windows-saves.h"
#include "backends/fs/windows/windows-fs-factory.h"
#include "backends/taskbar/win32/win32-taskbar.h"
#include "backends/updates/win32/win32-updates.h"
#include "backends/dialogs/win32/win32-dialogs.h"
#include "backends/printing/win32/win32-printman.h"

#include "common/memstream.h"
#include "common/md5.h"
#include "common/ustr.h"

#if defined(USE_TTS)
#include "backends/text-to-speech/windows/windows-text-to-speech.h"
#endif

#define DEFAULT_CONFIG_FILE "scummvm.ini"

OSystem_Win32::OSystem_Win32() {
}

void OSystem_Win32::init() {
	// Initialize File System Factory
	_fsFactory = new WindowsFilesystemFactory();

	// Create Win32 specific window
	initSDL();
	_window = new SdlWindow_Win32();

#if defined(USE_TASKBAR)
	// Initialize taskbar manager
	_taskbarManager = new Win32TaskbarManager((SdlWindow_Win32*)_window);
#endif

#if defined(USE_SYSDIALOGS)
	// Initialize dialog manager
	_dialogManager = new Win32DialogManager((SdlWindow_Win32*)_window);
#endif

#if defined(USE_JPEG)
	initializeJpegLibraryForWin95();
#endif

#if defined(USE_SYSTEM_PRINTING)
	_printingManager = createWin32PrintingManager();
#endif

	// Invoke parent implementation of this method
	OSystem_SDL::init();
}

WORD GetCurrentSubsystem() {
	// HMODULE is the module base address. And the PIMAGE_DOS_HEADER is located at the beginning.
	PIMAGE_DOS_HEADER EXEHeader = (PIMAGE_DOS_HEADER)GetModuleHandle(nullptr);
	assert(EXEHeader->e_magic == IMAGE_DOS_SIGNATURE);
	// PIMAGE_NT_HEADERS is bitness dependant.
	// Conveniently, since it's for our own process, it's always the correct bitness.
	// IMAGE_NT_HEADERS has to be found using a byte offset from the EXEHeader,
	// which requires the ugly cast.
	PIMAGE_NT_HEADERS PEHeader = (PIMAGE_NT_HEADERS)(((char*)EXEHeader) + EXEHeader->e_lfanew);
	assert(PEHeader->Signature == IMAGE_NT_SIGNATURE);
	return PEHeader->OptionalHeader.Subsystem;
}

void OSystem_Win32::initBackend() {
	// The console window is enabled for the console subsystem,
	// since Windows already creates the console window for us
	ConfMan.registerDefault("console", GetCurrentSubsystem() == IMAGE_SUBSYSTEM_WINDOWS_CUI);

	// Enable or disable the window console window
	if (ConfMan.getBool("console")) {
		if (AllocConsole()) {
			freopen("CONIN$","r",stdin);
			freopen("CONOUT$","w",stdout);
			freopen("CONOUT$","w",stderr);
		}
		SetConsoleTitle(TEXT("ScummVM Status Window"));
	} else {
		FreeConsole();
	}

	ConfMan.registerDefault("cachepath", Common::Path("cache"));
	ConfMan.registerDefault("temppath", Common::Path("temp"));
	ConfMan.registerDefault("screenshotpath", Common::Path("screenshots"));
	Win32::configurePortableTemp(ConfMan.getPath("temppath"));

	// Create the savefile manager
	if (_savefileManager == nullptr)
		_savefileManager = new WindowsSaveFileManager(true);

#if defined(USE_SPARKLE)
	// Initialize updates manager
	_updateManager = new Win32UpdateManager((SdlWindow_Win32*)_window);
#endif

	// Initialize text to speech
#ifdef USE_TTS
	_textToSpeechManager = new WindowsTextToSpeechManager();
#endif

	// Invoke parent implementation of this method
	OSystem_SDL::initBackend();
}

#ifdef USE_OPENGL
OSystem_SDL::GraphicsManagerType OSystem_Win32::getDefaultGraphicsManager() const {
	return GraphicsManagerOpenGL;
}
#endif

bool OSystem_Win32::hasFeature(Feature f) {
	if (CETestCLI::active() && (f == kFeatureDisplayLogFile || f == kFeatureOpenUrl || f == kFeatureSystemBrowserDialog)) return false;
	if (f == kFeatureDisplayLogFile || f == kFeatureOpenUrl)
		return true;

#ifdef USE_SYSDIALOGS
	if (f == kFeatureSystemBrowserDialog)
		return true;
#endif

	return OSystem_SDL::hasFeature(f);
}

bool OSystem_Win32::displayLogFile() {
	if (CETestCLI::active()) return false;
	if (_logFilePath.empty())
		return false;

	// Try opening the log file with the default text editor
	// log files should be registered as "txtfile" by default and thus open in the default text editor
	TCHAR *tLogFilePath = Win32::stringToTchar(_logFilePath.toString(Common::Path::kNativeSeparator));
	SHELLEXECUTEINFO sei;

	memset(&sei, 0, sizeof(sei));
	sei.fMask  = SEE_MASK_FLAG_NO_UI;
	sei.hwnd   = getHwnd();
	sei.lpFile = tLogFilePath;
	sei.nShow  = SW_SHOWNORMAL;

	if (ShellExecuteEx(&sei)) {
		free(tLogFilePath);
		return true;
	}

	// ShellExecute with the default verb failed, try the "Open with..." dialog
	PROCESS_INFORMATION processInformation;
	STARTUPINFO startupInfo;
	memset(&processInformation, 0, sizeof(processInformation));
	memset(&startupInfo, 0, sizeof(startupInfo));
	startupInfo.cb = sizeof(startupInfo);

	TCHAR cmdLine[MAX_PATH * 2];  // CreateProcess may change the contents of cmdLine
	_stprintf(cmdLine, TEXT("rundll32 shell32.dll,OpenAs_RunDLL %s"), tLogFilePath);
	BOOL result = CreateProcess(nullptr,
	                            cmdLine,
	                            nullptr,
	                            nullptr,
	                            FALSE,
	                            NORMAL_PRIORITY_CLASS,
	                            nullptr,
	                            nullptr,
	                            &startupInfo,
	                            &processInformation);
	free(tLogFilePath);
	if (result) {
		CloseHandle(processInformation.hProcess);
		CloseHandle(processInformation.hThread);
		return true;
	}

	return false;
}

bool OSystem_Win32::openUrl(const Common::String &url) {
	if (CETestCLI::active()) return false;
	TCHAR *tUrl = Win32::stringToTchar(url);
	SHELLEXECUTEINFO sei;

	memset(&sei, 0, sizeof(sei));
	sei.cbSize = sizeof(sei);
	sei.fMask  = SEE_MASK_FLAG_NO_UI;
	sei.hwnd   = getHwnd();
	sei.lpFile = tUrl;
	sei.nShow  = SW_SHOWNORMAL;

	BOOL success = ShellExecuteEx(&sei);

	free(tUrl);
	if (!success) {
		warning("ShellExecuteEx failed: error = %08lX", GetLastError());
		return false;
	}
	return true;
}

void OSystem_Win32::logMessage(LogMessageType::Type type, const char *message) {
	OSystem_SDL::logMessage(type, message);

#if defined( USE_WINDBG )
	TCHAR *tMessage = Win32::stringToTchar(message);
	OutputDebugString(tMessage);
	free(tMessage);
#endif
}

Common::String OSystem_Win32::getSystemLanguage() const {
#if defined(USE_DETECTLANG) && defined(USE_TRANSLATION)
	// We can not use "setlocale" (at least not for MSVC builds), since it
	// will return locales like: "English_USA.1252", thus we need a special
	// way to determine the locale string for Win32.
	TCHAR langName[9];
	TCHAR ctryName[9];

	if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, langName, ARRAYSIZE(langName)) != 0 &&
		GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, ctryName, ARRAYSIZE(ctryName)) != 0) {
		Common::String localeName = Win32::tcharToString(langName);
		localeName += "_";
		localeName += Win32::tcharToString(ctryName);

		return localeName;
	}
#endif // USE_DETECTLANG
	// Falback to SDL implementation
	return OSystem_SDL::getSystemLanguage();
}

// CE defaults are registered as relative paths, never serialized as the build location.
Common::Path OSystem_Win32::getDefaultIconsPath() {
	return Common::Path("data/icons");
}

Common::Path OSystem_Win32::getDefaultDLCsPath() {
	return Common::Path("data/dlcs");
}

static bool isStorageKey(const Common::String &key) {
	return key == "savepath" || key == "screenshotpath" || key == "logfile" ||
	       key == "iconspath" || key == "dlcspath" || key == "cachepath" || key == "temppath";
}

Common::Path OSystem_Win32::resolveStoragePath(const Common::String &key, const Common::Path &path) const {
	if (CETestCLI::active()) {
		// An owned test profile cannot redirect auxiliary writes outside its
		// root. Save/config/log overrides are already enforced by prepare().
		if (key == "screenshotpath") return Win32::portablePath("screenshots");
		if (key == "cachepath") return Win32::portablePath("cache");
		if (key == "temppath") return Win32::portablePath("temp");
		if (key == "iconspath") return Win32::portablePath("data/icons");
		if (key == "dlcspath") return Win32::portablePath("data/dlcs");
	}
	if (key == "engineprefs" && !path.empty()) {
		Common::String source = path.toString('/');
		Common::MemoryReadStream input((const byte *)source.c_str(), source.size());
		return Win32::portablePath("data/engine-preferences").join(ConfMan.getActiveDomainName()).join(
		    Common::computeStreamMD5AsString(input) + "-" + path.baseName());
	}
	if (key == "enginelog" && !path.empty()) {
		Common::Path log = ConfMan.getPath("logfile");
		Common::Path directory = log.empty() ? Win32::portablePath("logs") : log.getParent();
		directory = directory.join(ConfMan.getActiveDomainName());
		Win32::ensurePortableDirectory(directory);
		return directory.join(path.baseName());
	}
	if (key == "enginecache" && !path.empty()) {
		// This is an engine-derived cache name, not an explicit user storage
		// selection. Never write a generated cache beside read-only game assets.
		Common::String source = path.toString('/');
		Common::MemoryReadStream input((const byte *)source.c_str(), source.size());
		Common::String name = Common::computeStreamMD5AsString(input) + "-" + path.baseName();
		Common::Path cache;
		if (path.baseName().hasSuffix(".cfg")) {
			cache = Win32::portablePath("data/hpl1"); // Persistent engine preferences, not disposable cache.
		} else {
			cache = ConfMan.getPath("cachepath");
			if (cache.empty()) cache = Win32::portablePath("cache");
			cache = cache.join("hpl1");
		}
		return cache.join(ConfMan.getActiveDomainName()).join(name);
	}
	if (key == "savepath") {
		if (path.empty()) return Win32::portablePath("saves");
		if (path == "None") return path; // Let the upstream legacy-value repair run.
	}
	if (isStorageKey(key) && !path.empty()) {
		Common::Path resolved = Win32::resolvePortablePath(path);
		if (Win32::isPortableStoragePath(resolved)) return resolved;
		// Do not read, migrate or overwrite legacy external storage. Keep game
		// asset keys separate. The normalized value is serialized on next flush.
		warning("ScummVM CE: external %s '%s' replaced by its portable default; existing data was not migrated",
		        key.c_str(), path.toString(Common::Path::kNativeSeparator).c_str());
		if (key == "savepath") return Win32::portablePath("saves");
		if (key == "screenshotpath") return Win32::portablePath("screenshots");
		if (key == "logfile") return Win32::portablePath("logs/scummvm.log");
		if (key == "iconspath") return Win32::portablePath("data/icons");
		if (key == "dlcspath") return Win32::portablePath("data/dlcs");
		if (key == "cachepath") return Win32::portablePath("cache");
		if (key == "temppath") return Win32::portablePath("temp");
	}
	return path;
}

Common::Path OSystem_Win32::storagePathForConfig(const Common::String &key, const Common::Path &path) const {
	return isStorageKey(key) ? Win32::relativePortablePath(resolveStoragePath(key, path)) : path;
}

Common::Path OSystem_Win32::getScreenshotsPath() {
	Common::Path path = ConfMan.getPath("screenshotpath");
	if (path.empty()) path = Win32::portablePath("screenshots");
	Win32::ensurePortableDirectory(path);
	return path;
}

Common::Path OSystem_Win32::getDefaultConfigFileName() {
	return Win32::portablePath(DEFAULT_CONFIG_FILE);
}

Common::WriteStream *OSystem_Win32::createConfigWriteStream() {
	Common::WriteStream *stream = OSystem::createConfigWriteStream();
	if (!stream) Win32::portableStorageError(getDefaultConfigFileName());
	return stream;
}

Common::Path OSystem_Win32::getDefaultLogFileName() {
	Win32::ensurePortableDirectory(Win32::portablePath("logs"));
	return Win32::portablePath("logs/scummvm.log");
}

namespace {

class Win32ResourceArchive final : public Common::Archive {
	friend BOOL CALLBACK EnumResNameProc(HMODULE hModule, LPCTSTR lpszType, LPTSTR lpszName, LONG_PTR lParam);
public:
	Win32ResourceArchive();

	bool hasFile(const Common::Path &path) const override;
	int listMembers(Common::ArchiveMemberList &list) const override;
	const Common::ArchiveMemberPtr getMember(const Common::Path &path) const override;
	Common::SeekableReadStream *createReadStreamForMember(const Common::Path &path) const override;
private:
	typedef Common::List<Common::Path> FilenameList;

	FilenameList _files;
};

BOOL CALLBACK EnumResNameProc(HMODULE hModule, LPCTSTR lpszType, LPTSTR lpszName, LONG_PTR lParam) {
	if (IS_INTRESOURCE(lpszName))
		return TRUE;

	Win32ResourceArchive *arch = (Win32ResourceArchive *)lParam;
	Common::String filename = Win32::tcharToString(lpszName);
	// We use / as path separator in resources
	arch->_files.push_back(Common::Path(filename, '/'));
	return TRUE;
}

Win32ResourceArchive::Win32ResourceArchive() {
	EnumResourceNames(nullptr, MAKEINTRESOURCE(256), &EnumResNameProc, (LONG_PTR)this);
}

bool Win32ResourceArchive::hasFile(const Common::Path &path) const {
	for (const auto &curPath : _files) {
		if (curPath.equalsIgnoreCase(path))
			return true;
	}

	return false;
}

int Win32ResourceArchive::listMembers(Common::ArchiveMemberList &list) const {
	int count = 0;

	for (FilenameList::const_iterator i = _files.begin(); i != _files.end(); ++i, ++count)
		list.push_back(Common::ArchiveMemberPtr(new Common::GenericArchiveMember(*i, *this)));

	return count;
}

const Common::ArchiveMemberPtr Win32ResourceArchive::getMember(const Common::Path &path) const {
	return Common::ArchiveMemberPtr(new Common::GenericArchiveMember(path, *this));
}

Common::SeekableReadStream *Win32ResourceArchive::createReadStreamForMember(const Common::Path &path) const {
	// We store paths in resources using / separator
	Common::String name = path.toString('/');
	TCHAR *tName = Win32::stringToTchar(name);
	HRSRC resource = FindResource(nullptr, tName, MAKEINTRESOURCE(256));
	free(tName);

	if (resource == nullptr)
		return nullptr;

	HGLOBAL handle = LoadResource(nullptr, resource);

	if (handle == nullptr)
		return nullptr;

	const byte *data = (const byte *)LockResource(handle);

	if (data == nullptr)
		return nullptr;

	uint32 size = SizeofResource(nullptr, resource);

	if (size == 0)
		return nullptr;

	return new Common::MemoryReadStream(data, size);
}

} // End of anonymous namespace

void OSystem_Win32::addSysArchivesToSearchSet(Common::SearchSet &s, int priority) {
	s.add("Win32Res", new Win32ResourceArchive(), priority);

	OSystem_SDL::addSysArchivesToSearchSet(s, priority);
}

AudioCDManager *OSystem_Win32::createAudioCDManager() {
	return createWin32AudioCDManager();
}

uint32 OSystem_Win32::getOSDoubleClickTime() const {
	return GetDoubleClickTime();
}

// libjpeg-turbo uses SSE instructions that error on at least some Win95 machines.
// These can be disabled with an environment variable. Fixes bug #13643
#if defined(USE_JPEG)
void OSystem_Win32::initializeJpegLibraryForWin95() {
	OSVERSIONINFO versionInfo;
	ZeroMemory(&versionInfo, sizeof(versionInfo));
	versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
	GetVersionEx(&versionInfo);

	// Is Win95?
	if (versionInfo.dwMajorVersion == 4 && versionInfo.dwMinorVersion == 0) {
		// Disable SSE instructions in libjpeg-turbo.
		// This limits detected extensions to 3DNOW and MMX.
		_tputenv(TEXT("JSIMD_FORCE3DNOW=1"));
	}
}
#endif

#endif
