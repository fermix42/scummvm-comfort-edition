#!/usr/bin/env python3
"""Bounded option qualification with an external, owner-supplied sidecar."""
import argparse
import json
from pathlib import Path
from test_snatcher_integration import Client, sha

def main():
    p = argparse.ArgumentParser()
    for name in ('exe', 'game', 'sidecar', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    a.output.mkdir()
    original = sha(a.sidecar)
    for label, decensor, controls in [('original-controls', False, True), ('decensor', True, False)]:
        root = a.output / label
        c = Client(a.exe, root, a.game, 'gens')
        try:
            profile = root / 'profile.ini'
            text = profile.read_text().replace('snatcher_decensor=false', 'snatcher_decensor=' + str(decensor).lower())
            text = text.replace('[scummvm]', '[scummvm]\nextrapath=' + str(a.sidecar.parent))
            text += '\nsnatcher_original_controls_only=' + str(controls).lower() + '\n'
            profile.write_text(text)
            c.command('launch', engine='snatcher', game='snatcher', platform='segacd', path=str(a.game))
            c.command('wait', field='adapter.ready', equals=True, timeout_ms=30000)
            state = c.command('inspect')['adapter']
            assert state['decensor_enabled'] == decensor and state['mouse_enabled'] != controls
            # Common::JOYSTICK_BUTTON_BACK is 5; the installed JOY_BACK binding
            # must reach the ordinary pause key through the real keymapper.
            for expected in (True, False):
                c.command('input', kind='button', button=5, phase='down', map_physical=True)
                c.command('advance', frames=1)
                c.command('input', kind='button', button=5, phase='up', map_physical=True)
                assert c.command('inspect')['adapter']['input_paused'] == expected
                c.command('input', kind='key', key=112, ascii=112, phase='down', repeat=True, map_physical=True)
                c.command('advance', frames=1)
                assert c.command('inspect')['adapter']['input_paused'] == expected
                c.command('input', kind='key', key=112, ascii=112, phase='up', map_physical=True)
            if decensor:
                c.command('input', kind='logical', action='decensor_resource_inventory')
            c.command('input', kind='logical', action='autosave_policy_probe')
            c.command('save', slot=0, description='Options manual zero')
            c.command('load', slot=0)
        finally:
            c.close()
    assert sha(a.sidecar) == original
    (a.output / 'receipt.json').write_text(json.dumps(dict(status='PASS',
        executable_sha256=sha(a.exe), external_sidecar_sha256=original,
        sidecar_copied=False, real_devices=False), indent=2))

if __name__ == '__main__':
    main()
