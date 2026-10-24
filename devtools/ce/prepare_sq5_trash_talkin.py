#!/usr/bin/env python3
"""Prepare owned SQ5 data for CE without changing the supplied release or game.

No game data is distributed by this tool. Run with --help for local paths.
The supplied release's hash-bound recipes and decoder are used for SCI patches.
"""
import argparse
import base64
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil

from sq5_tte_ending_art import coff_assets


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def safe_child(root, name):
    path = (root / name).resolve()
    if not path.is_relative_to(root.resolve()) or path == root.resolve():
        raise ValueError(f'Path escapes its root: {name}')
    return path


def apply_recipe(data, recipe):
    if hashlib.sha256(data).hexdigest() != recipe['base_sha256']:
        raise ValueError('Patch base hash mismatch')
    result = bytearray()
    for op in recipe['ops']:
        if op[0] == 'copy':
            if not 0 <= op[1] <= op[1] + op[2] <= len(data):
                raise ValueError('Patch copy outside base')
            result.extend(data[op[1]:op[1] + op[2]])
        elif op[0] == 'insert':
            result.extend(base64.b64decode(op[1], validate=True))
        else:
            raise ValueError('Unknown patch operation')
    if hashlib.sha256(result).hexdigest() != recipe['output_sha256']:
        raise ValueError('Patch output hash mismatch')
    return bytes(result)


def artwork(patch):
    # Read decimal C byte arrays from the supplied GPL source patch. Do not
    # evaluate C/Python expressions or put the resulting artwork in Git.
    arrays = {}
    source = '\n'.join(line[1:] for line in patch.splitlines()
                       if line.startswith('+') and not line.startswith('+++'))
    for name, body in re.findall(r'static const byte (\w+)\[\] = \{(.*?)\};', source, re.S):
        if not (name.startswith('galaxy') or name in ('goliath1', 'goliath2')):
            continue
        if not re.fullmatch(r'[\d,\s]*', body):
            raise ValueError(f'Non-decimal artwork array: {name}')
        arrays[name] = bytes(int(value) for value in re.findall(r'\d+', body))
    expected = {'galaxyArtPalette': 192, 'galaxyArtPixels': 60800,
                'galaxySpritePalette': 192, 'goliath1': 1296, 'goliath2': 1296}
    expected.update({f'galaxySprite{i}': 3108 for i in range(13)})
    for name, size in expected.items():
        if len(arrays.get(name, b'')) != size:
            raise ValueError(f'Missing or unexpected artwork: {name}')
    return {name: arrays[name] for name in expected}


def audit(release):
    manifest = json.loads((release / 'payload/manifest.json').read_text())
    if manifest['version'] != '0.2.12':
        raise ValueError('Only release 0.2.12 has been reviewed')
    for name, expected in manifest['payload'].items():
        path = safe_child(release / 'payload', name)
        if sha(path) != expected:
            raise ValueError(f'Release payload hash mismatch: {name}')
    report = json.loads((release / 'BUILD-REPORT.json').read_text())
    if sha(release / 'payload/runtime/scummvm.exe') != report['runtime_sha256']:
        raise ValueError('Runtime does not match build report')
    info = json.loads((release / 'payload/source/BUILD-INFO.json').read_text())
    if sha(release / 'payload/source/scummvm-base.tar.gz') != info['source_sha256']:
        raise ValueError('Base source does not match build info')
    return manifest


def prepare(release, original, output):
    release, original, output = [p.resolve() for p in (release, original, output)]
    for source in (release, original):
        if output == source or output.is_relative_to(source) or source.is_relative_to(output):
            raise ValueError('Output must be separate from release and original')
    if output.exists():
        raise ValueError('Output already exists; choose a new directory')
    manifest = audit(release)
    found = {p.name.upper(): p for p in original.iterdir() if p.is_file()}
    for name, expected in manifest['originals'].items():
        if name not in found or sha(found[name]) != expected:
            raise ValueError(f'Missing or unsupported original: {name}')
    arrays = artwork((release / 'payload/source/sq5-voices.patch').read_text(encoding='utf8'))
    ending_arrays, ending_provenance = coff_assets((release / 'payload/runtime/scummvm.exe').read_bytes())
    arrays.update(ending_arrays)
    # These top-level decoder files are outside the payload manifest. Bind
    # them to the reviewed 0.2.12 files before executing the DCL decoder.
    decoders = {
        'sci_messages.py': 'e39ad6722439cb8e5b4c6d425f7d644e031c70970e5fc80dc0666ceea7cb5a03',
        'dcl_tables.py': '8ce9cc07464bf6d6d9a295c16f05e6f3972dd4feb91ff5d2d717e82d37c6a020',
    }
    for name, expected in decoders.items():
        if sha(release / name) != expected:
            raise ValueError(f'Decoder differs from reviewed source: {name}')
    import sys
    sys.path.insert(0, str(release))
    spec = importlib.util.spec_from_file_location('sq5_decoder', release / 'sci_messages.py')
    decoder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(decoder)
    patched = {}
    for recipe in manifest['patches']:
        base = found[recipe['base_file']].read_bytes()
        if 'slice' in recipe:
            start, end = recipe['slice']
            if not 0 <= start <= end <= len(base):
                raise ValueError('Patch slice outside original')
            base = base[start:end]
        if 'dcl_size' in recipe:
            base = decoder.dcl(base, recipe['dcl_size'])
        safe_child(output, recipe['output'])
        patched[recipe['output']] = apply_recipe(base, recipe)
    # All hashes and recipes pass before creating the output. On I/O failure,
    # leave the partial directory for inspection; never delete user paths.
    output.mkdir(parents=True)
    for name in manifest['originals']:
        shutil.copy2(found[name], output / name)
    for name, data in patched.items():
        (output / name).write_bytes(data)
    for name in manifest['payload']:
        if name.startswith('voices/'):
            target = safe_child(output, name)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(release / 'payload' / name, target)
    for name, data in arrays.items():
        (output / 'voices' / (name + '.bin')).write_bytes(data)
    shutil.copy2(release / 'release.json', output / 'sq5-tte-release.json')
    evidence = {'version': manifest['version'], 'original': str(original),
                'patches': {name: hashlib.sha256(data).hexdigest() for name, data in patched.items()},
                'ending_art': ending_provenance,
                'source_note': 'Missing ending headers recovered byte-for-byte from named COFF symbols in the hash-bound supplied runtime; pixels remain local.'}
    (output / 'ce-preparation.json').write_text(json.dumps(evidence, indent=2) + '\n')
    return evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release', required=True, type=Path)
    parser.add_argument('--original', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--audit-only', action='store_true')
    args = parser.parse_args()
    if args.audit_only:
        manifest = audit(args.release.resolve())
        arrays = artwork((args.release / 'payload/source/sq5-voices.patch').read_text(encoding='utf8'))
        ending_arrays, _ = coff_assets((args.release / 'payload/runtime/scummvm.exe').read_bytes())
        print(json.dumps({'version': manifest['version'], 'payload_files_verified': len(manifest['payload']),
                          'patches': len(manifest['patches']), 'external_art_arrays': len(arrays),
                          'recovered_ending_arrays': len(ending_arrays)}))
    else:
        if not args.original or not args.output:
            parser.error('--original and --output are required unless --audit-only')
        print(json.dumps(prepare(args.release, args.original, args.output), indent=2))


if __name__ == '__main__':
    main()
