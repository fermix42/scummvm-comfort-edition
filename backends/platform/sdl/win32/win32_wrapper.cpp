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

// For _tcscat
#define FORBIDDEN_SYMBOL_EXCEPTION_strcat
// Early storage errors occur before OSystem/logging exists.
#define FORBIDDEN_SYMBOL_EXCEPTION_fprintf
#define FORBIDDEN_SYMBOL_EXCEPTION_stderr

#define WIN32_LEAN_AND_MEAN
#include <sdkddkver.h>
#if !defined(_WIN32_IE) || (_WIN32_IE < 0x500)
 // required for SHGetSpecialFolderPath and SHGFP_TYPE_CURRENT in shlobj.h
#undef _WIN32_IE
#define _WIN32_IE 0x500
#endif
#if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x500)
 // required for VER_MAJORVERSION, VER_MINORVERSION and VER_GREATER_EQUAL in winnt.h
 // set to Windows 2000 which is the minimum needed for these constants
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x500
#endif
#include <windows.h>
#include <shellapi.h> // for CommandLineToArgvW()
#include <shlobj.h>
#include <tchar.h>

#include "common/ptr.h"
#include "common/array.h"
#include "common/system.h"
#include <stdio.h>
#include "common/scummsys.h"
#include "common/textconsole.h"
#include "backends/platform/sdl/win32/win32_wrapper.h"

// VerSetConditionMask, VerifyVersionInfo and SHGetFolderPath didn't appear until Windows 2000,
// so we need to check for them at runtime
ULONGLONG VerSetConditionMaskFunc(ULONGLONG dwlConditionMask, DWORD dwTypeMask, BYTE dwConditionMask) {
	typedef ULONGLONG(WINAPI *VerSetConditionMaskFunction)(ULONGLONG conditionMask, DWORD typeMask, BYTE conditionOperator);

	VerSetConditionMaskFunction verSetConditionMask = (VerSetConditionMaskFunction)(void *)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "VerSetConditionMask");
	if (verSetConditionMask == nullptr)
		return 0;

	return verSetConditionMask(dwlConditionMask, dwTypeMask, dwConditionMask);
}

BOOL VerifyVersionInfoFunc(LPOSVERSIONINFOEXA lpVersionInformation, DWORD dwTypeMask, DWORDLONG dwlConditionMask) {
	typedef BOOL(WINAPI *VerifyVersionInfoFunction)(LPOSVERSIONINFOEXA versionInformation, DWORD typeMask, DWORDLONG conditionMask);

	VerifyVersionInfoFunction verifyVersionInfo = (VerifyVersionInfoFunction)(void *)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "VerifyVersionInfoA");
	if (verifyVersionInfo == nullptr)
		return FALSE;

	return verifyVersionInfo(lpVersionInformation, dwTypeMask, dwlConditionMask);
}

HRESULT SHGetFolderPathFunc(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, LPTSTR pszPath) {
	typedef HRESULT (WINAPI *SHGetFolderPathFunc)(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, LPTSTR pszPath);

	SHGetFolderPathFunc pSHGetFolderPath = (SHGetFolderPathFunc)(void *)GetProcAddress(GetModuleHandle(TEXT("shell32.dll")), 
#ifndef UNICODE
		"SHGetFolderPathA"
#else
		"SHGetFolderPathW"
#endif
	);
	if (pSHGetFolderPath)
		return pSHGetFolderPath(hwnd, csidl, hToken, dwFlags, pszPath);

	typedef BOOL (WINAPI *SHGetSpecialFolderPathFunc)(HWND hwnd, LPTSTR pszPath, int csidl, BOOL fCreate);

	SHGetSpecialFolderPathFunc pSHGetSpecialFolderPath = (SHGetSpecialFolderPathFunc)(void *)GetProcAddress(GetModuleHandle(TEXT("shell32.dll")),
#ifndef UNICODE
		"SHGetSpecialFolderPathA"
#else
		"SHGetSpecialFolderPathW"
#endif
	);
	if (pSHGetSpecialFolderPath)
		return pSHGetSpecialFolderPath(hwnd, pszPath, csidl & ~CSIDL_FLAG_MASK, csidl & CSIDL_FLAG_CREATE) ? S_OK : E_FAIL;

	return E_NOTIMPL;
}

namespace Win32 {

static Common::String s_storageRoot;

static Common::String executableDirectory() {
	Common::Array<wchar_t> buffer;
	for (uint size = 256; size <= 32768; size *= 2) {
		buffer.resize(size);
		DWORD length = GetModuleFileNameW(nullptr, buffer.data(), size);
		if (!length) error("ScummVM CE: cannot determine executable directory (Windows error %lu)", GetLastError());
		if (length < size) {
			wchar_t *slash = wcsrchr(buffer.data(), L'\\');
			if (!slash) error("ScummVM CE: executable has no absolute directory");
			// Preserve the separator for an executable at a drive root.
			if (slash == buffer.data() + 2) ++slash;
			*slash = 0;
			char *utf8 = unicodeToAnsi(buffer.data());
			Common::String result(utf8); free(utf8);
			return result;
		}
	}
	error("ScummVM CE: executable path exceeds the Windows path limit");
}

Common::Path portablePath(const char *relative) {
	if (s_storageRoot.empty()) s_storageRoot = executableDirectory();
	return Common::Path(s_storageRoot, Common::Path::kNativeSeparator).join(relative);
}

Common::Path resolvePortablePath(const Common::Path &path) {
	Common::String text = path.toString(Common::Path::kNativeSeparator);
	// Absolute drive, UNC and device paths are explicit overrides.
	if (text.empty() || text[0] == '\\' || text[0] == '/' || (text.size() > 1 && text[1] == ':')) return path;
	return portablePath().join(path).normalize();
}

Common::Path relativePortablePath(const Common::Path &path) {
	Common::String text = path.toString('/');
	Common::String root = portablePath().toString('/');
	if (!root.hasSuffix("/")) root += '/';
	if (text.size() >= root.size() && text.substr(0, root.size()).equalsIgnoreCase(root))
		return Common::Path(text.substr(root.size()));
	return path;
}

void portableStorageError(const Common::Path &path) {
	Common::String message = Common::String::format(
	    "ScummVM CE cannot write '%s'. No Windows profile fallback is used. "
	    "Choose a writable destination or move the complete installation to a writable folder such as D:\\Games\\ScummVM CE. "
	    "Existing files have not been migrated.", path.toString(Common::Path::kNativeSeparator).c_str());
	// Before OSystem exists, error() has no log/message-box destination. Show
	// the error to GUI launches, or the inherited stderr pipe to CLI callers.
	if (!g_system) {
		fprintf(stderr, "%s\n", message.c_str());
		DWORD type = GetFileType(GetStdHandle(STD_ERROR_HANDLE));
		if (type != FILE_TYPE_PIPE && type != FILE_TYPE_DISK && type != FILE_TYPE_CHAR) {
			wchar_t *wide = ansiToUnicode(message.c_str());
			MessageBoxW(nullptr, wide, L"ScummVM CE portable storage", MB_OK | MB_ICONERROR);
			free(wide);
		}
	}
	error("%s", message.c_str());
}

void ensurePortableDirectory(const Common::Path &path) {
	Common::String text = path.toString(Common::Path::kNativeSeparator);
	wchar_t *wide = ansiToUnicode(text.c_str());
	DWORD attrs = GetFileAttributesW(wide);
	if (attrs != INVALID_FILE_ATTRIBUTES) {
		free(wide);
		if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) portableStorageError(path);
		return;
	}
	// Recursion is bounded by path components; tolerate concurrent creators.
	Common::Path parent = path.getParent();
	if (!parent.empty() && parent != path) ensurePortableDirectory(parent);
	bool created = CreateDirectoryW(wide, nullptr) != 0;
	if (!created && GetLastError() == ERROR_ALREADY_EXISTS) {
		DWORD racedAttributes = GetFileAttributesW(wide);
		created = racedAttributes != INVALID_FILE_ATTRIBUTES && (racedAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}
	free(wide);
	if (!created) portableStorageError(path);
}

void initializePortableStorage(const Common::String &isolatedRoot) {
	// Called only after the native controller has validated ownership of its root.
	s_storageRoot = isolatedRoot.empty() ? executableDirectory() : isolatedRoot;
	// Legacy engine diagnostics use relative C stdio names. Anchor those too;
	// all primary persistent destinations above are independently absolute.
	wchar_t *root = ansiToUnicode(s_storageRoot.c_str());
	bool changed = SetCurrentDirectoryW(root) != 0; free(root);
	if (!changed) portableStorageError(portablePath());
	// Process-local temporary-file APIs must not inherit the Windows TEMP path.
	// This directory is needed before initializing third-party runtime libraries.
	configurePortableTemp(portablePath("temp"));
}

void configurePortableTemp(const Common::Path &path) {
	ensurePortableDirectory(path);
	wchar_t *wide = ansiToUnicode(path.toString(Common::Path::kNativeSeparator).c_str());
	bool configured = SetEnvironmentVariableW(L"TEMP", wide) && SetEnvironmentVariableW(L"TMP", wide);
	free(wide);
	if (!configured) portableStorageError(path);
	// Existing directory attributes do not establish ACL writability. Probe
	// with CREATE_NEW and delete-on-close; never touch an existing file.
	HANDLE probe = INVALID_HANDLE_VALUE;
	for (uint attempt = 0; attempt < 16; ++attempt) {
		Common::String name = Common::String::format("ce-write-probe-%lu-%lu-%u.tmp", GetCurrentProcessId(), GetTickCount(), attempt);
		wchar_t *filename = ansiToUnicode(path.join(name).toString(Common::Path::kNativeSeparator).c_str());
		probe = CreateFileW(filename, GENERIC_WRITE, 0, nullptr, CREATE_NEW,
		                    FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
		DWORD code = GetLastError(); free(filename);
		if (probe != INVALID_HANDLE_VALUE || code != ERROR_FILE_EXISTS) break;
	}
	if (probe == INVALID_HANDLE_VALUE) portableStorageError(path);
	CloseHandle(probe);
}

void tracePortableWrite(const Common::String &path, bool success) {
	// Optional explicit diagnostics, never enabled by normal execution. Win32
	// calls avoid recursively tracing the trace itself. No profile lookup.
	wchar_t destination[32768];
	DWORD size = GetEnvironmentVariableW(L"SCUMMVM_CE_WRITE_TRACE", destination, ARRAYSIZE(destination));
	if (!size || size >= ARRAYSIZE(destination)) return;
	HANDLE file = CreateFileW(destination, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
	                          nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) return;
	wchar_t *wide = ansiToUnicode(path.c_str());
	Common::Array<wchar_t> absolute;
	absolute.resize(GetFullPathNameW(wide, 0, nullptr, nullptr));
	Common::String resolved(path);
	if (absolute.size() && GetFullPathNameW(wide, absolute.size(), absolute.data(), nullptr)) {
		char *utf8 = unicodeToAnsi(absolute.data()); resolved = utf8; free(utf8);
	}
	free(wide);
	Common::String line = Common::String(success ? "WRITE\t" : "FAILED\t") + resolved + "\n";
	DWORD written = 0; WriteFile(file, line.c_str(), line.size(), &written, nullptr); CloseHandle(file);
}

void getProcessDirectory(TCHAR *processDirectory, DWORD size) {
	TCHAR *directory = stringToTchar(executableDirectory());
	if (_tcslen(directory) >= size) {
		free(directory);
		error("ScummVM CE: executable directory exceeds this resource helper's buffer; path was not truncated");
	}
	_tcscpy(processDirectory, directory);
	free(directory);
}

bool confirmWindowsVersion(int majorVersion, int minorVersion) {
	OSVERSIONINFOEXA versionInfo;
	DWORDLONG conditionMask = 0;

	ZeroMemory(&versionInfo, sizeof(OSVERSIONINFOEXA));
	versionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);
	versionInfo.dwMajorVersion = majorVersion;
	versionInfo.dwMinorVersion = minorVersion;

	conditionMask = VerSetConditionMaskFunc(conditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
	conditionMask = VerSetConditionMaskFunc(conditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);

	return VerifyVersionInfoFunc(&versionInfo, VER_MAJORVERSION | VER_MINORVERSION, conditionMask);
}

// for using ScopedPtr with malloc/free
template <typename T>
struct Freer {
	inline void operator()(T *object) {
		free(object);
	}
};

bool moveFile(const Common::String &src, const Common::String &dst) {
	Common::ScopedPtr<TCHAR, Freer<TCHAR>> tSrc(stringToTchar(src));
	Common::ScopedPtr<TCHAR, Freer<TCHAR>> tDst(stringToTchar(dst));

	if (MoveFileEx(tSrc.get(), tDst.get(), MOVEFILE_REPLACE_EXISTING)) {
		return true;
	}

	// MoveFileEx may not be supported on the platform (Win9x)
	if (GetLastError() == ERROR_CALL_NOT_IMPLEMENTED) {
		// Fall back to deleting the destination before using MoveFile.
		// MoveFile requires that the destination not already exist.
		DeleteFile(tDst.get());
		return MoveFile(tSrc.get(), tDst.get());
	}

	return false;
}

bool isDriveCD(char driveLetter) {
	TCHAR drivePath[] = TEXT("x:\\");
	drivePath[0] = (TCHAR)driveLetter;

	return (GetDriveType(drivePath) == DRIVE_CDROM);
}

wchar_t *ansiToUnicode(const char *s) {
#ifndef UNICODE
	uint codePage = CP_ACP;
#else
	uint codePage = CP_UTF8;
#endif
	DWORD size = MultiByteToWideChar(codePage, 0, s, -1, nullptr, 0);

	if (size > 0) {
		LPWSTR result = (LPWSTR)calloc(size, sizeof(WCHAR));
		if (MultiByteToWideChar(codePage, 0, s, -1, result, size) != 0)
			return result;
	}

	return nullptr;
}

char *unicodeToAnsi(const wchar_t *s) {
#ifndef UNICODE
	uint codePage = CP_ACP;
#else
	uint codePage = CP_UTF8;
#endif
	DWORD size = WideCharToMultiByte(codePage, 0, s, -1, nullptr, 0, nullptr, nullptr);

	if (size > 0) {
		char *result = (char *)calloc(size, sizeof(char));
		if (WideCharToMultiByte(codePage, 0, s, -1, result, size, nullptr, nullptr) != 0)
			return result;
	}

	return nullptr;
}

TCHAR *stringToTchar(const Common::String& s) {
#ifndef UNICODE
	char *t = (char *)malloc(s.size() + 1);
	Common::strcpy_s(t, s.size() + 1, s.c_str());
	return t;
#else
	return ansiToUnicode(s.c_str());
#endif
}

Common::String tcharToString(const TCHAR *t) {
#ifndef UNICODE
	return t;
#else
	char *utf8 = unicodeToAnsi(t);
	Common::String s = utf8;
	free(utf8);
	return s;
#endif
}

#ifdef UNICODE
char **getArgvUtf8(int *argc) {
	// get command line arguments in wide-character
	LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), argc);

	// convert each argument to utf8
	char **argv = (char **)malloc((*argc + 1) * sizeof(char *));
	for (int i = 0; i < *argc; ++i) {
		argv[i] = Win32::unicodeToAnsi(wargv[i]);
	}
	argv[*argc] = nullptr; // null terminated array

	LocalFree(wargv);
	return argv;
}

void freeArgvUtf8(int argc, char **argv) {
	for (int i = 0; i < argc; ++i) {
		free(argv[i]);
	}
	free(argv);
}
#endif

} // End of namespace Win32
