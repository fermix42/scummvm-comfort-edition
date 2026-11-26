# SPDX-License-Identifier: GPL-3.0-or-later
"""Boundary and inventory regressions; no game files or engine execution."""
from pathlib import Path
import struct
import tempfile
import unittest

from inspect_mohawk_v5_reference import inspect
from inventory_mohawk_v5 import command_table, lexical_inventory, resources
from test_mohawk_v5_startup import archive


def reference():
    tokens = {f'0x{x:02x}': {'operand': 'none'} for x in range(256)}
    for x in (1, 6, 0x31):
        tokens[f'0x{x:02x}']['operand'] = 'word'
    for x in (0x4d, 0x4e, 0x4f, 0x70, 0x71):
        tokens[f'0x{x:02x}']['operand'] = 'command_word'
    tokens['0x05']['operand'] = 'literal'
    return {'tokens': tokens, 'real_literal': {'divisor': 10000, 'bytes': 4}}


def bcod(code, strings=b''):
    return struct.pack('>II', 8 + len(code) + len(strings), len(code)) + code + strings


class Inventory(unittest.TestCase):
    def test_original_fixed_real_does_not_swallow_following_expression(self):
        # The provisional VM consumed eight bytes and lost this next root.
        code = b'\0\x05\x04\0\0\x19\x64\x08\x04\x08'
        result, boundaries = lexical_inventory(bcod(code), reference())
        self.assertEqual(result['decoded_bytes'], len(code))
        self.assertIn(8, boundaries)
        self.assertEqual(result['tokens']['0x04'], 1)
        self.assertEqual(result['errors'], [])

    def test_truncated_literal_is_explicit(self):
        result, _ = lexical_inventory(bcod(b'\x05\x04\0\0'), reference())
        self.assertEqual(len(result['errors']), 1)

    def test_component_receiver_survives_index_argument(self):
        strings = b'\0\0file\0\0\0values\0'
        code = b'\x01\0\0\x05\x03\0\0\0\x07\x01\0\x07\x05\x03\0\0\0\x01\x1b\x4d\x58\x03\x08'
        result, _ = lexical_inventory(bcod(code, strings), reference())
        args = result['components'][0]['args']
        self.assertEqual(args[0], {'ref': 'file'})
        self.assertEqual(args[1], {'value': 7})
        self.assertEqual(args[2], {'ref': 'values[1]'})

    def test_dynamic_object_call_keeps_named_selector(self):
        strings = b'\0\0object\0\0\0open\0'
        code = b'\x01\0\0\x77\x06\0\x09\x07\x10\x07\x10\x08'
        result, _ = lexical_inventory(bcod(code, strings), reference())
        self.assertEqual(result['components'][0]['receiver'], {'ref': 'object'})
        self.assertEqual(result['components'][0]['args'], [{'value': 'open'}])
        self.assertEqual(result['errors'], [])

    def test_bad_sound_resource_does_not_hide_other_resources(self):
        data = bytearray(archive([('BCOD', 900, b'valid'), ('tWAV', 900, b'x')]))
        base = struct.unpack_from('>I', data, 20)[0]
        table = base + struct.unpack_from('>H', data, 24)[0]
        struct.pack_into('>H', data, table + 4 + 10 + 4, 100)
        with self.assertRaises(ValueError):
            list(resources(data))
        issues = []
        rows = list(resources(data, issues))
        self.assertEqual(rows, [('BCOD', 900, b'valid')])
        self.assertEqual(issues[0]['tag'], 'tWAV')

    def test_ce_command_holes_preserve_numbering(self):
        self.assertEqual(command_table()['0x4d:0x22']['name'], 'scriptLink')
        self.assertEqual(command_table()['0x4d:0x5e']['name'], 'setPageFade')

    def test_unknown_original_executable_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'foreign.exe'
            path.write_bytes(b'MZ')
            with self.assertRaisesRegex(ValueError, 'identity'):
                inspect(path)


if __name__ == '__main__':
    unittest.main()
