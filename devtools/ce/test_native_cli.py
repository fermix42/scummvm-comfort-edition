#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Native executable protocol and optional legitimate MI4 runtime regression.

All artifacts are disposable under --artifacts. No desktop inputs or script overrides.
Each pilot starts a new process and retains complete command/state transcripts.
"""
import argparse
import json
import pathlib
import hashlib
import time
import subprocess
import shutil
import stat
import configparser
import os
from test_cli_client import Client


def native_save(c, label):
    """Choose real menu entries using their actual script-owned selection indices."""
    state = c.command('inspect')
    if state['adapter']['menu_handler'] != 'main_menu':
        state = c.press('open_menu')
    assert state['adapter']['menu_handler'] == 'main_menu', state
    for _ in range(16):
        menu = state['adapter']['menus']['main_menu']
        if menu['menu']['cur_item'] == menu['save_index']:
            break
        state = c.axis(1, 32767) if state['platform'] == 'ps2' else c.press('down')
    else:
        raise AssertionError('native Save entry not reached')
    c.press('confirm')
    if state['platform'] == 'ps2':
        # Retail PS2 saves the profile selected at startup and directly asks Save/Cancel.
        confirmation = c.command('inspect')['adapter']['text_objects']
        assert any('sure you want to save' in text for text in confirmation), confirmation
        dialog = c.command('inspect')['adapter']['native_dialog']
        save_choice = next(int(k) for k, v in dialog['choices'].items() if v.get('text', '').startswith('/txtall570/'))
        for _ in range(3):
            if dialog['current_choice'] == save_choice:
                break
            dialog = c.axis(0, -32767)['adapter']['native_dialog']
        c.command('assert', field='adapter.native_dialog.current_choice', equals=save_choice)
        c.press('confirm')
    else:
        c.command('wait', field='adapter.menu_handler', equals='saveload_menu', timeout_ms=10000)
        c.press('confirm')
        c.command('wait', field='adapter.menus.saveload_menu.edit_line.is_active', equals=1, timeout_ms=10000)
        c.type_text(label)
        c.command('assert', field='adapter.menus.saveload_menu.edit_line.text', equals=label)
        c.press('confirm')
    c.command('wait', field='adapter.mode', equals=2, timeout_ms=20000)
    c.command('wait', field='adapter.button_handler_kind', equals='function', timeout_ms=10000)
    return c.command('saves')['saves']


def protocol(exe, root, extra):
    c = Client(exe, root, extra)
    try:
        assert c.command('inspect')['status'] == 'idle'
        c.process.stdin.write('[' * 40 + ']' * 40 + '\n')
        c.process.stdin.flush()
        assert c.receive()['error'] == 'invalid_request'
        assert c.command('capabilities')['protocol'] == 'ce-test-cli/1'
        assert c.command('nonsense', check=False)['error'] == 'unknown_command'
        assert c.command('input', check=False, kind='key', key=1, phase='down')['error'] == 'not_running'
        c.process.stdin.write('{broken json}\n')
        c.process.stdin.flush()
        assert c.receive()['error'] == 'invalid_request'
        assert c.command('inspect')['status'] == 'idle'
        assert c.command('launch', path='missing', game='monkey4', engine='grim', platform='windows', check=False)['error'] == 'invalid_launch'
        c.close()
        return {'case': 'protocol', 'checks': 8, 'passed': True}
    except BaseException:
        c.abort()
        raise


def activation(exe, root):
    root.mkdir()
    flags = {'creationflags': getattr(subprocess, 'CREATE_NO_WINDOW', 0)}
    missing = subprocess.run([str(exe), '--test-cli'], input='', capture_output=True, text=True, timeout=15, **flags)
    assert missing.returncode == 64 and json.loads(missing.stdout)['error'] == 'absolute_test_root_required'
    forbidden = subprocess.run([str(exe), '--test-cli', f'--test-root={root}', '--savepath=forbidden'],
                               input='', capture_output=True, text=True, timeout=15, **flags)
    assert forbidden.returncode == 64 and json.loads(forbidden.stdout)['error'] == 'unknown_test_option'
    sentinel = root/'sentinel.txt'
    sentinel.write_text('preserve this unrelated file')
    occupied = subprocess.run([str(exe), '--test-cli', f'--test-root={root}'],
                              input='', capture_output=True, text=True, timeout=15, **flags)
    assert occupied.returncode == 64 and json.loads(occupied.stdout)['error'] == 'test_root_not_empty'
    assert sentinel.read_text() == 'preserve this unrelated file' and list(root.iterdir()) == [sentinel]
    receipt = {'case': 'activation-isolation', 'checks': 4, 'passed': True,
               'missing_root': missing.returncode, 'forbidden_override': forbidden.returncode,
               'occupied_root': occupied.returncode}
    (root.parent/'activation.json').write_text(json.dumps(receipt, indent=2))
    return receipt


def pilot(exe, root, extra, game, platform, offscreen):
    c = Client(exe, root, extra, offscreen)
    try:
        c.launch(os.path.relpath(game, pathlib.Path.cwd()), platform)
        state = c.command('advance', frames=2)
        # Ordinary intro/movie/dialogue input; credits are not gameplay evidence.
        deadline = time.monotonic() + 150
        while time.monotonic() < deadline:
            adapter = state['adapter']
            if (adapter.get('scene') not in (None, 'cdt.set') and adapter.get('cutscene_level') == 0
                    and adapter.get('mode') == 2 and adapter.get('button_handler_kind') in ('table', 'function')):
                break
            if any('/txtall621/' in text for text in adapter.get('text_objects', [])):
                c.press('open_menu')  # Actual PS2 Press Start state, not a scene-name guess.
            elif platform == 'ps2' and adapter.get('menu_handler') == 'saveload_menu':
                c.press('confirm')  # PS2's real initial four-slot New Game/profile selection.
            elif adapter.get('menu_handler') == 'main_menu':
                c.press('confirm')
            elif any('/txtall616/' in text for text in adapter.get('text_objects', [])):
                pass  # Native Loading state: allow its scripts to finish.
            else:
                c.press('skip')
                c.command('input', kind='key', key=46, ascii=46, phase='down')
                c.command('advance', frames=2)
                c.command('input', kind='key', key=46, ascii=46, phase='up')
            state = c.command('advance', frames=25)
        assert state['adapter'].get('cutscene_level') == 0 and state['adapter'].get('scene') != 'cdt.set', state
        session = c.command('session_create', title='Native CLI A')['session']
        limit = state['adapter']['native_slots']
        assert c.command('load', slot=limit, check=False)['error'] == 'invalid_slot'
        assert c.command('load', slot=0, check=False)['error'] == 'save_not_owned'
        assert c.command('save', slot=0, check=False)['error'] == 'native_save_menu_required'
        initial = c.command('inspect')['adapter']
        moved = initial
        for direction in ('right', 'up', 'left', 'down'):
            c.press(direction, 20)
            moved = c.command('inspect')['adapter']
            if moved['actor'] != initial['actor']:
                break
        # Observe real position/orientation changes; an acknowledged event alone is not a pass.
        assert moved['actor'] != initial['actor'], (initial, moved)
        if platform == 'windows':
            c.command('input', kind='action', map='monkey4', action='GMNU', phase='down')
            c.command('advance', frames=3)
            c.command('input', kind='action', map='monkey4', action='GMNU', phase='up')
            c.command('advance', frames=2)
        else:
            c.press('open_menu')
        menu = c.command('inspect')
        assert menu['adapter']['menu_handler'] == 'main_menu', menu
        before = {'adapter': moved}
        saves = native_save(c, 'Native CLI A')
        own = [s for s in saves if s['session'] == session]
        assert own and own[0]['description'], saves
        assert own[0]['thumbnail_width'] == 160 and own[0]['thumbnail_height'] == 120, own
        saved_actor = c.command('inspect')['adapter']['actor']
        save_a = root/'saves'/own[0]['file']
        hash_a = hashlib.sha256(save_a.read_bytes()).hexdigest()
        new_session = c.command('session_create', title='Native CLI B')
        session_b = new_session['session']
        assert new_session['ce']['sidecar'] == '' and new_session['ce']['progress'] == {}, new_session['ce']
        assert session_b and session_b != session
        banks = native_save(c, 'Native CLI B')
        assert {session, session_b} <= {s['session'] for s in banks}, banks
        assert len({s['file'] for s in banks}) >= 2, banks
        assert hashlib.sha256(save_a.read_bytes()).hexdigest() == hash_a
        storage = c.command('storage')
        for key, suffix in [('savepath', 'saves'), ('screenshotpath', 'screenshots'),
                            ('cachepath', 'cache'), ('temppath', 'temp'),
                            ('iconspath', 'data/icons'), ('dlcspath', 'data/dlcs')]:
            assert pathlib.Path(storage[key]).resolve() == (root/suffix).resolve(), storage
        assert pathlib.Path(storage['logfile']).resolve() == (root/'engine.log').resolve(), storage
        assert pathlib.Path(storage['generated_cache']).is_relative_to(root/'cache/hpl1'), storage
        c.command('screenshot')
        c.command('advance', frames=2)
        screenshots = list((root/'screenshots').glob('*.png'))
        assert screenshots and screenshots[0].read_bytes().startswith(b'\x89PNG'), screenshots
        assert (root/'temp').is_dir()
        assert not (root/'cache').exists(), 'Unused cache should not be created'
        c.command('storage_probe')
        assert (root/'data/dlcs/ce-storage-probe/nested').is_dir()
        assert (root/'cache/ce-storage-probe.bin').read_bytes() == b'CE storage diagnostic\n'
        assert (root/'temp/ce-storage-probe.bin').read_bytes() == b'CE storage diagnostic\n'
        assert c.command('storage_probe', check=False)['error'] == 'storage_probe_file_exists'

        queue = c.command('inspect')['ce']['queue']
        for owner in (session, session_b):
            assert any(e['type'] == 'playthrough_save' and e['session'] == owner for e in queue), queue
        c.command('session_select', session=session)
        c.close()

        cold = Client(exe, root, extra, offscreen)
        c = cold
        c.launch(game, platform)
        c.command('advance', frames=2)
        c.command('session_select', session=session)
        c.command('load', slot=0)
        c.command('advance', frames=2)
        c.command('wait', field='adapter.button_handler_kind', equals='function', timeout_ms=10000)
        restored = c.command('inspect')
        assert restored['session'] == session, restored
        assert restored['adapter']['scene'] == before['adapter']['scene'], (before, restored)
        assert restored['adapter']['actor'] == saved_actor, (saved_actor, restored['adapter']['actor'])
        assert all(r['status'] == 'active' for r in restored['ce']['sessions'] if r['id'] in (session, session_b))
        c.command('assert', field='session', equals=session)
        c.command('assert', field='adapter.scene', equals=before['adapter']['scene'])
        c.command('session_select', session=session_b)
        c.command('load', slot=0)
        c.command('advance', frames=2)
        c.command('wait', field='adapter.button_handler_kind', equals='function', timeout_ms=10000)
        assert c.command('inspect')['session'] == session_b
        c.close()
        # A previously owned profile cannot redirect auxiliary test writes.
        profile_ini = root/'profile.ini'
        config = configparser.ConfigParser(interpolation=None)
        config.read(profile_ini, encoding='utf-8')
        outside = root.parent/(root.name + '-forbidden-auxiliary')
        for key in ('screenshotpath', 'cachepath', 'temppath', 'iconspath', 'dlcspath'):
            config['scummvm'][key] = str(outside/key)
        with profile_ini.open('w', encoding='utf-8') as stream:
            config.write(stream)
        isolated = Client(exe, root, extra, offscreen)
        try:
            isolated.launch(game, platform)
            protected = isolated.command('storage')
            for key in ('screenshotpath', 'cachepath', 'temppath', 'iconspath', 'dlcspath'):
                assert pathlib.Path(protected[key]).is_relative_to(root), protected
            isolated.command('screenshot')
            isolated.close()
        except BaseException:
            isolated.abort()
            raise
        assert not outside.exists(), 'Owned test profile redirected an auxiliary write'
        readonly_root = root.parent/(root.name + '-readonly-config')
        # CopyFile2 can reject long sidecar destinations without an extended
        # Win32 prefix, even though the long-path-aware CE executable uses them.
        source_copy = str(root.resolve())
        target_copy = str(readonly_root.resolve())
        if os.name == 'nt':
            source_copy = chr(92) * 2 + '?' + chr(92) + source_copy
            target_copy = chr(92) * 2 + '?' + chr(92) + target_copy
        shutil.copytree(source_copy, target_copy)
        profile = readonly_root/'profile.ini'
        original = hashlib.sha256(profile.read_bytes()).hexdigest()
        profile.chmod(stat.S_IREAD)
        broken = None
        rejected = False
        try:
            broken = Client(exe, readonly_root, extra, offscreen)
            broken.launch(game, platform)
            broken.command('session_create', title='Read-only configuration must fail')
        except RuntimeError as exc:
            rejected = 'configuration' in str(exc).lower()
        finally:
            if broken is not None:
                broken.abort()
            profile.chmod(stat.S_IREAD | stat.S_IWRITE)
        assert rejected, 'Read-only configuration was not reported as an error'
        assert hashlib.sha256(profile.read_bytes()).hexdigest() == original
        assert (readonly_root/'profile.ini.tmp').is_file(), 'Failed atomic replacement did not retain recovery data'
        return {'case': platform, 'passed': True, 'session': session,
                'scene': restored['adapter']['scene'], 'native_menu': True, 'cold_restore': True,
                'portable_storage_checks': 24, 'auxiliary_isolation': True, 'readonly_configuration_rejected': True, 'bank_isolation': True, 'second_session': session_b, 'save_a_sha256': hash_a}
    except BaseException:
        c.abort()
        raise


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--exe', type=pathlib.Path, required=True)
    p.add_argument('--extra', type=pathlib.Path, required=True)
    p.add_argument('--artifacts', type=pathlib.Path, required=True)
    p.add_argument('--windows', type=pathlib.Path)
    p.add_argument('--ps2', type=pathlib.Path)
    p.add_argument('--cine', type=pathlib.Path)
    p.add_argument('--offscreen', action='store_true')
    a = p.parse_args()
    a.artifacts.mkdir(parents=True, exist_ok=False)
    results = []
    cases = [('activation-isolation', lambda: activation(a.exe, a.artifacts/'occupied')),
             ('protocol', lambda: protocol(a.exe, a.artifacts/'protocol', a.extra))]
    if a.cine:
        def generic():
            c = Client(a.exe, a.artifacts/'cine', a.extra)
            try:
                c.launch(a.cine, 'pc', game='os', engine='cine')
                state = c.command('inspect')
                assert state['adapter']['error'] == 'unsupported', state
                assert state['video_driver'] == 'dummy', state
                c.command('input', kind='mouse_move', x=100, y=80)
                c.command('input', kind='key', key=32, ascii=32, phase='down')
                c.command('input', kind='key', key=32, ascii=32, phase='up')
                assert c.command('advance', frames=2, check=False)['error'] == 'unsupported'
                assert c.command('input', kind='action', map='absent', action='absent', phase='down', check=False)['error'] == 'unsupported'
                assert c.command('input', kind='axis', axis=6, position=1, check=False)['error'] == 'invalid_axis'
                assert c.command('wait', field='no_such_field', equals=True, timeout_ms=30, check=False)['error'] == 'timeout'
                assert c.command('inspect')['status'] == 'running'
                c.close(expected=2)
                return {'case': 'generic-cine', 'passed': True, 'checks': 11}
            except BaseException:
                c.abort()
                raise
        cases.append(('generic-cine', generic))
    for platform, game in [('windows', a.windows), ('ps2', a.ps2)]:
        if game:
            cases.append((platform, lambda platform=platform, game=game: pilot(a.exe, a.artifacts/platform, a.extra, game, platform, a.offscreen)))
    for name, run in cases:
        try:
            result = run()
        except Exception as exc:
            result = {'case': name, 'passed': False, 'error': repr(exc)}
        results.append(result)
        print(json.dumps(result), flush=True)
        (a.artifacts/'summary.json').write_text(json.dumps(results, indent=2))
    return int(any(not r['passed'] for r in results))


if __name__ == '__main__':
    raise SystemExit(main())
