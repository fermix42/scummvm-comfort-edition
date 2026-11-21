#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check original v119 Quick Combat's pause and next-key acknowledgement."""
import argparse
import configparser
from pathlib import Path

from test_encounter_restart import inspect, launch, press
from test_exploration_map import shot


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('exe', 'game', 'extra', 'artifacts'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    fixture = args.game / 'conversion/ce-m4-native-full247-walk/dismiss-and-wait/saves/nox-native-exploration-final.bin'
    work = args.artifacts.resolve()
    client = launch(args.exe.resolve(), work, args.game.resolve(),
                    args.extra.resolve(), fixture, False, 0)
    try:
        for key in (9, 9, 54):
            press(client, key, key)
        assert inspect(client)['adapter']['inventory_tab'] == 5
        for expected in (1, 2, 0, 1):
            press(client, 13, 13)
            profile = configparser.ConfigParser()
            profile.read(work / 'profile.ini')
            assert profile.getint('cli-game', 'nox_combat_mode') == expected
        shot(client, work).replace(work / 'settings-quick-on.png')
        press(client, 27, 27)
        press(client, 273)
        client.command('wait', field='adapter.quick_holding', equals=True, timeout_ms=8000)
        press(client, 56, 56)
        frozen = inspect(client)['adapter']
        assert frozen['quick_paused']
        shot(client, work).replace(work / 'quick-early.png')
        client.command('advance', frames=30)
        still = inspect(client)['adapter']
        for field in ('quick_remaining', 'quick_events_shown', 'battle_actions'):
            assert still[field] == frozen[field], field
        press(client, 32, 32)
        resumed = inspect(client)['adapter']
        assert not resumed['quick_paused'], 'Space must resume Quick Combat'
        press(client, 56, 56)
        assert inspect(client)['adapter']['quick_paused']
        press(client, 61, 61)
        resumed = inspect(client)['adapter']
        assert not resumed['quick_paused'] and resumed['quick_scroll_speed'] == 3
        press(client, 61, 61)
        assert inspect(client)['adapter']['quick_scroll_speed'] == 2
        for _ in range(50):
            current = inspect(client)['adapter']
            if current['quick_events_shown'] == frozen['quick_events_shown'] and current['quick_remaining'] + 4 < frozen['quick_remaining']:
                break
            client.command('advance', frames=1)
        else:
            raise AssertionError('No later characters appeared in the first disk-backed cry')
        shot(client, work).replace(work / 'quick-later.png')
        print('PASS original Quick Combat next-key resume and separate speed changes')
    finally:
        client.close()


if __name__ == '__main__':
    main()
