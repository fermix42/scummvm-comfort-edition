#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real Czech CD gameplay/save/cold-restore pilot using existing native hooks.

Requires owned 0.BIN..11.BIN and videos. No game data is modified or shipped.
GOOKA_TEST is the imported engine's existing opt-in test language; the normal
native controller supplies Escape to skip the opening video. No new adapter.
"""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import struct
from test_cli_client import Client


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def place(path):
    raw = path.read_bytes()
    if raw[:2] == b'\x1f\x8b': raw = gzip.decompress(raw)
    assert raw[:4] == b'GOOK' and struct.unpack_from('<I', raw, 4)[0] == 1
    fields = ('room', 'music', 'frame', 'visible', 'scale', 'x', 'y', 'priority',
              'subpriority', 'position_x', 'position_y', 'walk_x', 'walk_y', 'walk_direction')
    return dict(zip(fields, struct.unpack_from('<14i', raw, 8)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('exe', 'extra', 'game', 'artifacts'): parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    root = args.artifacts.resolve(); root.mkdir(parents=True, exist_ok=False)
    game = args.game.resolve(strict=True)
    assets = {p.name: digest(p) for p in game.iterdir() if p.is_file()}
    for name, size, expected in [('0.BIN', 68677632, 'b9579c10827278aff76cfd954844a87d'),
                                 ('1.BIN', 6123520, '01055407c2d36d44cfba42df11f324aa')]:
        with (game/name).open('rb') as stream: actual = hashlib.md5(stream.read(5000)).hexdigest()
        assert (game/name).stat().st_size == size and actual == expected
    assert all((game/(str(i)+'.BIN')).is_file() for i in range(12))

    def run(directory, script):
        previous = os.environ.get('GOOKA_TEST')
        os.environ['GOOKA_TEST'] = script
        client = None
        try:
            client = Client(args.exe.resolve(), directory, args.extra.resolve())
            state = client.launch(game, 'windows', game='gooka', engine='gooka')
            assert state['engine'] == 'gooka' and state['status'] == 'running'
            client.command('input', kind='key', key=27, ascii=27, phase='down')
            client.process.wait(timeout=60)
            client.close()
        except BaseException:
            if client: client.abort()
            raise
        finally:
            if previous is None: os.environ.pop('GOOKA_TEST', None)
            else: os.environ['GOOKA_TEST'] = previous

    first = root/'first'
    run(first, 'newgame;shot:screenshots/bedroom.png;esc;ticks:200;'
        'shot:screenshots/hall.png;save:1;click:400:390;ticks:300;esc;ticks:300;'
        'shot:screenshots/study.png;save:2;quit')
    a, b = place(first/'saves/cli-game.001'), place(first/'saves/cli-game.002')
    assert a['room'] == 800 and b['room'] == 817, (a, b)
    assert (a['x'], a['y']) != (b['x'], b['y'])
    saved_hash = digest(first/'saves/cli-game.002')
    run(first, 'load:2;shot:screenshots/cold.png;save:3;quit')
    restored = place(first/'saves/cli-game.003')
    assert restored == b, (b, restored)
    assert digest(first/'saves/cli-game.002') == saved_hash
    log = (first/'engine.log').read_text(encoding='utf-8', errors='replace')
    assert 'load slot 2: No error' in log and 'save slot 3: No error' in log
    for name in ('bedroom', 'hall', 'study', 'cold'):
        png = first/'screenshots'/(name+'.png')
        header = png.read_bytes()[:24]
        assert header[:8] == b'\x89PNG\r\n\x1a\n' and struct.unpack('>II', header[16:24]) == (640, 480)
    assert all(digest(game/name) == sha for name, sha in assets.items())
    report = {'status': 'PASS', 'executable_sha256': digest(args.exe.resolve()),
              'edition': 'Czech Windows CD v1.0.3.0', 'detector_signatures_unchanged': True,
              'actual_resource_containers': 12, 'rooms': [854, a['room'], b['room']],
              'input': 'native Escape and imported click/tick hooks', 'save_before': b,
              'cold_restore': restored, 'original_save_unchanged': True, 'game_assets_unchanged': True,
              'screenshots': {name: digest(first/'screenshots'/(name+'.png')) for name in ('bedroom','hall','study','cold')},
              'scope': 'Actual rendering/story movement/room transitions and normal engine save/load APIs; not in-game save-dialog navigation or a full playthrough.',
              'limitations': ['Opening video is skipped by a normal Escape event; full playback and audible output are not verified.',
                              'Upstream marks Gooka unstable and retains incomplete NPC/room behavior.']}
    (root/'verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__': main()
