# SPDX-License-Identifier: GPL-3.0-or-later
"""Extract ThinkQuick token dispatch metadata from the identified original PE32.

No game executable or script is emitted. Addresses apply only to the pinned
SHA-256. Decoder rules follow 0x4018ab; execution handlers follow 0x403ac0.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

REFERENCE_SHA256 = '219d81ea3d91b31da406e6891d01ba6ad12005e77b4d9f8f83930161dbfc9966'


def inspect(path):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != REFERENCE_SHA256:
        raise ValueError('Unsupported original executable identity; do not reuse address tables')
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    count, optional_size = struct.unpack_from('<H12xH', data, pe + 6)
    optional = pe + 24
    if struct.unpack_from('<H', data, optional)[0] != 0x10b:
        raise ValueError('Expected PE32')
    base = struct.unpack_from('<I', data, optional + 28)[0]
    sections = []
    for i in range(count):
        virtual_size, rva, size, offset = struct.unpack_from('<4I', data, optional + optional_size + 40 * i + 8)
        sections.append((rva, size, offset))

    def read(address, size):
        rva = address - base
        for start, length, offset in sections:
            if start <= rva and rva + size <= start + length:
                return data[offset + rva - start:offset + rva - start + size]
        raise ValueError(f'Address outside raw section: {address:x}')

    def u32(address):
        return struct.unpack('<I', read(address, 4))[0]

    def named_table(address):
        rows = []
        while u32(address):
            ptr = u32(address)
            text = bytearray()
            while read(ptr, 1) != b'\0':
                text.extend(read(ptr, 1))
                ptr += 1
            rows.append({'name': text.decode('ascii'), 'id': struct.unpack('<i', read(address + 4, 4))[0]})
            address += 8
        return rows

    result = {'schema': 1, 'exe_sha256': REFERENCE_SHA256,
              'reader': '0x4018ab', 'reader_wrapper': '0x406660',
              'evaluator': '0x403ac0',
              'real_literal': {'bytes': 4, 'encoding': 'unsigned big-endian fixed point',
                               'divisor': struct.unpack('<d', read(0x4885e0, 8))[0],
                               'reader': '0x4022f0', 'divisor_address': '0x4885e0'}, 'tokens': {}}
    result['component_factory'] = {'entry': '0x448ad6', 'name_table': '0x4985a0',
                                   'name_comparison': 'case-insensitive (call 0x479560)',
                                   'classes': named_table(0x4985a0)}
    result['component_common_methods'] = {'address': '0x4932b0', 'methods': named_table(0x4932b0)}
    result['component_method_tables'] = {}
    # Table contents and addresses are direct observations. Class/table
    # affiliation is an initial interface map, not a recovered method contract.
    for name, address in {'aaDatabase': 0x496c70, 'aaDirectory': 0x497158,
                          'aaQSelector': 0x497350, 'aaUserHistory': 0x497c80,
                          'aaStringLib': 0x497ab8, 'datafile': 0x497f28,
                          'encryptor': 0x498260, 'font': 0x4984a0,
                          'clock': 0x4987f0}.items():
        result['component_method_tables'][name] = {'address': f'0x{address:06x}', 'methods': named_table(address),
                                                  'affiliation': 'initial map; method semantics unqualified'}
    result['general_command_handlers'] = {
        f'0x{command:02x}': f'0x{u32(0x40db44 + 4 * read(0x40dca8 + command - 1, 1)[0]):06x}'
        for command in range(1, 130)}
    for token in range(256):
        lexical = u32(0x401a62 + 4 * read(0x401a76 + token - 1, 1)[0]) if 1 <= token <= 0x71 else 0x401a1f
        handler = u32(0x405137 + 4 * read(0x4051b3 + token - 1, 1)[0]) if 1 <= token <= 0x77 else 0x405109
        recipe = {0x4019e1: 'word', 0x401905: 'word', 0x401927: 'literal',
                  0x401a00: 'command_word', 0x401a1f: 'none'}[lexical]
        result['tokens'][f'0x{token:02x}'] = {'operand': recipe,
                                            'reader_handler': f'0x{lexical:06x}',
                                            'execution_handler': f'0x{handler:06x}',
                                            'default_execution': handler == 0x405109}
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = inspect(args.exe)
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2)
        stream.write('\n')
