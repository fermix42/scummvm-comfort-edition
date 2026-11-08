#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Representative AGS native-input storage pilot; no desktop input or save adapter.

Primordia's own startup and standard AGS/ScummVM event handling are used.
"""
import argparse
import configparser
import hashlib
import json
from pathlib import Path
import time
from test_cli_client import Client

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--exe', type=Path, required=True)
parser.add_argument('--extra', type=Path, required=True)
parser.add_argument('--game', type=Path, required=True)
parser.add_argument('--artifacts', type=Path, required=True)
parser.add_argument('--interactive', action='store_true')
args = parser.parse_args()
root = args.artifacts.resolve()
client = Client(args.exe.resolve(), root, args.extra.resolve())
try:
    (root/'profile.ini').write_text('[scummvm]\nconsole=false\nconfirm_exit=false\n'
        'enable_unsupported_game_warning=false\nautosave_period=1\nce_achievements_popups_enabled=false\n'
        '[cli-game]\nengineid=ags\ngameid=primordia\nplatform=windows\npath='+str(args.game.resolve())+
        '\ncheck_gamedata=false\nsave_override=true\nce_playthrough_tracking=true\n', encoding='utf-8')
    client.launch(args.game.resolve(), 'windows', game='primordia', engine='ags')
    time.sleep(3)
    print(json.dumps(client.command('inspect')), flush=True)
    print(json.dumps(client.command('storage')), flush=True)
    client.command('screenshot')
    if args.interactive:
        while True:
            request = json.loads(input())
            if request.get('cmd') == 'finish': break
            if request.get('cmd') == 'delay': time.sleep(request['seconds']); continue
            print(json.dumps(client.command(**request)), flush=True)
    else:
        client.command('storage_probe')
        time.sleep(12)  # Publisher logos precede Primordia's own main menu.
        client.command('input', kind='mouse_move', x=195, y=104)
        time.sleep(0.2)
        client.command('input', kind='mouse_button', x=195, y=104, button=1, phase='down')
        time.sleep(0.2)
        client.command('input', kind='mouse_button', x=195, y=104, button=1, phase='up')
        time.sleep(3)
        client.command('input', kind='key', key=27, ascii=27, phase='down')
        time.sleep(0.2)
        client.command('input', kind='key', key=27, ascii=27, phase='up')
        deadline = time.monotonic()+90
        while not (root/'saves/cli-game.002').exists() and time.monotonic() < deadline:
            client.command('inspect')
            time.sleep(1)
        assert (root/'saves/cli-game.002').is_file(), 'Primordia did not create its own checkpoint save'
        client.command('screenshot')
        created_saves = client.command('saves')
        assert any(row['chooser_slot'] == 2 and row['description'].lower() == 'autosave' for row in created_saves['saves'])
        print(json.dumps(created_saves), flush=True)
    client.close()
    assert list((root/'screenshots').glob('*.png'))
    saves = [p.name for p in (root/'saves').iterdir()]
    if not args.interactive:
        checkpoint = hashlib.sha256((root/'saves/cli-game.002').read_bytes()).hexdigest()
        config = configparser.ConfigParser(interpolation=None)
        config.read(root/'profile.ini', encoding='utf-8')
        config['cli-game']['save_slot'] = '2'
        with (root/'profile.ini').open('w', encoding='utf-8') as output: config.write(output)
        client = Client(args.exe.resolve(), root, args.extra.resolve())
        client.launch(args.game.resolve(), 'windows', game='primordia', engine='ags')
        time.sleep(5)
        client.command('screenshot')
        restored_saves = client.command('saves')
        print(json.dumps(restored_saves), flush=True)
        client.close()
        log = (root/'engine.log').read_text(encoding='utf-8', errors='replace')
        assert 'Restoring saved game' in log and 'cli-game.002' in log, 'No AGS restore attempt recorded'
        assert 'Unable to restore the saved game' not in log, 'AGS reported a restore failure'
        assert hashlib.sha256((root/'saves/cli-game.002').read_bytes()).hexdigest() == checkpoint
        saves = [p.name for p in (root/'saves').iterdir()]
    report = {'status': 'PASS', 'engine': 'ags', 'game': 'primordia', 'save_files': saves,
              'checkpoint_cold_restart': not args.interactive,
              'game_autosave': (root/'saves/cli-game.002').is_file(),
              'standard_autosave': (root/'saves/cli-game.000').is_file(),
              'scope': 'Actual AGS launch, game-generated checkpoint save, cold restart requesting that slot, screenshot and storage probes.'}
    (root/'ags-verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report), flush=True)
except BaseException:
    client.abort()
    raise
