#!/usr/bin/env python3
"""Registration, real detection and Add Game using an isolated portable build."""
import argparse
import configparser
import hashlib
import json
import os
from pathlib import Path
import subprocess

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--exe', type=Path, required=True)
    p.add_argument('--chd', type=Path, required=True)
    p.add_argument('--cue', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    a.output.mkdir()
    records = []
    def run(label, *arguments):
        r = subprocess.run([str(a.exe), *arguments], cwd=a.exe.parent,
            capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=45,
            env=dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy'),
            creationflags=subprocess.CREATE_NO_WINDOW)
        (a.output / (label + '.txt')).write_text(r.stdout + '\n' + r.stderr, encoding='utf-8')
        records.append(dict(label=label, arguments=arguments, exit=r.returncode))
        assert r.returncode == 0, (label, r.stderr)
        return r.stdout
    for option in ('--list-engines', '--list-all-engines'):
        text = run(option[2:], option)
        assert 'snatcher' in text.lower() and 'nox' in text.lower()
    for label, game in [('chd', a.chd), ('cue', a.cue)]:
        detected = run(label + '-detect', '--detect', '--path=' + str(game), '--game=snatcher')
        assert 'Snatcher' in detected
        config = a.exe.parent / (a.output.name + '-' + label + '-add.ini')
        assert not config.exists()
        run(label + '-add', '--config=' + str(config), '--add', '--path=' + str(game), '--game=snatcher')
        parser = configparser.ConfigParser(interpolation=None)
        parser.read(config)
        assert 'snatcher-us-segacd' in parser.sections(), parser.sections()
        target = parser['snatcher-us-segacd']
        assert target['gameid'] == 'snatcher' and target['engineid'] == 'snatcher'
        assert target['platform'] == 'segacd'
    (a.output / 'receipt.json').write_text(json.dumps(dict(status='PASS',
        executable_sha256=hashlib.file_digest(a.exe.open('rb'), 'sha256').hexdigest(),
        records=records), indent=2))

if __name__ == '__main__':
    main()
