# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare identified observation receipts; never substitutes static expectations."""
import argparse
import json
from pathlib import Path


def compare(reference, candidate):
    def index(receipt):
        result = {}
        for row in receipt['results']:
            name = row['name']
            if name in result: raise ValueError('Duplicate observation: ' + name)
            result[name] = row
        if not result: raise ValueError('Empty observation set')
        return result
    left, right = index(reference), index(candidate)
    rows = []
    for name in sorted(left.keys() | right.keys()):
        a, b = left.get(name), right.get(name)
        if a is None or b is None: status = 'missing'
        elif a.get('expression_sha256') != b.get('expression_sha256'): status = 'incomparable'
        elif a.get('program_sha256') != b.get('program_sha256') or a.get('observation_kind') != b.get('observation_kind'): status = 'incomparable'
        elif (a.get('vm_errors') or b.get('vm_errors')) and a.get('observation_kind') != 'error-completion': status = 'unqualified'
        elif a.get('status') != 'observed' or b.get('status') != 'observed': status = 'unqualified'
        elif 'actual' not in a or 'actual' not in b: status = 'unqualified'
        elif a['actual'] != b['actual']: status = 'mismatch'
        else: status = 'match'
        rows.append(dict(name=name, status=status, reference=a, candidate=b))
    return dict(schema=1, kind='observation comparison only; scope follows input receipts',
                reference_identity={k:v for k,v in reference.items() if k!='results'},
                candidate_identity={k:v for k,v in candidate.items() if k!='results'},
                passed=all(r['status']=='match' for r in rows), results=rows)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', required=True, type=Path)
    parser.add_argument('--candidate', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    result = compare(json.loads(args.reference.read_text()), json.loads(args.candidate.read_text()))
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, indent=2); stream.write('\n')
    raise SystemExit(0 if result['passed'] else 1)
