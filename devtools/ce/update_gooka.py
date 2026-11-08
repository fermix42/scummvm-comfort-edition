#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Import only Gooka from a pinned upstream revision; preserve local edits."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / 'devtools/ce/gooka-upstream.json'
REPOSITORY = 'fermix42/scummvm'
API = 'https://api.github.com/repos/' + REPOSITORY


def fetch(url):
    request = urllib.request.Request(url, headers={'User-Agent': 'ScummVM-CE-Gooka-import'})
    with urllib.request.urlopen(request, timeout=60) as response:
        return response.read()


def digest(data):
    return hashlib.sha256(data).hexdigest()


def adapt_module(files):
    # create_project cannot expand GNU make's filter-out/patsubst/wildcard expression.
    name = 'engines/gooka/module.mk'
    text = files[name].decode('utf-8')
    expression = 'MODULE_OBJS += $(filter-out rooms/room_data.o,$(patsubst $(srcdir)/engines/gooka/%.cpp,%.o,$(wildcard $(srcdir)/engines/gooka/rooms/room_*.cpp)))'
    if expression in text:
        rooms = sorted(path.removeprefix('engines/gooka/').removesuffix('.cpp') + '.o'
                       for path in files if path.startswith('engines/gooka/rooms/room_')
                       and path.endswith('.cpp') and not path.endswith('/room_data.cpp'))
        continuation = ' ' + chr(92) + chr(10) + chr(9)
        text = text.replace(expression, 'MODULE_OBJS +=' + continuation + continuation.join(rooms))
        files[name] = text.encode('utf-8')


def verify(manifest):
    expected = manifest['files']
    actual = {p.relative_to(ROOT).as_posix() for p in (ROOT / 'engines/gooka').rglob('*') if p.is_file()}
    if actual != set(expected):
        raise SystemExit('Gooka file list differs from manifest; preserve/reconcile local changes first.')
    for name, checksum in expected.items():
        if digest((ROOT / name).read_bytes()) != checksum:
            raise SystemExit('Local changes in ' + name + '; reconcile before updating.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ref', help='required exact reviewed 40-character fork commit for updates')
    parser.add_argument('--checkout', type=Path, help='read pinned Git objects from the maintained fork checkout')
    parser.add_argument('--verify', action='store_true', help='verify the imported snapshot without network access')
    args = parser.parse_args()
    previous = json.loads(MANIFEST.read_text()) if MANIFEST.exists() else None
    if previous:
        verify(previous)
    elif (ROOT / 'engines/gooka').exists():
        raise SystemExit('Untracked Gooka directory exists; refusing to overwrite.')
    if args.verify:
        if not previous:
            raise SystemExit('No imported snapshot.')
        print('Verified Gooka snapshot ' + previous['commit'])
        return
    if not args.ref or not re.fullmatch('[0-9a-f]{40}', args.ref):
        raise SystemExit('Updates require --ref with an exact reviewed fork commit, never a floating branch.')
    from urllib.parse import quote
    commit = args.ref
    if not args.checkout and json.loads(fetch(API + '/commits/' + quote(commit, safe='')))['sha'] != commit:
        raise SystemExit('Fork commit resolution mismatch')
    pending = [] if args.checkout else ['engines/gooka']
    downloaded = {}
    if args.checkout:
        def git(*arguments):
            return subprocess.check_output(['git', '-C', str(args.checkout), *arguments])
        origin = git('remote','get-url','origin').decode().strip().removesuffix('.git')
        if origin != 'https://github.com/' + REPOSITORY:
            raise SystemExit('Checkout origin is not the maintained Gooka fork')
        for entry in git('ls-tree','-r','-z',commit,'--','engines/gooka').split(b'\0'):
            if not entry:
                continue
            metadata, name = entry.decode().split('\t',1)
            mode, kind, sha = metadata.split()
            if mode != '100644' or kind != 'blob' or not name.startswith('engines/gooka/') or '..' in Path(name).parts:
                raise SystemExit('Unsupported fork tree entry: ' + name)
            data = git('show',commit+':'+name)
            blob = b'blob '+str(len(data)).encode()+b'\0'+data
            if hashlib.sha1(blob).hexdigest() != sha:
                raise SystemExit('Fork blob mismatch: '+name)
            downloaded[name] = data
    while pending:
        directory = pending.pop()
        for entry in json.loads(fetch(API + '/contents/' + directory + '?ref=' + commit)):
            name = entry['path']
            if not name.startswith('engines/gooka/') or '..' in Path(name).parts:
                raise SystemExit('Unexpected upstream path: ' + name)
            if entry['type'] == 'dir':
                pending.append(name)
            elif entry['type'] == 'file':
                data = fetch('https://raw.githubusercontent.com/' + REPOSITORY + '/' + commit + '/' + name)
                blob = b'blob ' + str(len(data)).encode() + b'\0' + data
                if hashlib.sha1(blob).hexdigest() != entry['sha']:
                    raise SystemExit('Upstream checksum mismatch: ' + name)
                downloaded[name] = data
            else:
                raise SystemExit('Unsupported upstream entry: ' + name)
    if not downloaded or 'engines/gooka/module.mk' not in downloaded:
        raise SystemExit('Incomplete engine snapshot.')
    upstream_checksums = {name: digest(data) for name, data in sorted(downloaded.items())}
    adapt_module(downloaded)
    if previous:
        verify(previous)
    for name, data in downloaded.items():
        destination = ROOT / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
    for name in set(previous['files'] if previous else ()) - set(downloaded):
        (ROOT / name).unlink()
    manifest = {'repository': 'https://github.com/' + REPOSITORY, 'commit': commit,
                'original_upstream': {'repository': 'https://github.com/Kungergely/scummvm',
                                      'baseline_commit': '66b8f9703e73fa7a2c9bd8e81ea312a824e97fdf'},
                'upstream_files': upstream_checksums,
                'files': {name: digest(data) for name, data in sorted(downloaded.items())}}
    MANIFEST.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print('Imported %d Gooka files at %s' % (len(downloaded), commit))


if __name__ == '__main__':
    main()
