#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the exploration map cheat in surface, interior and dungeon saves."""
import argparse
import json
from pathlib import Path

from test_encounter_restart import Client, digest, inspect, launch, press, save_state, select_save


def shot(client, work):
    screenshots = work / 'screenshots'
    before = set(screenshots.glob('*.png'))
    client.command('screenshot')
    new = set(screenshots.glob('*.png')) - before
    assert len(new) == 1, new
    return new.pop()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('exe', 'game', 'extra', 'dungeon', 'artifacts'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    root, exe, game, extra = (getattr(args, name).resolve() for name in ('artifacts', 'exe', 'game', 'extra'))
    root.mkdir()
    conversion = game / 'conversion'
    fixtures = {
        'surface': conversion / 'ce-m4-projectile365-world/unprotected-source-shooter-before-input/saves/nox-native-exploration-final.bin',
        'interior': conversion / 'ce-m4-native-full247-walk/dismiss-and-wait/saves/nox-native-exploration-final.bin',
        'dungeon': args.dungeon.resolve(),
        'ruin': conversion / 'ce-m4-shadow384b/flag0-actual-West-authored-entrance/saves/nox-native-exploration-final.bin',
    }
    info = {'exe_sha256': digest(exe), 'cases': {}}
    for area, fixture in fixtures.items():
        states, viewports = {}, {}
        for enabled in (False, True):
            work = root / f'{area}-{enabled}'
            options = {'ce_cheats_enabled': 'true', 'nox_reveal_exploration_map': 'true'} if enabled else {}
            client = launch(exe, work, game, extra, fixture, False, 0, **options)
            try:
                before = save_state(client, work, 101)
                state = inspect(client)
                adapter = state['adapter']
                assert adapter['map_reveal'] == enabled
                assert ('nox.reveal_exploration_map' in state['ce']['assistance']) == enabled
                viewport = shot(client, work)
                viewports[enabled] = digest(viewport)
                press(client, 109, 109, map_physical=True)
                assert inspect(client)['adapter']['map_overview'] == enabled
                if enabled:
                    diagram = shot(client, work)
                    assert digest(diagram) != digest(viewport)
                    press(client, 273)
                    position = inspect(client)['adapter']
                    assert (position['x'], position['y']) == (adapter['x'], adapter['y'])
                    press(client, 27, 27)
                    assert not inspect(client)['adapter']['map_overview']
                after = save_state(client, work, 102)
                for tag in ('CORE', 'RAND', 'SCEN', 'INPT', 'BTTL', 'DISC'):
                    assert before[tag] == after[tag], (area, enabled, tag)
                states[enabled] = before
                if area == 'interior' and enabled:
                    press(client, 273)
                    client.command('wait', field='adapter.stage', equals='battle', timeout_ms=5000)
                    press(client, 109, 109, map_physical=True)
                    assert not inspect(client)['adapter']['map_overview']
            finally:
                client.close()
            if enabled:
                # Remove the individual cheat from the disposable profile and
                # reload the older save in a new process.
                profile = work / 'profile.ini'
                profile.write_text(profile.read_text().replace('nox_reveal_exploration_map=true',
                                                                'nox_reveal_exploration_map=false'))
                client = Client(exe, work, extra, transcript=root / f'{area}-cold.jsonl')
                try:
                    client.launch(game, 'apple2', game='nox', engine='nox')
                    client.command('wait', field='adapter.stage', equals='title', timeout_ms=10000)
                    client.command('load', chooser_slot=select_save(client, 0)['chooser_slot'])
                    client.command('wait', field='adapter.stage', equals='world', timeout_ms=5000)
                    state = inspect(client)
                    assert not state['adapter']['map_reveal'] and not state['adapter']['map_overview']
                    assert 'nox.reveal_exploration_map' in state['ce']['assistance']
                finally:
                    client.close()
        for tag in ('CORE', 'RAND', 'SCEN', 'INPT', 'BTTL', 'DISC'):
            assert states[False][tag] == states[True][tag], (area, tag)
        assert viewports[False] != viewports[True], area
        info['cases'][area] = {'fixture_sha256': digest(fixture), 'hidden_cells': sum(states[False]['SCEN'][750:937]),
                               'world_bytes_match': True, 'cold_assistance_retained': True}
        assert info['cases'][area]['hidden_cells']
        print(f'PASS {area} viewport, full map, ordinary save state and cold assistance', flush=True)
    (root / 'verification.json').write_text(json.dumps(info, indent=2) + '\n')


if __name__ == '__main__':
    main()
