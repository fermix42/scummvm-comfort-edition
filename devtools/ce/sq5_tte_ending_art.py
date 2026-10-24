#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Recover omitted ending artwork from the exact supplied 0.2.12 runtime.

Uses named COFF symbols, not heuristic pattern matches or executable code.
Pixel bytes are local game assets and must not be committed to the CE repo.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

RUNTIME_SHA256 = '69fd17cf3ea43ea0166826dc715487eff3043bef1ff462a6010160587766aab3'
ASSETS = {
    'kFloEndingMouth': ('_ZL15kFloEndingMouth', 450),
    'kEndingFloEyes0': ('_ZL15kEndingFloEyes0', 300),
    'kEndingFloEyes1': ('_ZL15kEndingFloEyes1', 300),
    'kEndingDrooleEyes0': ('_ZL18kEndingDrooleEyes0', 1200),
    'kEndingDrooleEyes1': ('_ZL18kEndingDrooleEyes1', 1200),
}


def coff_assets(data):
    if hashlib.sha256(data).hexdigest() != RUNTIME_SHA256:
        raise ValueError('Runtime hash differs from the reviewed 0.2.12 executable')
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[:2] != b'MZ' or data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('Not a PE executable')
    machine, count_sections, _, symbols, count_symbols, optional_size, _ = struct.unpack_from('<HHIIIHH', data, pe + 4)
    if machine != 0x8664:
        raise ValueError('Expected x64 executable')
    section_table = pe + 24 + optional_size
    sections = []
    for i in range(count_sections):
        offset = section_table + i * 40
        name = data[offset:offset + 8].rstrip(b'\0').decode('ascii')
        raw_size, raw_offset = struct.unpack_from('<II', data, offset + 16)
        flags = struct.unpack_from('<I', data, offset + 36)[0]
        if raw_offset + raw_size > len(data):
            raise ValueError('Section exceeds executable')
        sections.append((name, raw_offset, raw_size, flags))
    strings = symbols + count_symbols * 18
    string_size = struct.unpack_from('<I', data, strings)[0]
    if string_size < 4 or strings + string_size > len(data):
        raise ValueError('Invalid COFF string table')
    wanted = {symbol: (name, size) for name, (symbol, size) in ASSETS.items()}
    found, provenance = {}, {}
    index = 0
    while index < count_symbols:
        offset = symbols + index * 18
        raw_name = data[offset:offset + 8]
        if raw_name[:4] == b'\0' * 4:
            start = struct.unpack_from('<I', raw_name, 4)[0]
            if not 4 <= start < string_size:
                raise ValueError('Invalid COFF symbol name')
            end = data.index(b'\0', strings + start, strings + string_size)
            name = data[strings + start:end].decode('ascii')
        else:
            name = raw_name.rstrip(b'\0').decode('ascii')
        value, section, _, _, auxiliary = struct.unpack_from('<IhHBB', data, offset + 8)
        if name in wanted:
            asset, size = wanted[name]
            if asset in found or not 1 <= section <= len(sections):
                raise ValueError('Ambiguous or invalid artwork symbol')
            section_name, raw_offset, raw_size, flags = sections[section - 1]
            if section_name != '.rdata' or flags & 0x80000000 or value + size > raw_size:
                raise ValueError('Artwork must lie in read-only initialized data')
            pixels = data[raw_offset + value:raw_offset + value + size]
            found[asset] = pixels
            provenance[asset] = {'symbol': name, 'section': section_name,
                                 'file_offset': raw_offset + value, 'size': size,
                                 'sha256': hashlib.sha256(pixels).hexdigest()}
        index += 1 + auxiliary
    if set(found) != set(ASSETS):
        raise ValueError('Executable does not contain all five named artwork symbols')
    return found, provenance


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--headers-output', type=Path,
                        help='Optional local directory for reconstructed missing C headers')
    args = parser.parse_args()
    assets, provenance = coff_assets(args.exe.read_bytes())
    args.output.mkdir(parents=True, exist_ok=True)
    for name, pixels in assets.items():
        target = args.output / (name + '.bin')
        if target.exists() and target.read_bytes() != pixels:
            raise ValueError(f'Refusing to overwrite different artwork: {target}')
    for name, pixels in assets.items():
        (args.output / (name + '.bin')).write_bytes(pixels)
    if args.headers_output:
        args.headers_output.mkdir(parents=True, exist_ok=True)
        comment = '// Reconstructed from named symbols in the hash-bound 0.2.12 runtime. Local game artwork.\n'
        frames = assets['kFloEndingMouth']
        mouth = comment + 'static const byte kFloEndingMouth[3][150] = {\n'
        for frame in range(3):
            mouth += '{' + ','.join(map(str, frames[frame * 150:(frame + 1) * 150])) + '},\n'
        mouth += '};\n'
        eyes = comment
        for name in ('kEndingFloEyes0', 'kEndingFloEyes1', 'kEndingDrooleEyes0', 'kEndingDrooleEyes1'):
            eyes += 'static const byte ' + name + '[] = {' + ','.join(map(str, assets[name])) + '};\n'
        for name, source in {'flo_ending_mouth.h': mouth, 'ending_blinks.h': eyes}.items():
            target = args.headers_output / name
            if target.exists() and target.read_text() != source:
                raise ValueError(f'Refusing to overwrite different header: {target}')
            target.write_text(source)
    report = {'runtime_sha256': RUNTIME_SHA256, 'assets': provenance,
              'total_bytes': sum(map(len, assets.values()))}
    (args.output / 'ending-art-provenance.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
