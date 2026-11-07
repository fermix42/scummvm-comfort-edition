#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare/seal a local portable candidate using CE's existing Windows packager."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
NAME = 'scummvm-comfort-edition-windows-x64'
ZIP_NAME = 'scummvm-ce-2026.4.0-consolidated-candidate-windows-x64.zip'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def manifest(folder, source, delivery=None):
    # Refresh distribution-facing links and changelog without adding test profiles.
    shutil.copy2(ROOT/'FORK-CHANGES.md', folder/'FORK-CHANGES.md')
    readme = (ROOT/'README.md').read_text(encoding='utf-8')
    readme = readme.replace('(doc/ce-portable-storage/README.md)', '(docs/PORTABLE-STORAGE.md)').replace('(doc/ce-test-cli/README.md)', '(docs/NATIVE-TEST-CLI.md)').replace('(doc/ce-consolidated/RELEASE-READINESS.md)', '(docs/RELEASE-READINESS.md)')
    (folder/'README.md').write_text(readme, encoding='utf-8')
    forbidden = [p for p in folder.rglob('*') if p.is_file() and
                 (p.suffix.lower() in ('.gsv', '.ps2', '.hdv') or
                  p.name in ('ce-playthroughs.json', 'ce-achievements-queue.json', 'profile.ini') or
                  p.name.endswith('.ce-achievements.json'))]
    assert not forbidden, forbidden
    assert (folder/'scummvm.ini').read_text(encoding='utf-8') == '[scummvm]\nextrapath=.\n'
    files = {p.relative_to(folder).as_posix(): sha(p) for p in sorted(folder.rglob('*'))
             if p.is_file() and p.name not in ('BUILD-MANIFEST.json', 'SHA256SUMS.txt')}
    result = {'package_kind': 'CE-consolidated-portable-candidate', 'source_sha': source,
              'delivery_sha': delivery, 'architecture': 'x64', 'configuration': 'Release',
              'exe_sha256': sha(folder/'scummvm.exe'), 'files': files,
              'contains_commercial_game_data': False, 'contains_personal_saves_or_credentials': False}
    (folder/'BUILD-MANIFEST.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    checksums = dict(files, **{'BUILD-MANIFEST.json': sha(folder/'BUILD-MANIFEST.json')})
    (folder/'SHA256SUMS.txt').write_text(''.join(f'{value}  {name}\n' for name, value in sorted(checksums.items())), encoding='utf-8')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--build', type=Path)
    parser.add_argument('--vcpkg', type=Path)
    parser.add_argument('--vs', type=Path)
    parser.add_argument('--seal', action='store_true')
    parser.add_argument('--delivery-sha')
    args = parser.parse_args()
    output = args.output.resolve()
    assert output.is_relative_to(ROOT) and output != ROOT, 'Use an isolated output beneath this worktree'
    folder = output/NAME
    if not args.seal:
        assert args.build and args.vcpkg and args.vs
        source = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
        temp = ROOT/'consolidation-validation-local/tool-temp'
        temp.mkdir(parents=True, exist_ok=True)
        env = dict(os.environ, TEMP=str(temp), TMP=str(temp), SDL_VIDEODRIVER='dummy',
                   SDL_AUDIODRIVER='dummy', PYTHONDONTWRITEBYTECODE='1')
        command = [os.sys.executable, '-B', str(ROOT/'devtools/create_project/scripts/package-window-layout.py'),
                   '--root', str(ROOT), '--build', str(args.build.resolve()), '--vcpkg', str(args.vcpkg.resolve()),
                   '--vs', str(args.vs.resolve()), '--output', str(output), '--sha', source]
        completed = subprocess.run(command, env=env, capture_output=True, timeout=180)
        output.mkdir(parents=True, exist_ok=True)
        (output/'packager-stdout.txt').write_bytes(completed.stdout)
        (output/'packager-stderr.txt').write_bytes(completed.stderr)
        assert completed.returncode == 0, (completed.returncode, output/'packager-stderr.txt')
        # These files were generated in this newly created package by the invoked
        # packager. Ship user documentation instead of its internal layout preset.
        for name in ('window-layout-test.ini', 'Start window layout test.cmd', 'WINDOW-LAYOUT-TEST.txt'):
            target = folder/name
            assert target.resolve().parent == folder.resolve() and target.is_file()
            target.unlink()
        docs = folder/'docs'
        docs.mkdir()
        for source_doc, target_name in (
            ('doc/ce-portable-storage/README.md', 'PORTABLE-STORAGE.md'),
            ('doc/ce-portable-storage/INVENTORY.md', 'STORAGE-INVENTORY.md'),
            ('doc/ce-test-cli/README.md', 'NATIVE-TEST-CLI.md'),
            ('doc/ce-test-cli/MI4.md', 'MI4-TESTING.md'),
            ('doc/ce-consolidated/HUMAN-ACCEPTANCE.md', 'HUMAN-ACCEPTANCE.md'),
        ):
            shutil.copy2(ROOT/source_doc, docs/target_name)
        guide = docs/'PORTABLE-STORAGE.md'
        text = guide.read_text(encoding='utf-8').replace('(INVENTORY.md)', '(STORAGE-INVENTORY.md)').replace('(RESULTS.md)', '(RELEASE-READINESS.md)')
        guide.write_text(text, encoding='utf-8')
        guide = docs/'NATIVE-TEST-CLI.md'
        text = guide.read_text(encoding='utf-8').replace('(RESULTS.md)', '(RELEASE-READINESS.md)').replace('(MI4.md)', '(MI4-TESTING.md)')
        guide.write_text(text, encoding='utf-8')
        guide = docs/'MI4-TESTING.md'
        guide.write_text(guide.read_text(encoding='utf-8').replace('(README.md)', '(NATIVE-TEST-CLI.md)'), encoding='utf-8')
        guide = docs/'STORAGE-INVENTORY.md'
        guide.write_text(guide.read_text(encoding='utf-8').replace('[Engine source reference index](engine-write-sites.json)', 'The engine source reference index in the source checkout'), encoding='utf-8')
        (folder/'scummvm.ini').write_text('[scummvm]\nextrapath=.\n', encoding='utf-8')
        (folder/'PORTABLE-README.txt').write_text(
            'ScummVM Comfort Edition 2026.4.0 — local consolidated acceptance candidate\n\n'
            'Extract the complete folder to a writable location and run scummvm.exe.\n'
            'CE stores configuration, saves, sessions, achievement metadata, screenshots, logs,\n'
            'caches and temporary files beneath this executable by default. Explicit overrides\n'
            'remain supported. No portable toggle, installer or launcher workaround is required.\n'
            'Move the complete folder while CE is closed. Existing absolute game-data paths\n'
            'may need updating on another computer; commercial game data is not included.\n'
            'No legacy saves or credentials are imported automatically. Read docs/PORTABLE-STORAGE.md\n'
            'before copying legacy files, and preserve originals and CE ownership metadata.\n'
            'Required runtime DLLs, engine data, themes and licenses accompany the executable.\n'
            'Review docs/HUMAN-ACCEPTANCE.md before approving a release. Nothing in this folder\n'
            'constitutes live synchronization approval or installation into an accepted location.\n', encoding='utf-8')
        result = manifest(folder, source)
    else:
        prior = json.loads((folder/'BUILD-MANIFEST.json').read_text(encoding='utf-8'))
        assert prior['package_kind'] == 'CE-consolidated-portable-candidate'
        assert prior['exe_sha256'] == sha(folder/'scummvm.exe')
        for source_doc in ('RELEASE-READINESS.md', 'HUMAN-ACCEPTANCE.md'):
            shutil.copy2(ROOT/'doc/ce-consolidated'/source_doc, folder/'docs'/source_doc)
        report = folder/'docs/RELEASE-READINESS.md'
        if args.delivery_sha:
            report.write_text(report.read_text(encoding='utf-8')+'\nFinal local delivery commit: `'+args.delivery_sha+'`.\n', encoding='utf-8')
        shutil.copy2(report, output/'RELEASE-READINESS.md')
        shutil.copy2(folder/'docs/HUMAN-ACCEPTANCE.md', output/'HUMAN-ACCEPTANCE.md')
        result = manifest(folder, prior['source_sha'], args.delivery_sha)
        archive = output/ZIP_NAME
        assert not archive.exists(), 'Do not overwrite a previously sealed candidate'
        with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as z:
            for p in sorted(folder.rglob('*')):
                if p.is_file():
                    z.write(p, (Path(NAME)/p.relative_to(folder)).as_posix())
        with zipfile.ZipFile(archive) as z:
            assert z.testzip() is None
            for name, expected in result['files'].items():
                assert hashlib.sha256(z.read(NAME+'/'+name)).hexdigest() == expected
        (output/(ZIP_NAME+'.sha256')).write_text(f'{sha(archive)}  {ZIP_NAME}\n', encoding='utf-8')
        (output/'delivery.json').write_text(json.dumps({'source_sha': result['source_sha'],
            'delivery_sha': args.delivery_sha, 'exe_sha256': result['exe_sha256'],
            'archive': str(archive), 'archive_sha256': sha(archive), 'files': len(result['files'])+2}, indent=2), encoding='utf-8')
    print(json.dumps({'folder': str(folder), 'files': len(result['files'])+2,
                      'source_sha': result['source_sha'], 'exe_sha256': result['exe_sha256'], 'sealed': args.seal}, indent=2))


if __name__ == '__main__':
    main()
