#!/usr/bin/env python3
"""Audit both integration parents and the generated Release x64 engine coverage.

Run from any directory after generating build-scummvm. These checks compare Git
blobs, not working-copy newlines. They supplement, not replace, runtime testing.
"""
from pathlib import Path
import json
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
UPSTREAM = '7d2709da5136866589f7602f5f5a7b17de03420e'
CE = '29e2ceb01611ac18654d82bfe2116db8183bd53d'
BASE = 'b72dad4a9c8e91139935cb83d234722199bc25c1'
PORTABLE = 'd9868b2d7bc10be6752c6b9899f01c8aea20c3cd'
NATIVE_CLI = '80f136b4e8d662225d132041d134d3acf0c079b7'
CONSOLIDATION_EDITORIAL = {'.gitignore', 'AGENTS.md', 'README.md'}
# Reviewed opt-in native controller hooks. All other upstream/CE blobs retain
# the original exact-identity checks. Runtime tests verify these adaptations.
NATIVE_CLI_ADAPTATIONS = {
    'backends/events/default/default-events.cpp',
    'backends/events/sdl/sdl2-events.cpp',
    'backends/graphics/sdl/sdl-graphics.cpp',
    'backends/platform/sdl/sdl-window.cpp',
    'backends/platform/sdl/sdl.cpp',
    'backends/platform/sdl/win32/win32-main.cpp',
    'backends/platform/sdl/win32/win32.cpp',
    'engines/ce_achievements.cpp', 'engines/ce_achievements.h',
    'engines/engine.cpp', 'engines/engine.h',
    'engines/grim/grim.cpp', 'engines/grim/grim.h',
    'engines/grim/module.mk', 'engines/module.mk',
}


def git(*args):
    return subprocess.check_output(['git', '-c', f'safe.directory={ROOT.as_posix()}',
                                    '-C', str(ROOT), *args]).decode('utf-8')


def tree(ref):
    result = {}
    for entry in git('ls-tree', '-rz', '--full-tree', ref).split('\0'):
        if entry:
            metadata, name = entry.split('\t', 1)
            result[name] = metadata
    return result


class IntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ce = tree(CE)
        cls.base = tree(BASE)
        cls.upstream = tree(UPSTREAM)
        cls.integrated = tree('HEAD')
        cls.portable = tree(PORTABLE)
        strict = json.loads((ROOT/'doc/ce-portable-storage/strict-runtime-blobs.json').read_text())
        cls.strict = {p: cls.portable[p].rsplit(' ', 1)[0]+' '+blob for p, blob in strict.items()}
        cls.portable_adaptations = set(git('diff', '--name-only', NATIVE_CLI, PORTABLE).splitlines())
        cls.custom = {p for p in cls.ce.keys() | cls.base.keys()
                      if cls.ce.get(p) != cls.base.get(p)}

    def test_exact_official_tag(self):
        self.assertEqual(git('rev-parse', 'v2026.4.0^{}').strip(), UPSTREAM)
        subprocess.check_call(['git', '-c', f'safe.directory={ROOT.as_posix()}',
                               '-C', str(ROOT), 'merge-base', '--is-ancestor',
                               UPSTREAM, 'HEAD'])

    def test_ce_history_retained(self):
        subprocess.check_call(['git', '-c', f'safe.directory={ROOT.as_posix()}',
                               '-C', str(ROOT), 'merge-base', '--is-ancestor', CE, 'HEAD'])

    def test_no_upstream_paths_missing(self):
        self.assertFalse(self.upstream.keys() - self.integrated.keys())

    def test_all_non_ce_upstream_blobs_exact(self):
        adaptations = {'devtools/create_project/create_project.cpp',
                       'test/system/null_osystem.cpp', 'test/system/null_osystem.h'} | NATIVE_CLI_ADAPTATIONS
        mismatches = [p for p, blob in self.upstream.items()
                      if p not in self.custom and p not in adaptations and p not in CONSOLIDATION_EDITORIAL
                      and self.integrated.get(p) != self.strict.get(p, self.portable.get(p) if p in self.portable_adaptations else blob)]
        self.assertEqual(mismatches, [])

    def test_ce_sources_and_data_preserved(self):
        # Reviewed test hooks, build/workflows and the fork changelog adapt;
        # all other original CE blobs retain exact identity.
        exceptions = NATIVE_CLI_ADAPTATIONS | {'run-build-release-cleanenv.ps1', 'FORK-CHANGES.md',
                      '.github/workflows/windows-release.yml',
                      '.github/workflows/windows-layout-test.yml', 'run-build-release-trace-cleanenv.ps1'}
        mismatches = [p for p in self.custom
                      if p not in exceptions and p not in CONSOLIDATION_EDITORIAL and not p.startswith('doc/integration-2026.4.0/')
                      and self.strict.get(p, self.portable.get(p) if p in self.portable_adaptations else self.ce.get(p)) != self.integrated.get(p)]
        self.assertEqual(mismatches, [])

    def test_reviewed_portable_runtime_adaptations_are_exact(self):
        # Accepted adaptations are pinned to an immutable milestone, not merely
        # exempted from preservation checks. New consolidation runtime edits fail.
        runtime = [p for p in self.portable_adaptations if p.endswith(('.cpp', '.h', '.manifest'))]
        self.assertTrue(runtime)
        self.assertEqual([p for p in runtime if self.integrated.get(p) != self.strict.get(p, self.portable.get(p))], [])

    def test_every_default_engine_is_enabled_in_release(self):
        props = (ROOT / 'build-scummvm/ScummVM_Globalx64.props').read_text()
        expected = set()
        for path in (ROOT / 'engines').glob('*/configure.engine'):
            for line in path.read_text().splitlines():
                match = re.match(r'add_engine\s+(\w+)\s+"[^"]+"\s+yes\b', line)
                if match:
                    expected.add(match[1].upper())
        actual = set(re.findall(r'\bENABLE_(\w+)\b', props))
        self.assertFalse(expected - actual, sorted(expected - actual))
        self.assertTrue({'NOX', 'MONKEY4', 'PHOENIXVR', 'HPL1', 'DM', 'AVALANCHE',
                         'CHAMBER', 'FOOL', 'EEM', 'HARVESTER'} <= actual)

    def test_ce_and_release_engines_have_real_compile_inputs(self):
        for name in ('nox', 'grim', 'sci', 'cine', 'macventure', 'phoenixvr',
                     'hpl1', 'dm', 'avalanche', 'chamber', 'fool', 'eem', 'harvester'):
            with self.subTest(engine=name):
                project = (ROOT / f'build-scummvm/{name}.vcxproj').read_text()
                self.assertIn('ClCompile Include=', project)
                self.assertIn(f'engines\\{name}\\', project)
        self.assertIn('engines\\ce_achievements.cpp',
                      (ROOT / 'build-scummvm/scummvm.vcxproj').read_text())
        self.assertIn('engines\\ce_test_cli.cpp',
                      (ROOT / 'build-scummvm/scummvm.vcxproj').read_text())
        self.assertIn('engines\\grim\\ce_test_cli.cpp',
                      (ROOT / 'build-scummvm/grim.vcxproj').read_text())

    def test_consolidated_unicode_build_changes_are_exact(self):
        # This build-policy change is checked as an exact transformation, not
        # silently exempted from the earlier CE preservation contract.
        name = 'run-build-release-trace-cleanenv.ps1'
        before = git('show', PORTABLE + ':' + name)
        expected = before.replace('--msvc-version 17', '--msvc-version 17 --use-windows-unicode')
        self.assertEqual(git('show', 'HEAD:' + name), expected)
        for name in ('.github/workflows/windows-release.yml', '.github/workflows/windows-layout-test.yml'):
            before = git('show', PORTABLE + ':' + name)
            expected = before.replace('create_project.exe .. --msvc --vcpkg --enable-gif',
                                      'create_project.exe .. --msvc --vcpkg --use-windows-unicode --enable-gif')
            anchor = '      - name: Create release archive\n' if 'windows-release' in name else '      - name: Upload Windows test package\n'
            step = ('      - name: Verify permanent portable storage boundaries\n'
                    '        run: |\n'
                    '          python -B devtools/ce/test_strict_storage.py --exe package/scummvm-comfort-edition-windows-x64/scummvm.exe --artifacts portable-storage-checks\n'
                    '          if ($LASTEXITCODE) { exit $LASTEXITCODE }\n')
            expected = expected.replace(anchor, step+anchor)
            self.assertEqual(git('show', 'HEAD:' + name), expected)
        self.assertEqual(git('show', 'HEAD:run-build-release-cleanenv.ps1'),
                         git('show', PORTABLE + ':run-build-release-cleanenv.ps1'))

    def test_windows_packaging_retains_engine_coverage(self):
        for name in ('windows-release.yml', 'windows-layout-test.yml'):
            workflow = (ROOT / '.github/workflows' / name).read_text()
            self.assertIn('--enable-gif --enable-engine=monkey4,nox', workflow)


if __name__ == '__main__':
    unittest.main(verbosity=2)
