#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Supplemental MI4 Windows native Load-menu acceptance using existing CLI capabilities."""
import argparse
import json
from pathlib import Path
import time
from test_cli_client import Client
from test_native_cli import native_save


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--extra', type=Path, required=True)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    args = parser.parse_args()
    root = args.artifacts.resolve()
    root.mkdir(parents=True, exist_ok=False)
    c = Client(args.exe.resolve(), root/'owned', args.extra.resolve())
    try:
        state = c.launch(args.game.resolve(), 'windows')
        deadline = time.monotonic()+150
        while time.monotonic() < deadline:
            a = state['adapter']
            if a.get('scene') == 'wed.set' and a.get('cutscene_level') == 0 and a.get('mode') == 2:
                break
            c.press('skip')
            c.command('input', kind='key', key=46, ascii=46, phase='down')
            c.command('advance', frames=2)
            c.command('input', kind='key', key=46, ascii=46, phase='up')
            state = c.command('advance', frames=25)
        else:
            raise AssertionError('Normal intro did not reach gameplay')
        owner = c.command('session_create', title='CLI Load')['session']
        c.press('right', 20)
        entries = native_save(c, 'CLI Load')
        assert any(s['session'] == owner and s['description'] == 'CLI Load' for s in entries)
        saved = c.command('inspect')
        c.press('left', 25)
        changed = c.command('inspect')
        assert changed['adapter']['actor'] != saved['adapter']['actor']
        state = c.press('open_menu')
        assert state['adapter']['menu_handler'] == 'main_menu'
        for _ in range(16):
            menu = state['adapter']['menus']['main_menu']
            if menu['menu']['cur_item'] == menu['load_index']:
                break
            state = c.press('down')
        else:
            raise AssertionError('Native Load entry not reached')
        c.press('confirm')
        c.command('wait', field='adapter.menu_handler', equals='saveload_menu', timeout_ms=10000)
        c.command('assert', field='adapter.menus.saveload_menu.mode', equals=1)
        c.command('assert', field='adapter.menus.saveload_menu.menu.cur_item', equals=1)
        c.press('confirm')
        dialog = c.command('inspect')['adapter']['native_dialog']
        assert dialog.get('prompt', '').startswith('/txtall519/'), dialog
        okay = next(int(k) for k, choice in dialog['choices'].items() if choice.get('text', '').startswith('/txtall377/'))
        for _ in range(3):
            if dialog['current_choice'] == okay:
                break
            dialog = c.press('left')['adapter']['native_dialog']
        c.command('assert', field='adapter.native_dialog.current_choice', equals=okay)
        c.press('confirm')
        c.command('advance', frames=5)
        c.command('wait', field='adapter.mode', equals=2, timeout_ms=15000)
        c.command('wait', field='adapter.button_handler_kind', equals='function', timeout_ms=15000)
        restored = c.command('inspect')
        assert restored['adapter']['actor'] == saved['adapter']['actor'], (saved['adapter'], restored['adapter'])
        assert restored['adapter']['scene'] == saved['adapter']['scene']
        assert restored['session'] == owner
        assert not any(r.get('request', {}).get('cmd') == 'load' for r in c.records)
        c.close()
        receipt = {'status': 'PASS', 'edition': 'windows', 'renderer': 'TinyGL/headless',
                   'native_save_menu': True, 'native_load_menu': True, 'direct_load_command_used': False,
                   'actor_changed_before_restore': True, 'exact_actor_restored': True,
                   'session_owner_restored': owner, 'scene': restored['adapter']['scene'],
                   'executable_sha256': c.records[0]['exe_sha256'],
                   'scope': 'Existing native event/menu adapter only; no new engine adapter, evaluator or state mutation.'}
        (root/'verification.json').write_text(json.dumps(receipt, indent=2))
        print(json.dumps(receipt, indent=2))
    except BaseException:
        c.abort()
        raise


if __name__ == '__main__':
    main()
