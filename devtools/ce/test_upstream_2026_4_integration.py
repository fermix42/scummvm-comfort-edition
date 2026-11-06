#!/usr/bin/env python3
"""Audit both integration parents and the generated Release x64 engine coverage.

Run from any directory after generating build-scummvm. These checks compare Git
blobs, not working-copy newlines. They supplement, not replace, runtime testing.
"""
from pathlib import Path
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
UPSTREAM = '7d2709da5136866589f7602f5f5a7b17de03420e'
CE = '29e2ceb01611ac18654d82bfe2116db8183bd53d'
BASE = 'b72dad4a9c8e91139935cb83d234722199bc25c1'


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
                       'test/system/null_osystem.cpp', 'test/system/null_osystem.h'}
        mismatches = [p for p, blob in self.upstream.items()
                      if p not in self.custom and p not in adaptations
                      and self.integrated.get(p) != blob]
        self.assertEqual(mismatches, [])

    def test_ce_sources_and_data_preserved(self):
        # Only the build script and this integration's own documentation adapt.
        exceptions = {'run-build-release-cleanenv.ps1',
                      '.github/workflows/windows-release.yml',
                      '.github/workflows/windows-layout-test.yml'}
        mismatches = [p for p in self.custom
                      if p not in exceptions and not p.startswith('doc/integration-2026.4.0/')
                      and self.ce.get(p) != self.integrated.get(p)]
        self.assertEqual(mismatches, [])

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

    def test_windows_packaging_retains_engine_coverage(self):
        for name in ('windows-release.yml', 'windows-layout-test.yml'):
            workflow = (ROOT / '.github/workflows' / name).read_text()
            self.assertIn('--enable-gif --enable-engine=monkey4,nox', workflow)


if __name__ == '__main__':
    unittest.main(verbosity=2)
