#!/usr/bin/env python3
"""Bounded native slot/shortcut/autosave tests; original media stays read-only."""
import argparse
import json
from pathlib import Path
import shutil
import time
from test_snatcher_integration import Client, sha

def launch(c, game):
    c.command('launch', engine='snatcher', game='snatcher', platform='segacd', path=str(game))
    c.command('wait', field='adapter.ready', equals=True, timeout_ms=30000)

def listings(c):
    return {s['chooser_slot']: s for s in c.command('saves')['saves']}

def key(c, code, phase):
    c.command('input', kind='key', key=code, phase=phase, map_physical=True)

def migration(a, folder):
    folder.mkdir()
    root = folder / 'profile'
    c = Client(a.legacy_exe, root, a.game, 'gens')
    c.label = 'legacy'
    try:
        launch(c, a.game)
        owner = c.command('session_create', title='Existing Session')['session']
        for slot in (0, 0, 1, 4, 5):
            c.command('save', slot=slot, description='Preserve existing slot ' + str(slot))
        before = {p.name: sha(p) for p in (root / 'saves').glob('cli-game.*')}
    finally:
        c.close()
    cfg = root / 'profile.ini'
    cfg.write_text(cfg.read_text().replace('[cli-game]', '[cli-game]\nsave_slot=0'))
    c = Client(a.exe, root, a.game, 'gens', reuse=True)
    c.label = 'migrated'
    try:
        launch(c, a.game)
        for old, new in ((0, 6), (1, 7), (4, 8)):
            for suffix in ('', '.bak', '.ce-achievements.json'):
                name = f'cli-game.{old:03d}{suffix}'
                if name in before:
                    assert sha(root / 'saves' / f'cli-game.{new:03d}{suffix}') == before[name]
            assert listings(c)[new]['session'] == owner
        assert sha(root / 'saves' / 'cli-game.005') == before['cli-game.005']
        assert c.command('inspect')['session'] == owner
        policy = c.command('input', kind='logical', action='save_policy')['slots']
        assert all(not x['manual_allowed'] and x['write_protected'] for x in policy[:5])
        assert policy[5]['manual_allowed']
        for slot in range(5):
            try:
                c.command('save', slot=slot, description='Forbidden manual write')
                raise RuntimeError('Reserved slot accepted a manual write')
            except AssertionError as e:
                assert 'reserved' in str(e).lower(), e
        # Installed keymap actions reach the real engine key handler and modal GUI.
        for code, field in ((286, 'save_dialog_open'), (288, 'load_dialog_open')):
            key(c, code, 'down')
            c.command('wait', field='adapter.' + field, equals=True, timeout_ms=5000)
            key(c, code, 'up')
            key(c, 27, 'down')
            key(c, 27, 'up')
            c.command('wait', field='adapter.' + field, equals=False, timeout_ms=5000)
        c.command('input', kind='logical', action='autosave_now')
        assert listings(c)[0]['session'] == owner
        assert (root / 'saves' / 'cli-game.000.autosave').exists()
        auto = sha(root / 'saves' / 'cli-game.000')
        c.command('load', slot=0)
        assert sha(root / 'saves' / 'cli-game.000') == auto, 'Loading autosave replaced itself'
    finally:
        c.close()
    c = Client(a.exe, root, a.game, 'gens', reuse=True)
    c.label = 'migration-cold'
    try:
        launch(c, a.game)
        assert listings(c)[6]['session'] == owner
        assert not (root / 'saves' / 'cli-game.009').exists(), 'A genuine autosave was migrated again'
        c.command('load', slot=7)
        assert c.command('inspect')['session'] == owner
    finally:
        c.close()

def periodic(a, folder):
    folder.mkdir()
    c = Client(a.exe, folder / 'profile', a.game, 'gens')
    try:
        launch(c, a.game)
        c.command('save', slot=90, description='Untracked before session')
        c.command('input', kind='logical', action='copy_to_free_slot_probe')
        assert (c.root / 'saves' / 'cli-game.005').exists()
        assert all(not (c.root / 'saves' / f'cli-game.{slot:03d}').exists() for slot in range(1, 5))
        owner = c.command('session_create', title='Periodic autosave')['session']
        count = c.command('inspect')['adapter']['autosaves_created']
        c.command('wait', field='adapter.autosaves_created', equals=count + 1, timeout_ms=10000)
        assert listings(c)[0]['session'] == owner
        c.command('save', slot=5, description='Protected manual')
        before = sha(c.root / 'saves' / 'cli-game.005')
        c.command('input', kind='logical', action='autosave_now')
        assert sha(c.root / 'saves' / 'cli-game.005') == before
        other = c.command('session_create', title='Other session')['session']
        assert other != owner
        try:
            c.command('save', slot=5, description='Must not overwrite another session')
            raise RuntimeError('Another session overwrote the manual save')
        except AssertionError as e:
            assert 'another Play Session' in str(e), e
        assert sha(c.root / 'saves' / 'cli-game.005') == before
        c.command('session_select', session=owner)
        count = c.command('inspect')['adapter']['autosaves_created']
        c.command('load', slot=5)
        assert c.command('inspect')['adapter']['autosaves_created'] > count
        c.command('load', slot=90)
        assert c.command('inspect')['session'] == ''
        c.command('input', kind='logical', action='autosave_now')
        assert listings(c)[0]['session'] == '', 'Untracked autosave inherited an old owner'
    finally:
        c.close()

def internal(a, folder):
    folder.mkdir()
    root = folder / 'profile'
    c = Client(a.exe, root, a.game, 'gens')
    (root / 'saves').mkdir(exist_ok=True)
    assert a.checkpoint.read_bytes()[:4] == b'SGR2'
    shutil.copyfile(a.checkpoint, root / 'saves' / 'cli-game-snatcher-original-1.sgr')
    cfg = root / 'profile.ini'
    cfg.write_text(cfg.read_text().replace('autosave_period=1', 'autosave_period=300'))
    def state():
        s = c.command('inspect')['adapter']
        assert not any(s.get(k) for k in ('blocker', 'runtime_error', 'render_error', 'audio_error', 'startup_error')), s
        return s
    def tap(code):
        for phase in ('down', 'up'):
            key(c, code, phase)
            c.command('advance', frames=2)
    def ready():
        for _ in range(160):
            s = state()
            if s.get('command_ready') and s.get('mouse_ready') and s.get('choice_references'):
                return s
            if s.get('dialogue_waiting') and s.get('runtime_status') == 4:
                tap(13)
            c.command('advance', frames=16)
        raise AssertionError('Ordinary menu did not become ready')
    def pick(reference):
        s = ready()
        refs = s['choice_references']
        assert reference in refs, (reference, refs)
        current = refs.index(s['command_reference'])
        for _ in range((refs.index(reference) - current) % len(refs)):
            tap(274)
        tap(13)
        c.command('advance', frames=16)
        return ready()
    try:
        launch(c, a.game)
        owner = c.command('session_create', title='Metal Gear saves')['session']
        for slot in range(1, 5):
            c.command('load', slot=1)
            c.command('wait', field='adapter.original_resume_pending', equals=False, timeout_ms=15000)
            c.command('session_select', session=owner)
            s = ready()
            assert s['resource'] == 67
            pick(551)  # Talk to Metal Gear through the actual authored menu.
            choices = pick(430)['choice_references']
            assert len(choices) >= 4, choices
            after = pick(choices[slot - 1])
            if 430 in after['choice_references'] and 142 in after['choice_references']:
                pick(430)  # Authored overwrite confirmation for the inherited slot.
            path = root / 'saves' / f'cli-game.{slot:03d}'
            assert path.read_bytes()[:4] == b'SGR2'
            saved = listings(c)[slot]
            assert saved['description'] == f'Internal Save Slot {slot}'
            assert saved['session'] == owner
        for slot in range(1, 5):
            c.command('load', slot=slot)
            c.command('wait', field='adapter.original_resume_pending', equals=False, timeout_ms=15000)
            assert c.command('inspect')['session'] == owner
            assert ready()['resource'] == 67
    finally:
        c.close()
    c = Client(a.exe, root, a.game, 'gens', reuse=True)
    c.label = 'internal-cold'
    try:
        launch(c, a.game)
        c.command('load', slot=4)
        c.command('wait', field='adapter.original_resume_pending', equals=False, timeout_ms=15000)
        assert c.command('inspect')['session'] == owner
    finally:
        c.close()

    # A recoverable older internal generation must not inherit its corrupt
    # primary's session, and startup migration must leave that backup in place.
    primary = root / 'saves' / 'cli-game.001'
    backup = root / 'saves' / 'cli-game.001.bak'
    shutil.copy2(primary, backup)
    backup.with_name(backup.name + '.ce-achievements.json').unlink(missing_ok=True)
    backup_hash = sha(backup)
    primary.write_bytes(b'corrupt owned internal generation')
    c = Client(a.exe, root, a.game, 'gens', reuse=True)
    c.label = 'internal-backup-cold'
    try:
        launch(c, a.game)
        assert 1 in listings(c)
        c.command('load', slot=1)
        c.command('wait', field='adapter.original_resume_pending', equals=False, timeout_ms=15000)
        assert c.command('inspect')['session'] == '', 'Internal backup inherited primary owner'
        assert sha(backup) == backup_hash
        assert primary.read_bytes() == b'corrupt owned internal generation'
    finally:
        c.close()

def main():
    p = argparse.ArgumentParser()
    for name in ('exe', 'legacy-exe', 'game', 'checkpoint', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    a.output.mkdir()
    migration(a, a.output / 'migration')
    periodic(a, a.output / 'periodic')
    internal(a, a.output / 'internal')
    (a.output / 'receipt.json').write_text(json.dumps(dict(status='PASS',
        executable_sha256=sha(a.exe), legacy_executable_sha256=sha(a.legacy_exe),
        checkpoint_sha256=sha(a.checkpoint), migration=True, session_ownership=True,
        f5_f7=True, reserved_slots_protected=True, periodic_and_preload_autosave=True,
        metal_gear_slots=[1, 2, 3, 4], cold_restore=True,
        internal_backup_does_not_inherit_primary_owner=True, real_devices=False), indent=2))

if __name__ == '__main__':
    main()
