#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the native encounter and CE Global Menu against private v119 fixtures."""
import argparse
import configparser
import hashlib
import json
from pathlib import Path
import shutil
import struct
import time

from test_party_invulnerability import Client, digest, encode_checkpoint, sections


def press(client, key, ascii=0, **options):
    client.command('input', kind='key', key=key, ascii=ascii, phase='down', **options)
    client.command('advance', frames=1, timeout=8)
    client.command('input', kind='key', key=key, ascii=ascii, phase='up', **options)
    client.command('advance', frames=1, timeout=8)


def inspect(client):
    return client.command('inspect')


def select_save(client, slot):
    return next(row for row in client.command('saves')['saves'] if f'slot{slot:03}.' in row['file'])


def save_state(client, root, slot):
    client.command('save', slot=slot, description=f'Encounter state {slot}')
    raw = (root / 'saves' / select_save(client, slot)['file']).read_bytes()
    native = raw.index(b'NPLS', 32)
    size = struct.unpack_from('<I', raw, native + 6)[0]
    return sections(raw[native + 10:native + 10 + size])


def launch(exe, root, game, extra, fixture, cheat, mode, **target_options):
    source = game / 'conversion/visual1139/baseline-new/saves'
    old = next(source.glob('*slot000.g0'))
    client = Client(exe, root, extra)
    try:
        cfg = configparser.ConfigParser(interpolation=None)
        cfg['scummvm'] = dict(enable_unsupported_game_warning='false')
        cfg['cli-game'] = dict(engineid='nox', gameid='nox', platform='apple2', path=str(game), check_gamedata='false',
                               ce_playthrough_tracking='true', nox_combat_mode=str(mode), nox_combat_scroll_speed='3')
        if cheat:
            cfg['cli-game'].update(ce_cheats_enabled='true', nox_party_invulnerability='true')
        cfg['cli-game'].update(target_options)
        with (root / 'profile.ini').open('w') as stream:
            cfg.write(stream)
        raw = old.read_bytes()
        assert raw[32:36] == b'NSM2'
        end = 40 + struct.unpack_from('<I', raw, 36)[0]
        payload = raw[32:end] + fixture.read_bytes()
        header = b'NXSV' + struct.pack('<III', 2, 1, len(payload))
        wrapped = header + hashlib.md5(header + payload + b'NXOK').digest() + payload + b'NXOK'
        path = root / 'saves' / old.name
        path.write_bytes(wrapped)
        sidecar = Path(str(old) + '.ce-achievements.json')
        data = json.loads(sidecar.read_text())
        data.update(save_hash=digest(path), tainted=True)
        (root / 'saves' / sidecar.name).write_text(json.dumps(data))
        shutil.copy2(source / 'ce-playthroughs.json', root / 'saves')
        client.launch(game, 'apple2', game='nox', engine='nox')
        client.command('wait', field='adapter.stage', equals='title', timeout_ms=10000)
        client.command('load', chooser_slot=select_save(client, 0)['chooser_slot'])
        client.command('wait', field='adapter.stage', equals='world', timeout_ms=5000)
        return client
    except BaseException:
        client.close()
        raise


def menu(client):
    client.command('input', kind='key', key=286, flags=1, map_physical=True, phase='down')
    client.command('wait', field='scummvm_dialog', equals=True, timeout_ms=4000)
    client.command('input', kind='key', key=286, flags=1, map_physical=True, phase='up')


def click(client, x, y):
    client.command('input', kind='mouse_move', x=x, y=y)
    for phase in ('down', 'up'):
        client.command('input', kind='mouse_button', button=1, x=x, y=y, phase=phase)


def restart(client):
    menu(client)
    click(client, 248, 199)  # Restart Battle, native 560 x 384 CE overlay.
    client.command('wait', field='scummvm_dialog', equals=False, timeout_ms=4000)
    client.command('wait', field='adapter.stage', equals='world', timeout_ms=4000)


def disable_during_battle(client):
    menu(client)
    client.command('input', kind='key', key=111, ascii=111, phase='down')
    client.command('input', kind='key', key=111, ascii=111, phase='up')
    click(client, 150, 153)  # Party Invulnerability on the Cheats tab.
    click(client, 374, 281)  # Apply the option and return to the Global Menu.
    client.command('wait', field='adapter.can_restart_encounter', equals=False, timeout_ms=4000)
    client.command('input', kind='key', key=27, ascii=27, phase='down')
    client.command('wait', field='scummvm_dialog', equals=False, timeout_ms=4000)
    client.command('input', kind='key', key=27, ascii=27, phase='up')
    assert inspect(client)['adapter']['stage'] == 'battle'


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ('exe', 'game', 'extra', 'artifacts'):
        ap.add_argument('--' + name, type=Path, required=True)
    args = ap.parse_args()
    game, root, exe, extra = (getattr(args, name).resolve() for name in ('game', 'artifacts', 'exe', 'extra'))
    root.mkdir()
    original = {p: digest(p) for p in game.glob('*.HDV')}
    conv = game / 'conversion'
    rat = conv / 'ce-m4-native-full247-walk/dismiss-and-wait/saves/nox-native-exploration-final.bin'
    shadow = conv / 'ce-m4-shadow384b/flag7-controlled-before-West/saves/nox-native-exploration-final.bin'
    for mode in (0, 1):
        for cheat in (False, True):
            work = root / f'shadow-{mode}-{cheat}'
            client = launch(exe, work, game, extra, shadow, cheat, mode)
            try:
                press(client, 276)
                client.command('wait', field='adapter.stage', equals='battle', timeout_ms=5000)
                initial = inspect(client)['adapter']
                assert (initial['x'], initial['y']) == (150, 168)
                assert initial['enemies'][0]['hp'] == 3210 and not initial['can_restart_encounter']
                if mode == 0:
                    press(client, 32, 32)  # Original Vortex response to a pass.
                final = client.command('wait', field='adapter.battle_phase', equals=2, timeout_ms=42000)['adapter']
                assert not final['can_restart_encounter']
                assert all(member['hp'] == 0 for member in final['battle_party'])
                assert final['quest_fields'] == initial['quest_fields']
                if cheat:
                    assert final['battle_party'] == defeat['battle_party'] and final['enemies'] == defeat['enemies']
                defeat = final
            finally:
                client.close()
    print('PASS original Dark Shadow admission and terminal loss with cheat off and on in both modes', flush=True)

    # Use the retained six-member Quick fixture and its source-supported
    # sprite 252 contact, but start at the actual command before admission.
    # Low physical power keeps the encounter pending until a party spell.
    original_rat = rat.read_bytes()
    source = conv / 'ce-m4-quick-enemy294-source/enemy-magic-controlled-before-input.bin'
    records = sections(source.read_bytes())['CORE']
    source_party = records.index(b'NPAR') + 6
    pieces = sections(original_rat)
    core = bytearray(pieces['CORE'])
    party = core.index(b'NPAR') + 6
    core[party:party + 768] = records[source_party:source_party + 768]
    for i in range(6):
        member = party + i * 128
        core[member + 1] = 2
        core[member + 4] = core[member + 18]
        core[member + 9] = 1
        core[member + 10] = min(1, core[member + 10])
        core[member + 17] = core[member + 34] = 0
    contact = core.index(b'NWLD') + 271 + 774
    assert core[contact] == 164
    core[contact] = 252
    pieces['CORE'] = core
    quick_fixture = root / 'quick-mana-before-input.bin'
    quick_fixture.write_bytes(encode_checkpoint(original_rat, pieces))
    for mode in (0, 1):
        work = root / f'restart-{mode}'
        client = launch(exe, work, game, extra, quick_fixture if mode else rat, True, mode)
        try:
            base = inspect(client)
            assert not base['adapter']['can_restart_encounter']
            assert 'nox.party_invulnerability' in base['ce']['assistance']
            before = save_state(client, work, 101)
            press(client, 273)
            client.command('wait', field='adapter.can_restart_encounter', equals=True, timeout_ms=5000)
            if mode == 0:
                for key in (99, 49, 13):  # C, readied Lesser Heal on key 1, self.
                    press(client, key, key)
                assert inspect(client)['adapter']['battle_party'][0]['mp'] == 6
            else:
                initial_mana = [member['mp'] for member in base['adapter']['party']]
                deadline = time.monotonic() + 30
                while time.monotonic() < deadline:
                    battle = inspect(client)['adapter']
                    assert battle['battle_phase'] == 0
                    if any(member['mp'] < mp for member, mp in zip(battle['battle_party'], initial_mana)):
                        break
                    time.sleep(0.1)
                else:
                    raise AssertionError('Quick party did not spend mana')
                client.command('wait', field='adapter.quick_holding', equals=True, timeout_ms=8000)
            assert inspect(client)['adapter']['battle_actions'] > 0
            restart(client)
            after = save_state(client, work, 102)
            for tag in ('CORE', 'WIND', 'TURN', 'ROUT', 'RAND', 'INPT', 'BTTL', 'DISC'):
                assert before[tag] == after[tag], (mode, tag)
            now = inspect(client)
            assert now['session'] == base['session'] and now['ce']['assistance'] == base['ce']['assistance']
            assert not now['adapter']['can_restart_encounter'] and 'battle_actions' not in now['adapter']
            press(client, 273)
            client.command('wait', field='adapter.can_restart_encounter', equals=True, timeout_ms=5000)
            if mode == 0:
                disable_during_battle(client)
                state = inspect(client)
                assert state['cheats'] and state['ce']['assistance'] == base['ce']['assistance']
            client.command('load', chooser_slot=select_save(client, 101)['chooser_slot'])
            client.command('wait', field='adapter.stage', equals='world', timeout_ms=4000)
            assert not inspect(client)['adapter']['can_restart_encounter']
        finally:
            client.close()
        print('PASS actual Global Menu restart in', 'Quick' if mode else 'Tactical', flush=True)
    work = root / 'quick-victory'
    client = launch(exe, work, game, extra, rat, True, 1)
    try:
        press(client, 273)
        client.command('wait', field='adapter.battle_phase', equals=1, timeout_ms=10000)
        assert not inspect(client)['adapter']['can_restart_encounter']
    finally:
        client.close()
    print('PASS restart is absent on Quick victory', flush=True)
    work = root / 'ordinary'
    client = launch(exe, work, game, extra, rat, False, 0)
    try:
        press(client, 273)
        client.command('wait', field='adapter.stage', equals='battle', timeout_ms=5000)
        assert not inspect(client)['adapter']['can_restart_encounter']
        menu(client)
        assert inspect(client)['scummvm_dialog']
        client.command('screenshot')
        client.command('input', kind='key', key=27, ascii=27, phase='down')
        client.command('wait', field='scummvm_dialog', equals=False, timeout_ms=4000)
        client.command('input', kind='key', key=27, ascii=27, phase='up')
        assert inspect(client)['adapter']['stage'] == 'battle'
    finally:
        client.close()
    assert all(digest(path) == value for path, value in original.items())
    print('PASS ordinary fight has no restart action', flush=True)
    (root / 'verification.json').write_text(json.dumps(dict(exe_sha256=digest(exe), rat_sha256=digest(rat),
        shadow_sha256=digest(shadow), quick_fixture_sha256=digest(quick_fixture), original_hdvs={str(p): v for p, v in original.items()}), indent=2))


if __name__ == '__main__':
    main()
