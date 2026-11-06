#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Interactive terminal bridge to the native pipe API; never sends desktop input.

One JSON request per line. Extra client helpers: press(action, frames), text(value), bye.
All engine requests/responses are retained by Client in a JSONL transcript.
"""
import argparse
import json
import pathlib
import sys
import time
from test_cli_client import Client

p = argparse.ArgumentParser(description=__doc__)
for name in ('exe', 'extra', 'root', 'game'):
    p.add_argument('--' + name, type=pathlib.Path, required=True)
p.add_argument('--platform', choices=('windows', 'ps2'), required=True)
p.add_argument('--offscreen', action='store_true')
p.add_argument('--prepare-save-dialog', action='store_true', help='Reach the PS2 native Save confirmation using ordinary input')
a = p.parse_args()
c = Client(a.exe, a.root, a.extra, a.offscreen)
try:
    state = c.launch(a.game, a.platform)
    if a.prepare_save_dialog:
        deadline = time.monotonic() + 150
        while time.monotonic() < deadline:
            adapter = state['adapter']
            if adapter.get('scene') not in (None, 'cdt.set') and adapter.get('cutscene_level') == 0:
                break
            if any('/txtall621/' in text for text in adapter.get('text_objects', [])):
                c.press('open_menu')
            elif adapter.get('menu_handler') == 'saveload_menu':
                c.press('confirm')
            elif not any('/txtall616/' in text for text in adapter.get('text_objects', [])):
                c.press('skip')
            state = c.command('advance', frames=25)
        assert state['adapter'].get('scene') != 'cdt.set', state
        c.command('session_create', title='Native Dialog Probe')
        state = c.press('open_menu')
        for _ in range(16):
            menu = state['adapter']['menus']['main_menu']
            if menu['menu']['cur_item'] == menu['save_index']:
                break
            state = c.axis(1, 32767)
        c.press('confirm')
        state = c.command('inspect')
    print(json.dumps(state), flush=True)
    for line in sys.stdin:
        try:
            request = json.loads(line)
            cmd = request.pop('cmd')
            if cmd == 'bye':
                break
            if cmd == 'press':
                result = c.press(request['action'], request.get('frames', 3))
            elif cmd == 'axis':
                result = c.axis(request['axis'], request['position'], request.get('frames', 3))
            elif cmd == 'text':
                for ch in request['value']:
                    c.command('input', kind='key', key=ord(ch.lower()), ascii=ord(ch), phase='down')
                    c.command('advance', frames=1)
                    c.command('input', kind='key', key=ord(ch.lower()), ascii=ord(ch), phase='up')
                result = c.command('inspect')
            else:
                result = c.command(cmd, check=False, **request)
            print(json.dumps(result), flush=True)
        except Exception as exc:
            print(json.dumps({'client_error': repr(exc)}), flush=True)
finally:
    c.close()
