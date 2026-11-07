#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exact Git ancestry/runtime-tree preservation; no working-tree resets or writes outside --output."""
import argparse
import fnmatch
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
MILESTONES = {
    'official_upstream': '7d2709da5136866589f7602f5f5a7b17de03420e',
    'integration': '2d0de89e8b875852a0aca2c69a2de9b3c8b445b9',
    'native_cli': '80f136b4e8d662225d132041d134d3acf0c079b7',
    'portable_storage': 'd9868b2d7bc10be6752c6b9899f01c8aea20c3cd',
}


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args]).decode('utf-8')


def tree(ref):
    result = {}
    for entry in git('ls-tree', '-rz', '--full-tree', ref).split('\0'):
        if entry:
            metadata, name = entry.split('\t', 1)
            result[name] = metadata
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    checks = []
    for label, ref in MILESTONES.items():
        subprocess.check_call(['git', '-C', str(ROOT), 'merge-base', '--is-ancestor', ref, 'HEAD'])
        checks.append(label + ' is an ancestor')
    assert git('rev-parse', 'v2026.4.0^{}').strip() == MILESTONES['official_upstream']
    checks.append('official tag exact identity')
    baseline, current = tree(MILESTONES['portable_storage']), tree('HEAD')
    upstream = tree(MILESTONES['official_upstream'])
    assert not upstream.keys() - current.keys()
    checks.append('no official upstream paths missing')
    # Consolidation may update tools, documentation and policy, not runtime code/resources.
    runtime = {p for p in baseline if p.startswith(('engines/', 'backends/', 'common/', 'graphics/',
               'audio/', 'video/', 'image/', 'math/', 'base/', 'gui/', 'dists/', 'icons/'))}
    changed = [p for p in sorted(runtime) if baseline[p] != current.get(p)]
    assert not changed, changed
    checks.append('all inherited runtime source and resource blobs exact')
    groups = {
        'achievements_sync_sessions_ownership': ['engines/ce_achievements*', 'engines/achievements*', 'engines/engine.*'],
        'mi4_banks_and_native_adapter': ['engines/grim/*'],
        'nox_archaist': ['engines/nox/*'],
        'sq5_trash_talkin_and_sci_video': ['engines/sci/*'],
        'deja_vu_macventure': ['engines/macventure/*'],
        'operation_stealth_cine': ['engines/cine/*'],
        'indy3_boxing_scumm': ['engines/scumm/*'],
        'launcher_cheats_themes': ['gui/*'],
        'window_layouts_scaling_positioning_and_sdl': ['backends/platform/sdl/*', 'backends/graphics/sdl/*'],
        'native_test_cli': ['engines/ce_test_cli.*', 'engines/grim/ce_test_cli.cpp', 'engines/grim/ce_test_cli.h'],
        'headless_tinygl': ['graphics/tinygl/*', 'backends/graphics/tinygl/*'],
        'portable_filesystem_and_saves': ['backends/fs/*', 'backends/saves/*', 'common/config-manager.*'],
        'branding_resources_packaging_inputs': ['dists/*', 'icons/*', 'gui/themes/*'],
    }
    coverage = {}
    for label, patterns in groups.items():
        paths = sorted(p for p in runtime if any(fnmatch.fnmatchcase(p, pattern) for pattern in patterns))
        assert paths, label
        coverage[label] = {'files_exact': len(paths), 'paths': paths}
    checks.append('all requested CE feature groups have preserved source/resources')
    props = (ROOT/'build-scummvm/ScummVM_Globalx64.props').read_text()
    actual = set(re.findall(r'\bENABLE_(\w+)\b', props))
    expected = set()
    for path in (ROOT/'engines').glob('*/configure.engine'):
        for line in path.read_text(encoding='utf-8').splitlines():
            match = re.match(r'add_engine\s+(\w+)\s+"[^"]+"\s+yes\b', line)
            if match:
                expected.add(match[1].upper())
    assert not expected - actual, sorted(expected - actual)
    assert {'NOX', 'MONKEY4', 'PHOENIXVR', 'SCI', 'CINE', 'MACVENTURE', 'SCUMM'} <= actual
    checks.append('all default engines and CE additions enabled')
    report = {'status': 'PASS', 'head': git('rev-parse', 'HEAD').strip(), 'milestones': MILESTONES,
              'checks': checks, 'check_count': len(checks), 'runtime_files_exact': len(runtime),
              'runtime_differences': changed, 'feature_groups': coverage,
              'default_engine_defines': sorted(expected), 'compiled_engine_defines': sorted(actual),
              'scope': 'Source/resource preservation and build registration, not per-engine gameplay acceptance.'}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({k: report[k] for k in ('status', 'check_count', 'runtime_files_exact', 'runtime_differences')}, indent=2))


if __name__ == '__main__':
    main()
