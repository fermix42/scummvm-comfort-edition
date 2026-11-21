#!/usr/bin/env python3
"""Real Snatcher save ownership and cold session restoration, dummy devices only."""
import argparse
import json
from pathlib import Path
from test_snatcher_integration import Client, sha

def main():
    p = argparse.ArgumentParser()
    for name in ('exe', 'game', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    a.output.mkdir()
    root = a.output / 'profile'
    c = Client(a.exe, root, a.game, 'gens')
    def launch():
        c.command('launch', engine='snatcher', game='snatcher', platform='segacd', path=str(a.game))
        c.command('wait', field='adapter.ready', equals=True, timeout_ms=30000)
    def owners():
        return {s['chooser_slot']: s['session'] for s in c.command('saves')['saves']}
    try:
        launch()
        c.command('save', slot=90, description='Legacy untracked')
        assert owners()[90] == ''
        one = c.command('session_create', title='Session 1')['session']
        assert one
        c.command('save', slot=5, description='Session one manual zero')
        assert owners()[5] == one, 'Saved under Session 1 but listed as untracked'
        two = c.command('session_create', title='Session 2')['session']
        assert two and two != one
        c.command('save', slot=6, description='Session two')
        assert owners()[6] == two
        c.command('load', slot=5)
        assert c.command('inspect')['session'] == one, 'Loading did not restore Session 1'
        c.command('save', slot=7, description='Continued Session one')
        assert owners()[7] == one
        before = {p.name: sha(p) for p in (root / 'saves').glob('cli-game.005*')}
        c.command('input', kind='logical', action='autosave_now')
        assert before == {p.name: sha(p) for p in (root / 'saves').glob('cli-game.005*')}
        c.command('load', slot=90)
        assert c.command('inspect')['session'] == '', 'Untagged save inherited a session'
        assert owners()[5] == one and owners()[6] == two
        c.command('session_select', session=two)
    finally:
        c.close()
    c = Client(a.exe, root, a.game, 'gens', reuse=True)
    try:
        launch()
        assert owners()[5] == one and owners()[6] == two and owners()[90] == ''
        c.command('load', slot=5)
        assert c.command('inspect')['session'] == one
        c.command('save', slot=8, description='Cold continued Session one')
        assert owners()[8] == one
        c.command('load', slot=6)
        assert c.command('inspect')['session'] == two
        c.command('save', slot=9, description='Session two backup fixture')
        c.command('save', slot=9, description='Session two replaces its own primary')
        assert owners()[9] == two
    finally:
        c.close()
    # A fallback generation must not inherit the newer primary's owner.
    backup = root / 'saves' / 'cli-game.009.bak'
    backup_hash = sha(backup)
    (root / 'saves' / 'cli-game.009').write_bytes(b'corrupt owned test fixture')
    c = Client(a.exe, root, a.game, 'gens', reuse=True)
    c.label = 'profile-backup-cold'
    try:
        launch()
        c.command('load', slot=9)
        assert c.command('inspect')['session'] == '', 'Backup inherited the primary session'
        assert sha(backup) == backup_hash
    finally:
        c.close()
    (a.output / 'receipt.json').write_text(json.dumps(dict(status='PASS',
        executable_sha256=sha(a.exe), session_one=one, session_two=two,
        created_save_owned=True, loaded_session_restored=True, cold_restore=True,
        untracked_preserved=True, autosave_protected=True,
        backup_does_not_inherit_primary_owner=True), indent=2))

if __name__ == '__main__':
    main()
