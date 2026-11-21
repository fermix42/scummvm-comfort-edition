#!/usr/bin/env python3
"""Verify approved SP25 save rebinding in both option directions, SDL dummy only."""
import argparse
import json
from pathlib import Path
import shutil
from test_snatcher_integration import Client, sha

def main():
    parser = argparse.ArgumentParser()
    for name in ('exe', 'game', 'sidecar', 'checkpoint', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir()
    root = args.output / 'profile'
    original_hash = sha(args.checkpoint)
    expected = None
    # The supplied checkpoint was made with the approved decensor option on.
    # Save under each option, then cold load under its opposite.
    for index, enabled in enumerate((True, False, True)):
        client = Client(args.exe, root, args.game, 'gens', reuse=index > 0)
        client.label = f'variant-{index}'
        try:
            profile = root / 'profile.ini'
            text = profile.read_text()
            text = text.replace('snatcher_decensor=false', 'snatcher_decensor=true') if enabled else text.replace('snatcher_decensor=true', 'snatcher_decensor=false')
            if not index:
                text = text.replace('[scummvm]', '[scummvm]\nextrapath=' + str(args.sidecar.parent))
                (root / 'saves').mkdir(exist_ok=True)
                shutil.copy2(args.checkpoint, root / 'saves/cli-game.005')
            profile.write_text(text)
            client.command('launch', engine='snatcher', game='snatcher', platform='segacd', path=str(args.game))
            client.command('wait', field='adapter.ready', equals=True, timeout_ms=30000)
            client.command('load', slot=5 + index)
            state = client.command('inspect')['adapter']
            assert state['decensor_enabled'] == enabled
            actual = {key: state[key] for key in ('resource', 'node', 'packed_digest', 'scene_resource', 'scene_index')}
            if expected is None:
                expected = actual
            assert actual == expected, (actual, expected)
            client.command('save', slot=6 + index, description='Opposite option compatibility')
        finally:
            client.close()
    assert sha(args.checkpoint) == original_hash
    (args.output / 'receipt.json').write_text(json.dumps(dict(status='PASS',
        executable_sha256=sha(args.exe), checkpoint_sha256=original_hash,
        enabled_to_disabled=True, disabled_to_enabled=True,
        narrative_and_location_preserved=True, real_devices=False), indent=2))

if __name__ == '__main__':
    main()
