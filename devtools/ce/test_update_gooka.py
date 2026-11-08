#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline safety and project-generation checks for the Gooka importer."""
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import update_gooka as updater


class ImportTests(unittest.TestCase):
    def test_snapshot(self):
        import json
        updater.verify(json.loads(updater.MANIFEST.read_text()))

    def test_local_changes_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            engine = root / 'engines/gooka'
            engine.mkdir(parents=True)
            source = engine / 'gooka.cpp'
            source.write_bytes(b'original')
            manifest = {'files': {'engines/gooka/gooka.cpp': updater.digest(b'original')}}
            with patch.object(updater, 'ROOT', root):
                updater.verify(manifest)
                source.write_bytes(b'local edits')
                with self.assertRaises(SystemExit):
                    updater.verify(manifest)
                source.write_bytes(b'original')
                (engine / 'local.cpp').write_bytes(b'extra')
                with self.assertRaises(SystemExit):
                    updater.verify(manifest)

    def test_all_room_sources_registered(self):
        import re
        engine = updater.ROOT / 'engines/gooka'
        module = (engine / 'module.mk').read_text()
        objects = re.findall(r'\brooms/([\w]+)\.o\b', module)
        sources = {path.stem for path in (engine / 'rooms').glob('*.cpp')}
        self.assertEqual(set(objects), sources)
        self.assertEqual(len(objects), len(set(objects)))

    def test_adaptation_is_idempotent(self):
        engine = updater.ROOT / 'engines/gooka'
        files = {path.relative_to(updater.ROOT).as_posix(): path.read_bytes()
                 for path in engine.rglob('*') if path.is_file()}
        original = dict(files)
        updater.adapt_module(files)
        self.assertEqual(files, original)


if __name__ == '__main__':
    unittest.main()
