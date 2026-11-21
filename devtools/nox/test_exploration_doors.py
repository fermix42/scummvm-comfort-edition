#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Capture an authored ruin door while obscured, and check its normal interaction."""
import argparse
import json
from pathlib import Path
import struct

from test_encounter_restart import digest, inspect, launch, press, save_state
from test_exploration_map import shot
from test_native_world_input import current_world_tables


def door(parts):
    core = parts['CORE']
    length = struct.unpack_from('<I', core, 6)[0]
    tables = current_world_tables(core[10:10 + length])
    return list(tables[1024 + 160:1028 + 160])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('exe', 'game', 'extra', 'artifacts'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    root, exe, game, extra = (getattr(args, name).resolve() for name in ('artifacts', 'exe', 'game', 'extra'))
    root.mkdir()
    fixture = game / 'conversion/ce-m4-shadow384b/flag0-actual-West-authored-entrance/saves/nox-native-exploration-final.bin'
    results = {'exe_sha256': digest(exe), 'fixture_sha256': digest(fixture)}
    for enabled in (False, True):
        work = root / str(enabled)
        options = {'ce_cheats_enabled': 'true', 'nox_reveal_exploration_map': 'true'} if enabled else {}
        client = launch(exe, work, game, extra, fixture, False, 0, **options)
        try:
            a = inspect(client)['adapter']
            assert (a['map_type'], a['map'], a['x'], a['y']) == (3, 31, 24, 38)
            before = save_state(client, work, 101)
            assert door(before) == [20, 37, 92, 16]
            assert before['SCEN'][750 + 93 - 4 - 17] == 1
            captures = {'distant': str(shot(client, work))}
            if enabled:
                press(client, 109, 109)
                assert inspect(client)['adapter']['map_overview']
                captures['overview'] = str(shot(client, work))
                press(client, 27, 27)
                after_map = save_state(client, work, 102)
                for tag in ('CORE', 'INPT', 'SCEN', 'RAND', 'DISC'):
                    assert before[tag] == after_map[tag], tag
            for _ in range(4):
                press(client, 276)
            a = inspect(client)['adapter']
            assert (a['x'], a['y']) == (20, 38)
            press(client, 273)
            a = inspect(client)['adapter']
            assert (a['x'], a['y']) == (20, 38)
            blocked = save_state(client, work, 103)
            assert door(blocked) == [20, 37, 92, 16]
            captures['adjacent'] = str(shot(client, work))
            press(client, 111, 111)
            press(client, 273)
            after_open = save_state(client, work, 104)
            assert door(after_open) == [20, 37, 92, 16]
            press(client, 273)
            a = inspect(client)['adapter']
            assert (a['x'], a['y']) == (20, 38)
            results[str(enabled)] = captures
        finally:
            client.close()
    (root / 'verification.json').write_text(json.dumps(results, indent=2) + '\n')
    print('PASS authored Ruin 31 door state, blocked movement, normal opening refusal with cheat off and on')


if __name__ == '__main__':
    main()
