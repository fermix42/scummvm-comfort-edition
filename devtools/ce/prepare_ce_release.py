#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare a clean CE portable package and deterministic ZIP; never publishes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
NAME = 'scummvm-comfort-edition-windows-x64'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def archive(folder, destination, top):
    with zipfile.ZipFile(destination, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=5) as output:
        for path in sorted(folder.rglob('*')):
            if not path.is_file(): continue
            entry = zipfile.ZipInfo(top+'/'+path.relative_to(folder).as_posix(), (1980, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.create_system = 3
            entry.external_attr = 0o100644 << 16
            output.writestr(entry, path.read_bytes(), compress_type=zipfile.ZIP_DEFLATED, compresslevel=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('build','vcpkg','vs','output'): parser.add_argument('--'+key, type=Path)
    parser.add_argument('--sha', required=True)
    parser.add_argument('--validation', type=Path, help='Final source/executable-matched validation summary to include when sealing')
    parser.add_argument('--seal', action='store_true', help='Verify and archive an already prepared folder after runtime validation')
    args = parser.parse_args()
    release = json.loads((ROOT/'CE-RELEASE.json').read_text())
    source = subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'], text=True).strip()
    if source != args.sha or subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain=v1'], text=True).strip():
        raise SystemExit('Release preparation requires the exact clean committed source')
    assert args.output
    output = args.output.resolve()
    folder = output/NAME
    if not args.seal:
        assert args.build and args.vcpkg and args.vs and not folder.exists()
        output.mkdir(parents=True, exist_ok=True)
        command = [sys.executable,'-B',str(ROOT/'devtools/create_project/scripts/package-window-layout.py'),
                   '--root',str(ROOT),'--build',str(args.build.resolve()),'--vcpkg',str(args.vcpkg.resolve()),
                   '--vs',str(args.vs.resolve()),'--sha',source,'--output',str(output)]
        result = subprocess.run(command, capture_output=True, timeout=180)
        (output/'packager-stdout.log').write_bytes(result.stdout)
        (output/'packager-stderr.log').write_bytes(result.stderr)
        if result.returncode: raise SystemExit('Base packager failed; inspect logs outside the distribution')
        # Remove only known development files created above, never user data.
        for name in ('window-layout-test.ini','Start window layout test.cmd','WINDOW-LAYOUT-TEST.txt','VERSION.txt','ENGINES.txt'):
            path = folder/name
            assert path.resolve().parent == folder.resolve() and path.is_file()
            path.unlink()
        for name in ('CE-RELEASE.json','FORK-CHANGES.md'):
            shutil.copy2(ROOT/name,folder/name)
        shutil.copy2(ROOT/release['notes'],folder/'RELEASE-NOTES.md')
        for name in ('VIDEO-SCALING.md','OPENING-SUBTITLE-VERIFICATION.json',
                     'LOCALIZATION-VERIFICATION.json','VIDEO-SCALING-VERIFICATION.json',
                     'EDITORIAL-VERIFICATION.json'):
            shutil.copy2(ROOT/'doc/ce-gooka'/name,folder/'doc/ce-gooka'/name)
        for name in ('README.md','GLOSSARY.md'):
            dest=folder/'engines/gooka/localization'/name
            dest.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(ROOT/'engines/gooka/localization'/name,dest)
        for name in ('README.md','STRICT-STORAGE.md','ACCEPTANCE.md'):
            dest=folder/'doc/ce-portable-storage'/name;dest.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(ROOT/'doc/ce-portable-storage'/name,dest)
        readme=(folder/'README.md').read_text(encoding='utf-8').replace('('+release['notes']+')','(RELEASE-NOTES.md)')
        for name in ('doc/ce-test-cli/README.md','doc/ce-consolidated/RELEASE-READINESS.md'):
            readme=readme.replace('('+name+')','(https://github.com/fermix42/scummvm-comfort-edition/blob/'+source+'/'+name+')')
        (folder/'README.md').write_text(readme,encoding='utf-8')
        (folder/'GOOKA-CREDITS.txt').write_text('Gooka engine: Gergely Kun (Gergely Kún)\n'
            'https://github.com/Kungergely/scummvm\nGPL-3.0-or-later; see COPYING and doc/ce-gooka/gooka-upstream.json.\n',encoding='utf-8')
        with (folder/'GOOKA-CREDITS.txt').open('a',encoding='utf-8') as credits:
            credits.write('Maintained engine and English localization: https://github.com/fermix42/scummvm\n'
                          'See engines/gooka/localization/README.md for localization provenance.\n')
        (folder/'scummvm.ini').write_bytes(b'[scummvm]\nextrapath=.\n')
        (folder/'SOURCE-COMMIT.txt').write_text(source+'\n',encoding='ascii')
        base = json.loads((folder/'BUILD-MANIFEST.json').read_text())
        base.update({'ce_version':release['version'],'proposed_tag':release['proposed_tag'],
                     'upstream_version':release['upstream_version'],'source_sha':source,
                     'exe_sha256':digest(folder/'scummvm.exe'),'clean_source':True})
        base['files']={p.relative_to(folder).as_posix():digest(p) for p in sorted(folder.rglob('*'))
                       if p.is_file() and p.name not in ('BUILD-MANIFEST.json','SHA256SUMS.txt')}
        (folder/'BUILD-MANIFEST.json').write_text(json.dumps(base,indent=2)+'\n',encoding='utf-8')
        sums=dict(base['files'],**{'BUILD-MANIFEST.json':digest(folder/'BUILD-MANIFEST.json')})
        (folder/'SHA256SUMS.txt').write_text(''.join(h+'  '+name+'\n' for name,h in sorted(sums.items())),encoding='utf-8')
    manifest = json.loads((folder/'BUILD-MANIFEST.json').read_text())
    assert manifest['source_sha']==source and manifest['ce_version']==release['version']
    expected=set(manifest['files'])|{'BUILD-MANIFEST.json','SHA256SUMS.txt'}
    actual={p.relative_to(folder).as_posix() for p in folder.rglob('*') if p.is_file()}
    assert actual==expected, sorted(actual^expected)
    for name,sha in manifest['files'].items(): assert digest(folder/name)==sha,name
    sums=dict(manifest['files'],**{'BUILD-MANIFEST.json':digest(folder/'BUILD-MANIFEST.json')})
    assert (folder/'SHA256SUMS.txt').read_text(encoding='utf-8')==''.join(h+'  '+name+'\n' for name,h in sorted(sums.items()))
    assert (folder/'scummvm.ini').read_bytes()==b'[scummvm]\nextrapath=.\n'
    forbidden=[name for name in actual if name.lower().endswith(('.log','.gsv','.ps2','.hdv','.ce-achievements.json'))
               or Path(name).name in ('profile.ini','ce-playthroughs.json','ce-achievements-queue.json')
               or 'test.ini' in name.lower()]
    assert not forbidden,forbidden
    if args.seal:
        if args.validation:
            validation=json.loads(args.validation.read_text(encoding='utf-8'))
            assert validation['source_commit']==source and validation['executable_sha256']==manifest['exe_sha256']
            assert validation['status']=='PASS', 'Do not seal a passing release from failed validation'
            (folder/'VALIDATION-SUMMARY.json').write_text(json.dumps(validation,indent=2)+'\n',encoding='utf-8')
            manifest['files']['VALIDATION-SUMMARY.json']=digest(folder/'VALIDATION-SUMMARY.json')
            (folder/'BUILD-MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
            sums=dict(manifest['files'],**{'BUILD-MANIFEST.json':digest(folder/'BUILD-MANIFEST.json')})
            (folder/'SHA256SUMS.txt').write_text(''.join(h+'  '+name+'\n' for name,h in sorted(sums.items())),encoding='utf-8')
            actual.add('VALIDATION-SUMMARY.json')
        top='scummvm-comfort-edition-'+release['version']+'-windows-x64'
        target=output/(top+'.zip'); repeat=output/(top+'.repeat.zip')
        archive(folder,target,top);archive(folder,repeat,top)
        sha=digest(target);assert digest(repeat)==sha
        with zipfile.ZipFile(target) as zipped: assert zipped.testzip() is None
        # A disposable comparison ZIP created by this invocation only.
        assert repeat.resolve().parent==output;repeat.unlink()
        target.with_suffix('.zip.sha256').write_text(sha+'  '+target.name+'\n',encoding='ascii')
        report={'status':'PASS','source_commit':source,'ce_version':release['version'],
                'proposed_tag':release['proposed_tag'],'exe_sha256':digest(folder/'scummvm.exe'),
                'zip':target.name,'zip_sha256':sha,'files':len(actual),
                'repeat_archive_sha256_identical':True,'compiler_bit_identity_verified':False,
                'draft_release_notes':'DRAFT-RELEASE-NOTES.md','published':False,'tag_created':False}
        notes=(ROOT/release['notes']).read_text(encoding='utf-8')
        notes+='\n\nCandidate identity\n\nSource commit: '+source+'\n\nExecutable SHA256: '+report['exe_sha256']+'\n\nZIP: '+target.name+'\n\nZIP SHA256: '+sha+'\n'
        if args.validation:
            notes+='\nValidation results:\n\n'+json.dumps(validation['tests'],indent=2)+'\n'
        (output/'DRAFT-RELEASE-NOTES.md').write_text(notes,encoding='utf-8')
        (output/'release-artifacts.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2))
    else: print('Prepared clean CE',release['version'],len(actual),'files at',folder)


if __name__ == '__main__': main()
