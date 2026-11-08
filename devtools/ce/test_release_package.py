#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Deterministic archive and non-overwrite release guarantees."""
import hashlib
import json
import os
from pathlib import Path
import tempfile
import unittest
import zipfile
from prepare_ce_release import archive, ROOT


class ReleaseTests(unittest.TestCase):
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

    def test_publishing_cannot_clobber_existing_assets(self):
        text=(ROOT/'.github/workflows/windows-release.yml').read_text()
        self.assertNotIn('--clobber',text)
        self.assertNotIn('gh release edit',text)
        self.assertIn('--verify-tag',text)
        self.assertIn('Release already exists',text)


if __name__ == '__main__': unittest.main(verbosity=2)
