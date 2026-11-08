#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Permanent Windows CE storage contract; no game assets or profile writes needed.

Run with --exe for actual launch/path-boundary checks in disposable copies.
The inventory includes disabled/vendor/read-only sites: a changed site requires
review, rather than assuming every engine uses SaveFileManager.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
INVENTORY = ROOT/'doc/ce-portable-storage/strict-write-sites.json'
PATTERN = re.compile(r'\b(?:fopen\w*|freopen\w*|_wfopen\w*|fwrite|CreateFile[AW]?|DeleteFile[AW]?|MoveFile\w*|remove|rename|_?mkdir|CreateDirectory\w*|SHGetFolderPath\w*|SHGetKnownFolderPath|GetTempPath\w*|getenv|SDL_getenv|GetEnvironmentVariable\w*)\s*\(|\b(?:ofstream|fstream)\b')


def inventory():
    sites = {}
    tracked = set(subprocess.check_output(['git', '-C', str(ROOT), 'ls-files'], text=True).splitlines())
    for directory in ('engines', 'common', 'graphics', 'audio', 'gui', 'image', 'video',
                      'backends/fs', 'backends/saves', 'backends/plugins',
                      'backends/networking', 'backends/cloud', 'backends/platform/sdl/win32',
                      'backends/graphics', 'backends/imgui', 'backends/updates',
                      'backends/taskbar', 'backends/text-to-speech'):
        for path in sorted((ROOT/directory).rglob('*')):
            if path.suffix not in ('.cpp', '.h', '.c'): continue
            # Other-platform backends do not participate in Windows CE.
            relative = path.relative_to(ROOT).as_posix()
            if relative not in tracked: continue
            if relative.startswith(('backends/fs/', 'backends/saves/', 'backends/plugins/')):
                if path.parent.name not in ('fs', 'saves', 'plugins', 'windows', 'win32', 'default', 'std'): continue
            lines = [line.strip() for line in path.read_text(encoding='utf-8', errors='replace').splitlines() if PATTERN.search(line)]
            if lines: sites[relative] = lines
    return sites


class Contract(unittest.TestCase):
    def test_engine_and_subsystem_raw_sites_require_review(self):
        self.assertEqual(inventory(), json.loads(INVENTORY.read_text(encoding='utf-8')))

    def test_no_windows_profile_default(self):
        text = (ROOT/'backends/platform/sdl/win32/win32.cpp').read_text()
        for token in ('getApplicationDataDirectory', 'detectPortableConfigFile', 'CSIDL_APPDATA', 'CSIDL_MYPICTURES'):
            self.assertNotIn(token, text)
        self.assertIn('portablePath(DEFAULT_CONFIG_FILE)', text)
        self.assertIn('resolveStoragePath(key, path)', text)

    def test_common_writes_and_mutations_are_guarded(self):
        for name in ('backends/fs/stdiostream.cpp', 'backends/fs/windows/windows-fs.cpp',
                     'backends/saves/windows/windows-saves.cpp'):
            self.assertIn('requirePortableStoragePath', (ROOT/name).read_text())
        text = (ROOT/'backends/platform/sdl/win32/win32_wrapper.cpp').read_text()
        self.assertIn('FILE_ATTRIBUTE_REPARSE_POINT', text)
        self.assertIn('requirePortableStoragePath(Common::Path(dst', text)
        self.assertIn('requirePortableStoragePath(Common::Path(src', text)

    def test_cli_checks_precede_backend_startup(self):
        text = (ROOT/'backends/platform/sdl/win32/win32-main.cpp').read_text()
        self.assertLess(text.index('requirePortableStoragePath'), text.index('g_system = new'))
        self.assertIn('"--config", "-c", "--savepath", "--logfile"', text)

    def test_asset_keys_are_separate(self):
        text = (ROOT/'backends/platform/sdl/win32/win32.cpp').read_text()
        keys = text.split('static bool isStorageKey', 1)[1].split('\n}', 1)[0]
        for key in ('"path"', '"extrapath"', '"soundfont"', '"themepath"'):
            self.assertNotIn(key, keys)


def runtime(exe, artifacts):
    artifacts.mkdir(parents=True, exist_ok=False)
    install = artifacts/'CE spaces 日本語'; install.mkdir()
    for file in exe.parent.iterdir():
        if file.suffix.lower() in ('.exe', '.dll') and file.name != 'scummvm-tests.exe': shutil.copy2(file, install/file.name)
    outside = artifacts/'official data'; outside.mkdir()
    sentinel = outside/'scummvm.ini'; sentinel.write_text('[scummvm]\nkeep=official\n')
    before = sentinel.read_bytes()
    cwd = artifacts/'unrelated cwd'; cwd.mkdir()
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', SCUMMVM_CE_WRITE_TRACE='1')
    checks = []
    def launch(folder, arguments, success):
        result = subprocess.run([str(folder/exe.name), *arguments], cwd=cwd, env=env, capture_output=True, timeout=30)
        assert (result.returncode == 0) == success, (arguments, result.returncode, result.stderr)
        if not success: assert b'No Windows profile fallback' in result.stderr, result.stderr
        checks.append({'args': arguments, 'exit': result.returncode})
    launch(install, ['--version'], True)
    for option in ('--config', '-c', '--savepath', '--logfile', '--screenshotpath', '--iconspath', '--dlcspath'):
        launch(install, [option+'='+str(sentinel), '--version'], False)
    for path in (install/'../escaped', artifacts/'CE spaces 日本語 sibling/saves', install/'temp/../../escaped'):
        launch(install, ['--savepath='+str(path), '--version'], False)
    (install/'local saves').mkdir()
    launch(install, ['--savepath='+str(install/'local saves'), '--version'], True)
    assert sentinel.read_bytes() == before and list(outside.iterdir()) == [sentinel]
    relocated = artifacts/'moved Ω'
    for attempt in range(30):
        try:
            install.rename(relocated)
            break
        except PermissionError:
            if attempt == 29: raise
            time.sleep(0.1)  # Windows image/antivirus handles can outlive exit.
    launch(relocated, ['--version'], True)
    assert not list(cwd.iterdir())
    report = {'status': 'PASS', 'executable_sha256': hashlib.sha256(exe.read_bytes()).hexdigest(), 'checks': checks,
              'scope': 'Actual fresh launches, CLI boundaries, Unicode, unrelated CWD and relocation; gameplay is tested separately.'}
    (artifacts/'verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--update-inventory', action='store_true', help='Record reviewed raw write/path sites; inspect the diff before committing')
    parser.add_argument('--exe', type=Path)
    parser.add_argument('--artifacts', type=Path)
    args = parser.parse_args()
    if args.update_inventory:
        INVENTORY.write_text(json.dumps(inventory(), indent=2)+'\n', encoding='utf-8')
    else:
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Contract))
        if not result.wasSuccessful(): raise SystemExit(1)
        if args.exe:
            assert args.artifacts, '--artifacts required with --exe'
            runtime(args.exe.resolve(strict=True), args.artifacts.resolve())
