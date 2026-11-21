#!/usr/bin/env python3
"""Real native menu selection from a retained diagnostic predecessor."""
import argparse
import json
from pathlib import Path
import shutil
from test_snatcher_integration import Client, sha

def main():
    p = argparse.ArgumentParser()
    for name in ('exe', 'game', 'checkpoint', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    a.output.mkdir()
    c = Client(a.exe, a.output / 'profile', a.game, 'gens')
    try:
        saves = c.root / 'saves'
        saves.mkdir(exist_ok=True)
        shutil.copyfile(a.checkpoint, saves / 'cli-game.056')
        c.command('launch', engine='snatcher', game='snatcher', platform='segacd', path=str(a.game))
        c.command('wait', field='adapter.ready', equals=True, timeout_ms=30000)
        c.command('load', slot=56)
        c.command('wait', field='adapter.command_ready', equals=True, timeout_ms=10000)
        before = c.command('inspect')['adapter']
        assert before['choices'] > 1
        for phase in ('down', 'up'):
            c.command('input', kind='button', button=13, phase=phase, map_physical=True)
            c.command('advance', frames=2)
        selected = c.command('inspect')['adapter']
        assert selected['command_reference'] != before['command_reference']
        for phase in ('down', 'up'):
            c.command('input', kind='key', key=273, phase=phase, map_physical=True)
            c.command('advance', frames=2)
        restored = c.command('inspect')['adapter']
        assert restored['command_reference'] == before['command_reference']
        assert not any(restored.get(k) for k in ('blocker', 'runtime_error', 'audio_error', 'render_error'))
        c.command('save', slot=5, description='Ordinary menu input boundary')
        c.command('input', kind='logical', action='autosave_now')
        protected = sha(saves / 'cli-game.005')
        c.command('save', slot=99, description='Manual last slot from real menu')
        c.command('load', slot=99)
        assert sha(saves / 'cli-game.005') == protected
    finally:
        c.close()
    c = Client(a.exe, a.output / 'profile', a.game, 'gens', reuse=True)
    try:
        c.command('launch', engine='snatcher', game='snatcher', platform='segacd', path=str(a.game))
        c.command('wait', field='adapter.ready', equals=True, timeout_ms=30000)
        c.command('load', slot=5)
        cold = c.command('inspect')['adapter']
        for field in ('resource', 'node', 'command_reference', 'choice_references', 'packed_digest'):
            assert cold[field] == restored[field], (field, cold[field], restored[field])
        c.command('input', kind='logical', action='autosave_now')
        assert sha(saves / 'cli-game.005') == protected
    finally:
        c.close()
    (a.output / 'receipt.json').write_text(json.dumps(dict(status='PASS',
        executable_sha256=sha(a.exe), checkpoint_sha256=sha(a.checkpoint),
        original_reference=before['command_reference'], selected_reference=selected['command_reference'],
        restored_reference=restored['command_reference'], cold_menu_exact=True,
        manual_last_slot_loaded=True, real_devices=False), indent=2))

if __name__ == '__main__':
    main()
