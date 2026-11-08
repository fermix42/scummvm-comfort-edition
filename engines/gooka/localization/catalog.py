#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Extract a private UTF-8 text inventory from owned Gooka PE containers."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def resources(path):
    data = path.read_bytes()
    u16 = lambda p: struct.unpack_from('<H', data, p)[0]
    u32 = lambda p: struct.unpack_from('<I', data, p)[0]
    pe = u32(0x3c)
    if data[pe:pe+4] != b'PE\0\0':
        raise ValueError('Not a PE container')
    optional = pe + 24
    if u16(optional) != 0x10b:
        raise ValueError('Expected PE32')
    section_start = optional + u16(pe + 20)
    sections = []
    for i in range(u16(pe + 6)):
        s = section_start + i*40
        sections.append((u32(s+12), max(u32(s+8), u32(s+16)), u32(s+20)))
    def offset(rva):
        for start, size, raw in sections:
            if start <= rva < start+size:
                return raw+rva-start
        raise ValueError('Unmapped resource RVA')
    base = offset(u32(optional + 112))
    def walk(relative, ids):
        p = base+relative
        for i in range(u16(p+12)+u16(p+14)):
            name, value = struct.unpack_from('<II', data, p+16+i*8)
            if name & 0x80000000:
                continue
            if value & 0x80000000:
                yield from walk(value & 0x7fffffff, ids+[name])
            else:
                rva, size = struct.unpack_from('<II', data, base+value)
                start = offset(rva)
                if start+size > len(data):
                    raise ValueError('Truncated resource')
                yield ids+[name], data[start:start+size]
    yield from walk(0, [])


def inventory(game, source):
    records = {}
    for i in range(12):
        for key, raw in resources(game / f'{i}.BIN'):
            if key[0] != 32755:
                continue
            text = raw.split(b'\0', 1)[0].decode('cp1250')
            rid = key[1]
            entry = records.setdefault(rid, dict(id=rid, czech=text, english='',
                status='untranslated', containers=[], context=[],
                source_sha256=hashlib.sha256(raw).hexdigest()))
            if entry['czech'] != text:
                raise ValueError(f'Conflicting text variants for {rid}')
            entry['containers'].append(dict(bin=i, language=key[2]))
    for path in source.rglob('*.cpp'):
        for n, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
            for token in re.findall(r'(?<![\w])\d+(?![\w])', line.split('//')[0]):
                if int(token) in records:
                    records[int(token)]['context'].append(f'{path.relative_to(source)}:{n}: {line.strip()}')
    return sorted(records.values(), key=lambda e:e['id'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    entries = inventory(args.game, Path(__file__).resolve().parents[1])
    english = Path(__file__).with_name('english.json')
    if english.exists():
        translations = {e['id']:e for e in json.loads(english.read_text(encoding='utf-8'))}
        for entry in entries:
            translated = translations.get(entry['id'])
            if translated and translated['source_sha256'] == entry['source_sha256']:
                entry.update(english=translated['english'],status=translated['status'],note=translated['note'])
    args.output.write_text(json.dumps(entries, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(f'{len(entries)} text IDs; {sum(bool(e["czech"]) for e in entries)} nonempty')
