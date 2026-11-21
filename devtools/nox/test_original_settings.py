#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check v119 Settings through the normal inventory and portable profile."""
import argparse
import configparser
from pathlib import Path

from test_encounter_restart import Client, inspect, launch, press, select_save
from test_exploration_map import shot


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('exe', 'game', 'extra', 'artifacts'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    exe, game, extra, work = (getattr(args, name).resolve() for name in ('exe', 'game', 'extra', 'artifacts'))
    fixture = game / 'conversion/ce-m4-native-full247-walk/dismiss-and-wait/saves/nox-native-exploration-final.bin'

    def profile():
        cfg = configparser.ConfigParser()
        cfg.read(work / 'profile.ini')
        return cfg['cli-game']

    def settings(client):
        for key in (9, 9, 54):
            press(client, key, key)
        assert inspect(client)['adapter']['inventory_tab'] == 5

    def enter(client, digits=''):
        for digit in digits:
            press(client, ord(digit), ord(digit))
        press(client, 13, 13)

    client = launch(exe, work, game, extra, fixture, False, 0)
    try:
        settings(client)
        shot(client, work).replace(work / 'settings-font1.png')
        press(client, 274)
        press(client, 274)
        enter(client)
        shot(client, work).replace(work / 'password.png')
        enter(client)
        enter(client, '1234')
        enter(client, '4321')
        assert 'nox_dialogue_password' not in profile()
        enter(client)
        enter(client)
        enter(client, '1234')
        shot(client, work).replace(work / 'confirm.png')
        enter(client, '1234')
        assert profile().getint('nox_dialogue_password') == 0x1234
        press(client, 273)
        enter(client)
        enter(client, '0000')
        assert 'nox_mature_dialog' not in profile()
        enter(client)
        enter(client, '1234')
        assert not profile().getboolean('nox_mature_dialog')
        enter(client)
        enter(client, '1234')
        assert profile().getboolean('nox_mature_dialog')
        press(client, 274)
        press(client, 274)
        enter(client)
        assert profile().getboolean('nox_alternate_font')
        shot(client, work).replace(work / 'font-reboot.png')
    finally:
        client.close()

    cold = Client(exe, work, extra)
    try:
        cold.launch(game, 'apple2', game='nox', engine='nox')
        cold.command('wait', field='adapter.stage', equals='title', timeout_ms=10000)
        cold.command('load', chooser_slot=select_save(cold, 0)['chooser_slot'])
        cold.command('wait', field='adapter.stage', equals='world', timeout_ms=5000)
        settings(cold)
        shot(cold, work).replace(work / 'settings-font2.png')
        assert profile().getint('nox_dialogue_password') == 0x1234
        assert (work / 'settings-font1.png').read_bytes() != (work / 'settings-font2.png').read_bytes()
    finally:
        cold.close()
    print('PASS password confirmation/refusal, protected Mature Dialog, and cold font preference')


if __name__ == '__main__':
    main()
