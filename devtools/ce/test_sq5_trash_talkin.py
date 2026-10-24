#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Deterministic importer boundary tests; no game data required."""
import base64
import hashlib
from pathlib import Path
import tempfile
import unittest

from prepare_sq5_trash_talkin import apply_recipe, safe_child, prepare
from sq5_tte_ending_art import coff_assets


class PreparationTests(unittest.TestCase):
    def test_ending_recovery_rejects_unrecognized_runtime(self):
        with self.assertRaisesRegex(ValueError, 'Runtime hash differs'):
            coff_assets(b'not the reviewed executable')

    def recipe(self, ops, output=b'abXYef'):
        return {'base_sha256': hashlib.sha256(b'abcdef').hexdigest(),
                'output_sha256': hashlib.sha256(output).hexdigest(), 'ops': ops}

    def test_hash_bound_patch(self):
        recipe = self.recipe([['copy', 0, 2], ['insert', base64.b64encode(b'XY').decode()], ['copy', 4, 2]])
        self.assertEqual(apply_recipe(b'abcdef', recipe), b'abXYef')
        with self.assertRaisesRegex(ValueError, 'base hash'):
            apply_recipe(b'ABCDEF', recipe)
        recipe['output_sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'output hash'):
            apply_recipe(b'abcdef', recipe)

    def test_invalid_ranges_and_operations(self):
        for op in (['copy', -1, 2], ['copy', 1, -1], ['copy', 0, 7], ['execute', 'bad']):
            with self.subTest(op=op), self.assertRaises(ValueError):
                apply_recipe(b'abcdef', self.recipe([op]))

    def test_payload_cannot_escape_root(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            self.assertEqual(safe_child(root, 'voices/test.wav'), root / 'voices/test.wav')
            for name in ('../outside', str(root.parent), '.'):
                with self.subTest(name=name), self.assertRaises(ValueError):
                    safe_child(root, name)

    def test_sources_and_existing_output_are_never_modified(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            release, original = root / 'release', root / 'original'
            release.mkdir()
            original.mkdir()
            sentinel = original / 'sentinel'
            sentinel.write_bytes(b'original')
            for output in (original, original / 'child', root, release / 'child'):
                with self.subTest(output=output), self.assertRaisesRegex(ValueError, 'separate'):
                    prepare(release, original, output)
            existing = root / 'existing'
            existing.mkdir()
            with self.assertRaisesRegex(ValueError, 'already exists'):
                prepare(release, original, existing)
            self.assertEqual(sentinel.read_bytes(), b'original')


if __name__ == '__main__':
    unittest.main()
