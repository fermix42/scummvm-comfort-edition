#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Deterministic archive and non-overwrite release guarantees."""
import hashlib
import json
import os
import re
from pathlib import Path
import tempfile
import unittest
import zipfile
from prepare_ce_release import archive, ROOT, verify_feature_data
import importlib.util
import shutil


class ReleaseTests(unittest.TestCase):
    def test_custom_engine_data_is_packaged_beside_kyra(self):
        spec=importlib.util.spec_from_file_location('layout_packager',ROOT/'devtools/create_project/scripts/package-window-layout.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        required={'kyra.dat','gooka.dat','snatcher-decensor.dat'}
        paths={p.name:p for p in module.distributed_engine_data(ROOT)}
        self.assertTrue(required <= paths.keys())
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp)/'package';folder.mkdir()
            for name in required:shutil.copy2(paths[name],folder/name)
            verify_feature_data(folder)
            archive(folder,Path(temp)/'engine-data.zip','CE')
            with zipfile.ZipFile(Path(temp)/'engine-data.zip') as z:
                self.assertEqual(set(z.namelist()),{'CE/'+name for name in required})
                for name in required:self.assertEqual(z.read('CE/'+name),paths[name].read_bytes())
            (folder/'gooka.dat').write_bytes(b'stale')
            with self.assertRaisesRegex(SystemExit,'gooka.dat'):verify_feature_data(folder)
            (folder/'snatcher-decensor.dat').unlink()
            shutil.copy2(paths['gooka.dat'],folder/'gooka.dat')
            with self.assertRaisesRegex(SystemExit,'snatcher-decensor.dat'):verify_feature_data(folder)

    def test_archive_is_independent_of_input_mtimes(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);folder=root/'package';folder.mkdir()
            (folder/'b.dat').write_bytes(b'resource');(folder/'a.exe').write_bytes(b'executable')
            archive(folder,root/'first.zip','CE')
            for p in folder.iterdir(): os.utime(p,(1234567890,1234567890))
            archive(folder,root/'second.zip','CE')
            self.assertEqual(hashlib.sha256((root/'first.zip').read_bytes()).digest(),hashlib.sha256((root/'second.zip').read_bytes()).digest())
            with zipfile.ZipFile(root/'first.zip') as zipped:
                self.assertEqual(zipped.namelist(),['CE/a.exe','CE/b.dat'])
                self.assertIsNone(zipped.testzip())

    def test_existing_archive_is_never_overwritten(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);folder=root/'package';folder.mkdir()
            target=root/'existing.zip';target.write_bytes(b'preserve existing asset')
            with self.assertRaises(FileExistsError): archive(folder,target,'CE')
            self.assertEqual(target.read_bytes(),b'preserve existing asset')

    def test_ce_version_is_separate_from_upstream(self):
        release=json.loads((ROOT/'CE-RELEASE.json').read_text())
        self.assertEqual(release['proposed_tag'],'v'+release['version'])
        self.assertNotEqual(release['version'],release['upstream_version'])
        self.assertTrue((ROOT/release['notes']).is_file())
        header=(ROOT/'base/ce_version.h').read_text()
        self.assertEqual(re.search(r'#define SCUMMVM_CE_VERSION "([^"]+)"',header)[1],release['version'])
        upstream=(ROOT/'base/internal_version.h').read_text()
        self.assertEqual(re.search(r'#define SCUMMVM_VERSION "([^"]+)"',upstream)[1],release['upstream_version'])

    def test_publishing_cannot_clobber_existing_assets(self):
        text=(ROOT/'devtools/ce/publish-approved-release.ps1').read_text()
        self.assertNotIn('--clobber',text)
        self.assertIn('--verify-tag',text)
        self.assertIn('refusing to edit',text)
        self.assertIn('disabled_manually',text)
        self.assertIn('if (-not $Publish)',text)
        workflow=(ROOT/'.github/workflows/windows-candidate.yml').read_text()
        self.assertIn('workflow_dispatch:',workflow)
        self.assertNotIn('  push:',workflow)
        self.assertNotIn('release create',workflow)
        retired=(ROOT/'.github/workflows/windows-release.yml').read_text()
        self.assertNotIn('  push:',retired)
        self.assertNotIn('release create',retired)


if __name__ == '__main__': unittest.main(verbosity=2)
