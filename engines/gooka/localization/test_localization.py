#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Focused real-data localization checks; original files are read only."""
import argparse
import functools
import hashlib
import json
from pathlib import Path
import re
from catalog import inventory, resources
from compile import ROOT, controls, header

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--game', type=Path, required=True)
args = parser.parse_args()
original = inventory(args.game, ROOT.parent)
english = json.loads((ROOT/'english.json').read_text(encoding='utf-8'))
assert [e['id'] for e in original] == [e['id'] for e in english]
assert (ROOT/'english.h').read_text(encoding='utf-8') == header(english)
widths = []
for i in range(12):
    for key, raw in resources(args.game/f'{i}.BIN'):
        if key[:2] == [32753,403]:
            for c in range(256):
                rows = raw[c*16:c*16+16]
                mask = rows[15] if rows[15]&128 else functools.reduce(int.__or__,rows,0)
                bit = next((b for b in range(8) if mask&(1<<b)),8)
                widths.append(max(9-bit,1))
assert len(widths) == 256
for a,b in zip(original,english):
    assert a['source_sha256'] == b['source_sha256'],a['id']
    assert controls(a['czech']) == controls(b['english']),a['id']
    assert b['english'].encode('cp1250').decode('cp1250') == b['english']
    assert not a['czech'].strip() or b['english'].strip(),a['id']
    if 3202 <= a['id'] <= 3221:
        for line in b['english'].splitlines():
            if not line.startswith('@'):
                assert sum(widths[c] for c in line.encode('cp1250')) <= 250,(a['id'],line)
assert 'ADGF_UNSTABLE | ADGF_ADDENGLISH' in (ROOT.parent/'detection_tables.h').read_text()
assert 'Common::parseLanguage(ConfMan.get("language"))' in (ROOT.parent/'resource.cpp').read_text()
print(json.dumps(dict(status='PASS',ids=len(original),nonempty=sum(bool(e['czech']) for e in original),
                     english_mapped=len(english),needs_context_review=[e['id'] for e in english if e['status']=='needs-context-review'],
                     controls_preserved=True,cp1250_roundtrip=True,book_columns_fit=True),indent=2))
